#include "mpi_engine.h"
#include <mpi.h>
#include <iostream>
#include <vector>
#include <xgboost/c_api.h>
#include "../gui/mainwindow.h"

// ========================================================================
// XGBOOST ERROR TRAPPER
// ========================================================================
#define SAFE_XGB(call, rank) { \
    int err = (call); \
    if (err != 0) { \
        std::cerr << "\n[FATAL XGBOOST ERROR Worker " << rank << "] " << XGBGetLastError() << std::endl; \
        MPI_Abort(MPI_COMM_WORLD, 1); \
    } \
}

namespace MPIEngine {

    // ========================================================================
    // THE MASTER NODE (Orchestrator)
    // ========================================================================
    ProcessingStats runMasterInference(BatchManager<std::vector<float>>& clean_queue, int world_size, int num_features, MainWindow* ui) {
        std::vector<float> batch;
        int active_worker = 1;

        long total_rows_processed = 0;
        long total_attacks_detected = 0;

        // Start timing the inference phase specifically
        double start_time = MPI_Wtime();

        while (clean_queue.consume(batch)) {
            int rows_in_batch = batch.size() / num_features;

            // 1. Send data to worker (Tag 1)
            MPI_Send(&rows_in_batch, 1, MPI_INT, active_worker, 1, MPI_COMM_WORLD);
            MPI_Send(batch.data(), batch.size(), MPI_FLOAT, active_worker, 1, MPI_COMM_WORLD);

            // 2. Receive exactly 1 float per row back from worker (The Winning Class ID)
            std::vector<float> predictions(rows_in_batch);
            MPI_Recv(predictions.data(), rows_in_batch, MPI_FLOAT, active_worker, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // 3. Process results and throttle UI alerts
            bool batch_has_attack = false;
            for (int i = 0; i < rows_in_batch; ++i) {
                total_rows_processed++;
                // Class 0 is Benign. Anything > 0.5 is an attack.
                if (predictions[i] > 0.5f) {
                    total_attacks_detected++;
                    batch_has_attack = true;
                }
            }

            // UI PROTECTION: Only send 1 alert per batch to prevent Qt from freezing
            if (batch_has_attack && ui) {
                QMetaObject::invokeMethod(ui, "intrusionAlert", Q_ARG(QString, "Malicious Traffic Detected!"));
            }

            // Round-robin load balancing
            active_worker++;
            if (active_worker >= world_size) active_worker = 1;
        }

        double end_time = MPI_Wtime();

        // Package stats for the GUI Controller
        ProcessingStats stats;
        stats.total_rows = total_rows_processed;
        stats.total_attacks = total_attacks_detected;
        stats.inference_time = end_time - start_time;

        std::cout << "[MASTER] Inference complete. Total Attacks Found: " << total_attacks_detected << std::endl;

        return stats;
    }

    // ========================================================================
    // THE WORKER NODES (Inference Engines)
    // ========================================================================
    void runWorkerInference(int rank, int num_features) {
        BoosterHandle booster;
        SAFE_XGB(XGBoosterCreate(NULL, 0, &booster), rank);

        // Path to your JSON model
        const char* model_path = "D:/ML-Intrusion-Detection/DesktopApp/src/xgboost_ids_model2.json";

        if (XGBoosterLoadModel(booster, model_path) != 0) {
            std::cerr << "[WORKER " << rank << "] FATAL ERROR: Could not find model at " << model_path << std::endl;
            return;
        }

        // Set to 1 thread per worker to avoid CPU thrashing with MPI
        SAFE_XGB(XGBoosterSetParam(booster, "nthread", "1"), rank);

        std::cout << "[WORKER " << rank << "] Model loaded. Awaiting data..." << std::endl;

        while (true) {
            int rows_to_process = 0;
            MPI_Status status;

            // Wait for metadata from Master
            MPI_Recv(&rows_to_process, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            // Check for the Kill Signal (Tag 99)
            if (status.MPI_TAG == 99) {
                break;
            }

            // Receive the feature batch (39 columns per row)
            std::vector<float> features(rows_to_process * num_features);
            MPI_Recv(features.data(), features.size(), MPI_FLOAT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Create XGBoost Matrix
            DMatrixHandle dmatrix;
            SAFE_XGB(XGDMatrixCreateFromMat(features.data(), rows_to_process, num_features, -1.0f, &dmatrix), rank);

            // Run Prediction
            bst_ulong out_len;
            const float* out_result = nullptr;
            SAFE_XGB(XGBoosterPredict(booster, dmatrix, 0, 0, 0, &out_len, &out_result), rank);

            // Multiclass Argmax Logic (Assuming 6 classes based on your output data)
            const int NUM_CLASSES = 6;
            std::vector<float> winning_classes(rows_to_process);
            int attacks_in_batch = 0;

            for (int i = 0; i < rows_to_process; ++i) {
                int best_class = 0;
                float max_prob = -1.0f;

                for (int c = 0; c < NUM_CLASSES; ++c) {
                    float current_prob = out_result[i * NUM_CLASSES + c];
                    if (current_prob > max_prob) {
                        max_prob = current_prob;
                        best_class = c;
                    }
                }

                winning_classes[i] = static_cast<float>(best_class);
                if (best_class > 0) attacks_in_batch++;
            }

            std::cout << "[WORKER " << rank << "] Processed " << rows_to_process
                << " rows. Detected " << attacks_in_batch << " attacks." << std::endl;

            // Send winning IDs back to Master (Tag 2)
            MPI_Send(winning_classes.data(), winning_classes.size(), MPI_FLOAT, 0, 2, MPI_COMM_WORLD);

            // Cleanup DMatrix for the next batch
            SAFE_XGB(XGDMatrixFree(dmatrix), rank);
        }

        // Cleanup Booster
        SAFE_XGB(XGBoosterFree(booster), rank);

        // Final unique shutdown message
        std::cout << "[WORKER " << rank << "] Shutting down gracefully." << std::endl;
    }

    // ========================================================================
    // THE SHUTDOWN SIGNAL
    // ========================================================================
    void broadcastKillSignal(int world_size) {
        int dummy = -1;
        // Notify all workers to exit their loops
        for (int i = 1; i < world_size; ++i) {
            MPI_Send(&dummy, 1, MPI_INT, i, 99, MPI_COMM_WORLD);
        }
    }
}