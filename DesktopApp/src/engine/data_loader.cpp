#include "data_loader.h"
#include <fstream>
#include <iostream>
#include <mpi.h> // <--- CRITICAL: Provides MPI_Wtime() for high-res timing

void DataLoader::streamBinaryFile(std::string path,
    int batchSize,
    int numFeatures,
    BatchManager<std::vector<float>>& queue,
    double& active_time) {

    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "[LOADER ERROR] Could not open file: " << path << std::endl;
        active_time = 0.0;
        queue.setFinished();
        return;
    }

    // --- START ACTIVE TIMER ---
    double start = MPI_Wtime();

    while (true) {
        // Allocate space for the batch
        std::vector<float> batch(batchSize * numFeatures);

        // Read directly into the vector's memory
        file.read(reinterpret_cast<char*>(batch.data()), batch.size() * sizeof(float));

        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) break;

        // If the last batch is smaller than the requested batchSize, resize it
        size_t elementsRead = static_cast<size_t>(bytesRead / sizeof(float));
        if (elementsRead < batch.size()) {
            batch.resize(elementsRead);
        }

        // Push to the Preprocessor's queue
        queue.produce(std::move(batch));
    }

    // --- STOP ACTIVE TIMER ---
    active_time = MPI_Wtime() - start;

    file.close();

    // Signal the Preprocessor that no more raw data is coming
    queue.setFinished();

    std::cout << "[LOADER] EOF reached. Disk streaming took: " << active_time << "s" << std::endl;
}