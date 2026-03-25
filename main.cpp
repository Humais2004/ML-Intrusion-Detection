#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <mpi.h>
#include <xgboost/c_api.h>

int main(int argc, char** argv) {
    // ---------------------------------------------------------
    // 1. INITIALIZE MPI ENVIRONMENT
    // ---------------------------------------------------------
    MPI_Init(&argc, &argv);
    
    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int num_features = 39;
    int num_classes = 6;
    std::vector<std::string> labels = {"Benign", "Bot", "Brute Force", "DDoS", "DoS", "Web Attack"};

    // ---------------------------------------------------------
    // 2. LOAD THE XGBOOST MODEL (Every Process gets a brain)
    // ---------------------------------------------------------
    BoosterHandle booster;
    XGBoosterCreate(NULL, 0, &booster);
    if (XGBoosterLoadModel(booster, "xgboost_ids_model2.json") != 0) {
        if (rank == 0) std::cerr << "CRITICAL: Failed to load xgboost_ids_model2.json.\n";
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    // Force XGBoost to use 1 internal thread so it doesn't fight MPI for CPU cores
    XGBoosterSetParam(booster, "nthread", "1");

    int local_rows = 0;
    std::vector<float> local_data;
    
    int total_rows = 0;
    int base_rows_per_worker = 0;
    std::vector<float> global_data;

    // ---------------------------------------------------------
    // 3. MASTER READS THE DATASET FROM DISK
    // ---------------------------------------------------------
    if (rank == 0) {
        std::ifstream file("X_sample.bin", std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "CRITICAL: [MASTER] Could not open X_sample.bin\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        std::streamsize size_in_bytes = file.tellg();
        file.seekg(0, std::ios::beg);
        
        int total_floats = size_in_bytes / sizeof(float);
        total_rows = total_floats / num_features;
        
        global_data.resize(total_floats);
        file.read(reinterpret_cast<char*>(global_data.data()), size_in_bytes);
        file.close();

        // Calculate division of labor
        base_rows_per_worker = total_rows / world_size;
        int remainder = total_rows % world_size;
        
        // Master keeps the base amount + any leftover rows
        local_rows = base_rows_per_worker + remainder;
        local_data.assign(global_data.begin(), global_data.begin() + (local_rows * num_features));
    }

    // =========================================================
    // ⏱️ SYNCHRONIZE AND START THE STOPWATCH
    // =========================================================
    MPI_Barrier(MPI_COMM_WORLD); 
    double start_time = 0.0;
    if (rank == 0) {
        start_time = MPI_Wtime(); 
        std::cout << "[MASTER] Executing Parallel Distribution across " << world_size << " nodes...\n";
    }

    // ---------------------------------------------------------
    // 4. DISTRIBUTE DATA (MPI Scatter logic)
    // ---------------------------------------------------------
    if (rank == 0) {
        int offset = local_rows;
        for (int i = 1; i < world_size; i++) {
            MPI_Send(&base_rows_per_worker, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&global_data[offset * num_features], base_rows_per_worker * num_features, MPI_FLOAT, i, 1, MPI_COMM_WORLD);
            offset += base_rows_per_worker;
        }
    } else {
        MPI_Recv(&local_rows, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        local_data.resize(local_rows * num_features);
        MPI_Recv(local_data.data(), local_rows * num_features, MPI_FLOAT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    // ---------------------------------------------------------
    // 5. PARALLEL MULTI-CLASS INFERENCE
    // ---------------------------------------------------------
    DMatrixHandle dmatrix;
    XGDMatrixCreateFromMat(local_data.data(), local_rows, num_features, -1.0f, &dmatrix);

    bst_ulong out_len;
    const float* out_result;
    XGBoosterPredict(booster, dmatrix, 0, 0, 0, &out_len, &out_result);

    // ArgMax: Convert probability arrays to a single Class ID integer
    std::vector<int> local_predictions(local_rows);
    for (int i = 0; i < local_rows; i++) {
        int start_idx = i * num_classes;
        int best_class = 0;
        float max_prob = out_result[start_idx];
        
        for (int c = 1; c < num_classes; c++) {
            if (out_result[start_idx + c] > max_prob) {
                max_prob = out_result[start_idx + c];
                best_class = c;
            }
        }
        local_predictions[i] = best_class; 
    }

    // ---------------------------------------------------------
    // 6. GATHER RESULTS AND STOP TIMER
    // ---------------------------------------------------------
    if (rank == 0) {
        std::vector<int> global_predictions(total_rows);
        
        // Master copies its own predictions
        std::copy(local_predictions.begin(), local_predictions.end(), global_predictions.begin());
        int offset = local_rows;

        // Catch predictions from all workers
        for (int i = 1; i < world_size; i++) {
            MPI_Recv(&global_predictions[offset], base_rows_per_worker, MPI_INT, i, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            offset += base_rows_per_worker;
        }

        double end_time = MPI_Wtime();
        double total_time = end_time - start_time;

        // ---------------------------------------------------------
        // 7. PRINT FINAL METRICS
        // ---------------------------------------------------------
        std::cout << "\n==================================================================\n";
        std::cout << "🚀 DISTRIBUTED NIDS PERFORMANCE METRICS\n";
        std::cout << "==================================================================\n";
        std::cout << "Total Packets Processed : " << total_rows << "\n";
        std::cout << "Hardware Cores Utilized : " << world_size << "\n";
        std::cout << "Total Execution Time    : " << total_time << " seconds\n";
        std::cout << "Throughput              : " << (total_rows / total_time) << " packets per second\n";
        std::cout << "==================================================================\n";
        
        // Print a tiny sanity-check sample
        std::cout << "\nSample Packets from Dataset:\n";
        for (int i = 0; i < std::min(5, total_rows); i++) {
            std::cout << "Packet " << i << " -> Classified as: " << labels[global_predictions[i]] << "\n";
        }
        
    } else {
        // Workers throw their processed integer arrays back to Master
        MPI_Send(local_predictions.data(), local_rows, MPI_INT, 0, 2, MPI_COMM_WORLD);
    }

    // ---------------------------------------------------------
    // 8. CLEANUP
    // ---------------------------------------------------------
    XGDMatrixFree(dmatrix);
    XGBoosterFree(booster);
    MPI_Finalize();
    return 0;
}