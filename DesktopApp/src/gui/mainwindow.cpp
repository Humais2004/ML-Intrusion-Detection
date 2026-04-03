#include "mainwindow.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <thread>
#include <mpi.h> 
#include "../engine/data_loader.h"
#include "../engine/preprocessor.h" 
#include "../parallel/mpi_engine.h"

MainWindow::MainWindow(int mpi_size, QWidget* parent)
    : QMainWindow(parent),
    m_mpi_size(mpi_size),
    m_hasAlerted(false),
    statusLabel(nullptr)
{
    // Register custom struct for Signal/Slot safety
    qRegisterMetaType<ProcessingStats>("ProcessingStats");

    // UI Setup
    auto* centralWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(centralWidget);

    pathInput = new QLineEdit(this);
    auto* btnBrowse = new QPushButton("Browse .bin File", this);
    batchInput = new QLineEdit("100000", this);
    runButton = new QPushButton("RUN ENGINE", this);
    logWindow = new QTextEdit(this);
    logWindow->setReadOnly(true);

    layout->addWidget(new QLabel("Dataset Path:"));
    layout->addWidget(pathInput);
    layout->addWidget(btnBrowse);
    layout->addWidget(new QLabel("Batch Size:"));
    layout->addWidget(batchInput);
    layout->addWidget(runButton);
    layout->addWidget(new QLabel("Execution Log:"));
    layout->addWidget(logWindow);

    setCentralWidget(centralWidget);
    setWindowTitle("Parallel NIDS Engine - CUI Lahore");
    resize(650, 550);

    // Connect local signals
    connect(btnBrowse, &QPushButton::clicked, this, &MainWindow::onSelectFile);
    connect(runButton, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(this, &MainWindow::logMessage, this, &MainWindow::updateLog);
    connect(this, &MainWindow::intrusionAlert, this, &MainWindow::showAlert);
}

void MainWindow::onSelectFile() {
    QString file = QFileDialog::getOpenFileName(this, "Select Binary Dataset", "", "Binary Files (*.bin)");
    if (!file.isEmpty()) pathInput->setText(file);
}

void MainWindow::onRunClicked() {
    std::string path = pathInput->text().toStdString();
    int batchSize = batchInput->text().toInt();

    m_hasAlerted = false; // Reset alert state
    runButton->setEnabled(false);
    logWindow->clear();
    updateLog("🚀 Initializing Parallel Pipeline with " + QString::number(m_mpi_size) + " nodes...");

    std::thread controller([this, path, batchSize]() {
        BatchManager<std::vector<float>> raw_queue;
        BatchManager<std::vector<float>> clean_queue;

        // Local timing variables to be updated by threads
        double loadActiveT = 0.0;
        double prepActiveT = 0.0;
        double global_start = MPI_Wtime();

        // 1. Start Loader Thread (Passes timing by reference)
        std::thread loaderThread(DataLoader::streamBinaryFile, path, batchSize, 80, std::ref(raw_queue), std::ref(loadActiveT));

        // 2. Start Preprocessor Thread (Passes timing by reference)
        std::thread preprocessorThread(runPreprocessor, std::ref(raw_queue), std::ref(clean_queue), std::ref(prepActiveT));

        // 3. Run MPI Master Inference
        ProcessingStats stats = MPIEngine::runMasterInference(clean_queue, m_mpi_size, 39, this);

        // --- REMOVED: Workers are no longer killed here ---
        // Workers will stay alive waiting for the next job

        // 4. Join Local Threads
        if (loaderThread.joinable()) loaderThread.join();
        if (preprocessorThread.joinable()) preprocessorThread.join();

        double global_end = MPI_Wtime();

        // Finalize Stat Assembly
        stats.load_time = loadActiveT;
        stats.preprocess_time = prepActiveT;
        stats.total_time = global_end - global_start;
        stats.rows_per_second = stats.total_rows / (stats.total_time > 0 ? stats.total_time : 1);

        // Send to GUI
        QMetaObject::invokeMethod(this, "showFinalStats", Q_ARG(ProcessingStats, stats));
        });

    controller.detach();
}

void MainWindow::updateLog(const QString& msg) {
    logWindow->append(msg);
}

void MainWindow::showAlert(const QString& type) {
    // Throttled: Only show red text once per run to prevent UI freezing
    if (!m_hasAlerted) {
        logWindow->append("<font color='red'><b>[ALERT] Intrusion Detected! Monitoring active...</b></font>");
        m_hasAlerted = true;
    }
}

void MainWindow::showFinalStats(ProcessingStats stats) {
    QString report = QString(
        "\n====================================\n"
        "🟢 FINAL PROCESSING REPORT\n"
        "====================================\n"
        "Total Rows Processed: %1\n"
        "Total Attacks Detected: %2\n"
        "------------------------------------\n"
        "⏱️ PHASE TIMINGS (Active Work)\n"
        "Data Loading:         %3s\n"
        "Preprocessing:        %4s (OpenMP)\n"
        "Inference:            %5s (MPI)\n"
        "------------------------------------\n"
        "🚀 PERFORMANCE\n"
        "Total Execution Time: %6s\n"
        "Processing Speed:     %7 rows/sec\n"
        "====================================")
        .arg(stats.total_rows)
        .arg(stats.total_attacks)
        .arg(stats.load_time, 0, 'f', 3)
        .arg(stats.preprocess_time, 0, 'f', 3)
        .arg(stats.inference_time, 0, 'f', 3)
        .arg(stats.total_time, 0, 'f', 3)
        .arg(QString::number(stats.rows_per_second, 'f', 2));

    logWindow->append(report);
    runButton->setEnabled(true);
}

MainWindow::~MainWindow() {
    // When the user clicks the "X" on the window, 
    // this sends the final Kill Tag (99) to workers.
    MPIEngine::broadcastKillSignal(m_mpi_size);

    // Give them a tiny moment to finish their cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}