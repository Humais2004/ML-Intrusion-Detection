#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <string>
#include <vector>
#include "batch_manager.h"

class DataLoader {
public:
    /**
     * @brief Streams a raw binary file into the processing pipeline.
     * @param active_time Reference to capture the exact duration of the disk read.
     */
    static void streamBinaryFile(std::string path,
        int batchSize,
        int numFeatures,
        BatchManager<std::vector<float>>& queue,
        double& active_time);
};

#endif // DATA_LOADER_H