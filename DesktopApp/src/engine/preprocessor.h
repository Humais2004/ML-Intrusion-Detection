#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <vector>
#include "batch_manager.h"

// UPDATED: Added active_time to capture the internal processing duration
void runPreprocessor(BatchManager<std::vector<float>>& raw_queue,
    BatchManager<std::vector<float>>& clean_queue,
    double& active_time);

#endif // PREPROCESSOR_H