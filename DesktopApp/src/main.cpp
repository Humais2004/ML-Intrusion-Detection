#include <mpi.h>
#include <QApplication>
#include <iostream>

#include "gui/mainwindow.h"
#include "parallel/mpi_engine.h"

int main(int argc, char** argv) {
    // 1. Initialize MPI
    // We use MPI_Init_thread because Qt and our DataLoader use threads.
    int provided_thread_level;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided_thread_level);

    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int exit_code = 0;

    if (rank == 0) {
        // --- MASTER NODE (UI & Orchestration) ---
        std::cout << "[MASTER] Starting NIDS Application. Total MPI Nodes: " << world_size << std::endl;

        QApplication app(argc, argv);

        // Pass world_size to the GUI so it knows how to divide the batch sizes
        MainWindow window(world_size);
        window.show();

        // The app pauses here and waits for the user to click buttons.
        exit_code = app.exec();

        // Once the user closes the window, tell the workers to shut down.
        std::cout << "[MASTER] GUI closed. Broadcasting kill signal to workers..." << std::endl;
        MPIEngine::broadcastKillSignal(world_size);
    }
    else {
        // --- WORKER NODES (Headless Math Engines) ---
        std::cout << "[WORKER " << rank << "] Online and waiting for instructions." << std::endl;

        // Workers sit in an infinite loop inside this function until they receive a kill signal
        MPIEngine::runWorkerInference(rank, 39);

    }

    // 3. Clean up and Exit
    MPI_Finalize();
    return exit_code;
}