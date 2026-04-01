#ifndef MPI_ENGINE_H
#define MPI_ENGINE_H

#include <vector>
#include "../engine/batch_manager.h"
#include "../common/packet_types.h" // Required for the ProcessingStats return type

// Forward declaration to prevent circular dependencies with mainwindow.h
class MainWindow;

namespace MPIEngine {

    /**
     * @brief Orchestrates the inference process on the Master Node (Rank 0).
     * * @return ProcessingStats: Contains total rows, attacks, and inference timing
     * to be combined with Loading/Preprocessing times in the UI.
     */
    ProcessingStats runMasterInference(BatchManager<std::vector<float>>& clean_queue,
        int world_size,
        int num_features,
        MainWindow* ui);

    /**
     * @brief The core loop for Worker Nodes (Rank > 0).
     * Loads the XGBoost model and waits for data batches from the Master.
     */
    void runWorkerInference(int rank, int num_features);

    /**
     * @brief Sends a termination signal (Tag 99) to all worker nodes.
     * Prevents the application from freezing at the end of execution.
     */
    void broadcastKillSignal(int world_size);
}

#endif // MPI_ENGINE_H