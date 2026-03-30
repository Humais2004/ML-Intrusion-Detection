#include <mpi.h>
#include <QApplication>
#include <iostream>

// Include your custom engine headers
#include "gui/mainwindow.h"
#include "parallel/mpi_engine.h"

int main(int argc, char** argv) {
    // 1. Initialize MPI with Thread Support
    // Since we are using Qt threads and background C++ threads, 
    // we need MPI to know this is a multi-threaded environment.
    int provided_thread_level;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided_thread_level);

    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int exit_code = 0;

    // ---------------------------------------------------------
    // THE SPLIT: Master vs. Workers
    // ---------------------------------------------------------
    if (rank == 0) {
        // --- MASTER NODE (Rank 0) ---
        // The Master is the ONLY node that gets a Graphical User Interface.
        std::cout << "[MASTER] Initializing NIDS GUI. Total Cores: " << world_size << "\n";

        // Initialize Qt6 Application
        QApplication app(argc, argv);

        // Create the Main Window and pass the world_size so the GUI 
        // knows how many workers it has for the statistics screen.
        MainWindow window(world_size);
        window.show();

        // Start the GUI Event Loop. 
        // The program will pause here until the user clicks the "X" to close the window.
        exit_code = app.exec();

        // Once the GUI is closed, we must gracefully shut down the worker ranks.
        // We broadcast a kill signal (-1) so they break out of their infinite loops.
        MPIEngine::broadcastKillSignal(world_size);
        std::cout << "[MASTER] GUI Closed. Shutting down system.\n";

    }
    else {
        // --- WORKER NODES (Rank > 0) ---
        // Workers are "Headless". They never see the Qt GUI.
        // They immediately jump into an infinite loop waiting for data from the Master.

        int num_features = 39; // The number of columns in your dataset
        MPIEngine::runWorkerInference(rank, num_features);
    }

    // 3. Clean up and Exit
    MPI_Finalize();
    return exit_code;
}