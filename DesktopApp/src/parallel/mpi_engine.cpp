#include "mpi_engine.h"
#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <xgboost/c_api.h>
#include "../gui/mainwindow.h"

// MPI Tags for protocol communication
#define TAG_DATA    1
#define TAG_RESULT  2
#define TAG_LOG     3
#define TAG_END_JOB 4  // <--- New: Tells worker to reset for next file
#define TAG_KILL    99 // <--- Final: Tells worker to exit completely

// ========================================================================
// XGBOOST ERROR TRAPPER
// ========================================================================
#define SAFE_XGB(call, rank) { \
    int err = (call); \
    if (err != 0) { \
        std::string err_msg = "XGBoost Error: " + std::string(XGBGetLastError()); \
        MPIEngine::sendLogToUI(err_msg, rank); \
        MPI_Abort(MPI_COMM_WORLD, 1); \
    } \
}

namespace MPIEngine {

    // Helper: Sends a string message to Rank 0 to be displayed on the GUI
    void sendLogToUI(const std::string& msg, int rank) {
        std::string formatted = "[WORKER " + std::to_string(rank) + "] " + msg;
        int len = static_cast<int>(formatted.length()) + 1;
        MPI_Send(formatted.c_str(), len, MPI_CHAR, 0, TAG_LOG, MPI_COMM_WORLD);
    }

    // Helper: Master checks for any pending log messages from workers
    void collectRemoteLogs(MainWindow* ui) {
        int hasLog = 0;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_LOG, MPI_COMM_WORLD, &hasLog, &status);

        while (hasLog) {
            int msgLen;
            MPI_Get_count(&status, MPI_CHAR, &msgLen);
            std::vector<char> buffer(msgLen);
            MPI_Recv(buffer.data(), msgLen, MPI_CHAR, status.MPI_SOURCE, TAG_LOG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (ui) {
                QMetaObject::invokeMethod(ui, "logMessage", Q_ARG(QString, QString::fromUtf8(buffer.data())));
            }
            MPI_Iprobe(MPI_ANY_SOURCE, TAG_LOG, MPI_COMM_WORLD, &hasLog, &status);
        }
    }

    // ========================================================================
    // THE OPTIMIZED MASTER NODE (Asynchronous Orchestrator)
    // ========================================================================
    ProcessingStats runMasterInference(BatchManager<std::vector<float>>& clean_queue, int world_size, int num_features, MainWindow* ui) {
        ProcessingStats stats;
        double start_time = MPI_Wtime();

        // Track state for each worker
        struct WorkerState {
            bool busy = false;
            int rows_sent = 0;
            std::vector<float> pred_buffer;
            MPI_Request send_req = MPI_REQUEST_NULL;
            MPI_Request recv_req = MPI_REQUEST_NULL;
        };

        std::vector<WorkerState> workers(world_size);
        std::vector<std::vector<float>> data_buffers(world_size); // Persistent buffers for Isend

        bool queue_active = true;
        int active_workers_count = 0;

        while (queue_active || active_workers_count > 0) {
            // 1. Collect any logs waiting in the MPI buffer
            collectRemoteLogs(ui);

            // 2. Check for finished workers and process results
            for (int r = 1; r < world_size; ++r) {
                if (workers[r].busy) {
                    int finished = 0;
                    MPI_Test(&workers[r].recv_req, &finished, MPI_STATUS_IGNORE);

                    if (finished) {
                        bool batch_has_attack = false;
                        for (float p : workers[r].pred_buffer) {
                            stats.total_rows++;
                            if (p > 0.5f) {
                                stats.total_attacks++;
                                batch_has_attack = true;
                            }
                        }

                        if (batch_has_attack && ui) {
                            QMetaObject::invokeMethod(ui, "intrusionAlert", Q_ARG(QString, "Malicious Traffic Detected!"));
                        }

                        workers[r].busy = false;
                        active_workers_count--;
                    }
                }

                // 3. If worker is free, give it a new batch
                if (!workers[r].busy && queue_active) {
                    if (clean_queue.consume(data_buffers[r])) {
                        int n_rows = data_buffers[r].size() / num_features;
                        workers[r].rows_sent = n_rows;
                        workers[r].pred_buffer.resize(n_rows);
                        workers[r].busy = true;
                        active_workers_count++;

                        // Non-blocking Send and Receive
                        MPI_Isend(data_buffers[r].data(), data_buffers[r].size(), MPI_FLOAT, r, TAG_DATA, MPI_COMM_WORLD, &workers[r].send_req);
                        MPI_Irecv(workers[r].pred_buffer.data(), n_rows, MPI_FLOAT, r, TAG_RESULT, MPI_COMM_WORLD, &workers[r].recv_req);
                    }
                    else {
                        queue_active = false;
                    }
                }
            }

            // Minimal sleep to prevent CPU spinning while waiting for I/O
            if (active_workers_count > 0) std::this_thread::yield();
        }

        // AFTER THE LOOP FINISHES: Send End Job signal instead of Kill
        int dummy = 0;
        for (int i = 1; i < world_size; ++i) {
            MPI_Send(&dummy, 1, MPI_INT, i, TAG_END_JOB, MPI_COMM_WORLD);
        }

        stats.inference_time = MPI_Wtime() - start_time;
        return stats;
    }

