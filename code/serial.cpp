#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono> // Standard high-res timer
#include <xgboost/c_api.h>

int main() {
    int num_features = 39;
    int num_classes = 6;
    std::vector<std::string> labels = {"Benign", "Bot", "Brute Force", "DDoS", "DoS", "Web Attack"};

    // 1. Load Model
    BoosterHandle booster;
    XGBoosterCreate(NULL, 0, &booster);
    if (XGBoosterLoadModel(booster, "xgboost_ids_model2.json") != 0) {
        std::cerr << "CRITICAL: Could not load model file.\n";
        return 1;
    }

    // 2. DISABLE MULTITHREADING (Fair Baseline)
    XGBoosterSetParam(booster, "nthread", "1");

    // 3. Read Binary Data
    std::ifstream file("X_sample.bin", std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "CRITICAL: Could not open X_sample.bin\n";
        return 1;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    int total_floats = size / sizeof(float);
    int total_rows = total_floats / num_features;

    std::vector<float> data(total_floats);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();

    // 4. Start Stopwatch
    std::cout << "Starting SERIAL inference for " << total_rows << " packets...\n";
    auto start = std::chrono::high_resolution_clock::now();

    // 5. Predict
    DMatrixHandle dmatrix;
    XGDMatrixCreateFromMat(data.data(), total_rows, num_features, -1.0f, &dmatrix);
    bst_ulong out_len;
    const float* out_result;
    XGBoosterPredict(booster, dmatrix, 0, 0, 0, &out_len, &out_result);

    // 6. ArgMax Loop
    std::vector<int> predictions(total_rows);
    for (int i = 0; i < total_rows; i++) {
        int start_idx = i * num_classes;
        int best_class = 0;
        float max_prob = out_result[start_idx];
        for (int c = 1; c < num_classes; c++) {
            if (out_result[start_idx + c] > max_prob) {
                max_prob = out_result[start_idx + c];
                best_class = c;
            }
        }
        predictions[i] = best_class;
    }

    // 7. Stop Stopwatch
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "\n--- SERIAL RESULTS ---\n";
    std::cout << "Time Taken: " << diff.count() << " seconds\n";
    std::cout << "Throughput: " << (total_rows / diff.count()) << " packets/sec\n";

    XGDMatrixFree(dmatrix);
    XGBoosterFree(booster);
    return 0;
}