    // ========================================================================
    // THE WORKER NODES (Inference Engines)
    // ========================================================================
    void runWorkerInference(int rank, int num_features) {
        BoosterHandle booster;
        SAFE_XGB(XGBoosterCreate(NULL, 0, &booster), rank);

        const char* model_path = "D:/ML-Intrusion-Detection/DesktopApp/src/xgboost_ids_model2.json";
        if (XGBoosterLoadModel(booster, model_path) != 0) {
            sendLogToUI("CRITICAL: Model load failed!", rank);
            return;
        }

        SAFE_XGB(XGBoosterSetParam(booster, "nthread", "1"), rank);
        sendLogToUI("Engine online and model loaded.", rank);

        bool app_alive = true;
        while (app_alive) {
            sendLogToUI("Waiting for next job...", rank);

            bool job_active = true;
            while (job_active) {
                MPI_Status status;
                MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                if (status.MPI_TAG == TAG_KILL) {
                    int dummy;
                    MPI_Recv(&dummy, 1, MPI_INT, 0, TAG_KILL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    job_active = false;
                    app_alive = false;
                    break;
                }

                if (status.MPI_TAG == TAG_END_JOB) {
                    // Receive the dummy message to clear the buffer
                    int dummy;
                    MPI_Recv(&dummy, 1, MPI_INT, 0, TAG_END_JOB, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    sendLogToUI("Job finished. Resetting for next run.", rank);
                    job_active = false; // Break inner loop to wait for new data
                    break;
                }

                if (status.MPI_TAG == TAG_DATA) {
                    int msg_size = 0;
                    MPI_Get_count(&status, MPI_FLOAT, &msg_size);
                    int rows_to_process = msg_size / num_features;

                    std::vector<float> features(msg_size);
                    MPI_Recv(features.data(), msg_size, MPI_FLOAT, 0, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    DMatrixHandle dmatrix;
                    SAFE_XGB(XGDMatrixCreateFromMat(features.data(), rows_to_process, num_features, -1.0f, &dmatrix), rank);

                    bst_ulong out_len;
                    const float* out_result = nullptr;
                    SAFE_XGB(XGBoosterPredict(booster, dmatrix, 0, 0, 0, &out_len, &out_result), rank);

                    // Argmax for 6 classes
                    const int NUM_CLASSES = 6;
                    std::vector<float> winning_classes(rows_to_process);
                    for (int i = 0; i < rows_to_process; ++i) {
                        int best_class = 0;
                        float max_prob = -1.0f;
                        for (int c = 0; c < NUM_CLASSES; ++c) {
                            float p = out_result[i * NUM_CLASSES + c];
                            if (p > max_prob) { max_prob = p; best_class = c; }
                        }
                        winning_classes[i] = static_cast<float>(best_class);
                    }

                    // Send results back
                    MPI_Send(winning_classes.data(), winning_classes.size(), MPI_FLOAT, 0, TAG_RESULT, MPI_COMM_WORLD);

                    SAFE_XGB(XGDMatrixFree(dmatrix), rank);
                }
            }
        }

        SAFE_XGB(XGBoosterFree(booster), rank);
        sendLogToUI("Shutting down gracefully.", rank);
    }

    void broadcastKillSignal(int world_size) {
        int dummy = 0;
        for (int i = 1; i < world_size; ++i) {
            MPI_Send(&dummy, 1, MPI_INT, i, TAG_KILL, MPI_COMM_WORLD);
        }
    }
}