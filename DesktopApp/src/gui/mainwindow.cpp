#include "mainwindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QScrollArea>
#include <QRegularExpression>
#include <thread>
#include <mpi.h>
#include "../engine/data_loader.h"
#include "../engine/preprocessor.h"
#include "../parallel/mpi_engine.h"

// ============================================================================
// COLOR PALETTE
// ============================================================================
static const char* C_BG          = "#0d1117";
static const char* C_SURFACE     = "#161b22";
static const char* C_SURFACE2    = "#21262d";
static const char* C_BORDER      = "#30363d";
static const char* C_TEXT        = "#e6edf3";
static const char* C_MUTED       = "#8b949e";
static const char* C_CYAN        = "#58d8f7";
static const char* C_GREEN       = "#3fb950";
static const char* C_RED         = "#f85149";
static const char* C_ORANGE      = "#d29922";
static const char* C_CARD_IDLE   = "#161b22";
static const char* C_CARD_LOAD   = "#1c2133";
static const char* C_CARD_ACTIVE = "#0d2137";
static const char* C_CARD_DONE   = "#0d1f16";
static const char* C_CARD_ERROR  = "#2d0d0d";

// ============================================================================
// HELPER
// ============================================================================
static QLabel* makeLabel(const QString& text, const QString& color,
                          int ptSize = 10, bool bold = false)
{
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(
        QString("color:%1; font-size:%2pt; font-weight:%3; background:transparent;")
        .arg(color).arg(ptSize).arg(bold ? "bold" : "normal"));
    return lbl;
}

// ============================================================================
// CONSTRUCTOR
// ============================================================================
MainWindow::MainWindow(int mpi_size, QWidget* parent)
    : QMainWindow(parent),
      m_mpi_size(mpi_size),
      m_batchSize(100000),
      m_hasAlerted(false),
      m_runInProgress(false),
      pathInput(nullptr), batchInput(nullptr),
      runButton(nullptr), browseButton(nullptr),
      workerGridFrame(nullptr),
      phaseLoadIcon(nullptr), phaseLoadLabel(nullptr),
      phasePrepIcon(nullptr), phasePrepLabel(nullptr),
      phaseInfIcon(nullptr),  phaseInfLabel(nullptr),
      rowsCountLabel(nullptr), attacksCountLabel(nullptr), speedLabel(nullptr),
      alertBanner(nullptr), alertLabel(nullptr),
      summaryPanel(nullptr), summaryTitle(nullptr), summaryContent(nullptr)
{
    qRegisterMetaType<ProcessingStats>("ProcessingStats");

    applyDarkTheme();
    buildUI();

    connect(browseButton, &QPushButton::clicked,    this, &MainWindow::onSelectFile);
    connect(runButton,    &QPushButton::clicked,    this, &MainWindow::onRunClicked);
    connect(this, &MainWindow::logMessage,          this, &MainWindow::updateLog);
    connect(this, &MainWindow::intrusionAlert,      this, &MainWindow::showAlert);
    connect(this, &MainWindow::workerStateChanged,  this, &MainWindow::onWorkerStateChanged);
    connect(this, &MainWindow::phaseUpdate,         this, &MainWindow::onPhaseUpdate);
    connect(this, &MainWindow::liveStatsUpdate,     this, &MainWindow::onLiveStatsUpdate);

    setWindowTitle("Parallel NIDS Engine  ·  CUI Lahore");
    setMinimumSize(780, 640);
    resize(960, 740);
}

// ============================================================================
// DARK THEME
// ============================================================================
void MainWindow::applyDarkTheme()
{
    setStyleSheet(QString(R"(
        QMainWindow, QWidget {
            background-color: %1;
            color: %2;
            font-family: 'Segoe UI', 'Inter', sans-serif;
        }
        QLineEdit {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 9pt;
            min-height: 26px;
        }
        QLineEdit:focus { border: 1px solid %5; }
        QPushButton#browseBtn {
            background-color: %3;
            color: %6;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 6px 14px;
            font-size: 9pt;
            font-weight: bold;
            min-height: 28px;
        }
        QPushButton#browseBtn:hover { background-color: %7; border-color: %6; }
        QPushButton#runBtn {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #0e7490, stop:1 #0284c7);
            color: #ffffff;
            border: none;
            border-radius: 8px;
            padding: 10px 28px;
            font-size: 11pt;
            font-weight: bold;
            letter-spacing: 1px;
            min-height: 38px;
        }
        QPushButton#runBtn:hover {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #0891b2, stop:1 #0369a1);
        }
        QPushButton#runBtn:disabled { background: %3; color: %6; }
        QScrollArea  { border: none; background: transparent; }
        QScrollBar:vertical   { background:%3; width:8px;  border-radius:4px; }
        QScrollBar:horizontal { background:%3; height:8px; border-radius:4px; }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background:%4; border-radius:4px; min-height:20px; min-width:20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { height:0; width:0; }
    )")
    .arg(C_BG).arg(C_TEXT).arg(C_SURFACE).arg(C_BORDER)
    .arg(C_CYAN).arg(C_MUTED).arg(C_SURFACE2));
}

// ============================================================================
// BUILD UI
// ============================================================================
void MainWindow::buildUI()
{
    // Outer scroll area — prevents ALL cropping on resize
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* central = new QWidget();
    central->setStyleSheet(QString("background-color:%1;").arg(C_BG));
    scrollArea->setWidget(central);
    setCentralWidget(scrollArea);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // ── Title ──────────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout();
        row->addWidget(makeLabel("⚡  PARALLEL NIDS ENGINE", C_TEXT, 13, true));
        row->addStretch();
        row->addWidget(makeLabel("CUI Lahore  ·  MPI + OpenMP + XGBoost", C_MUTED, 9));
        root->addLayout(row);
    }

    // Separator
    {
        auto* sep = new QFrame();
        sep->setFixedHeight(1);
        sep->setStyleSheet(QString("background-color:%1; border:none;").arg(C_BORDER));
        root->addWidget(sep);
    }

    // ── Alert Banner ─────────────────────────────────────────────────────
    alertBanner = new QFrame();
    alertBanner->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #7f1d1d, stop:1 #450a0a);"
        "border:1px solid #f85149; border-radius:8px;");
    alertBanner->setMinimumHeight(44);
    {
        auto* row = new QHBoxLayout(alertBanner);
        row->setContentsMargins(12, 6, 12, 6);
        alertLabel = makeLabel("🚨  INTRUSION DETECTED — Malicious traffic flagged!", C_RED, 10, true);
        row->addWidget(alertLabel);
    }
    alertBanner->hide();
    root->addWidget(alertBanner);

    // ── Control Panel ────────────────────────────────────────────────────
    buildControlPanel();
    {
        auto* frame = new QFrame();
        frame->setStyleSheet(
            QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
            .arg(C_SURFACE).arg(C_BORDER));
        auto* vl = new QVBoxLayout(frame);
        vl->setContentsMargins(14, 12, 14, 12);
        vl->setSpacing(8);
        vl->addWidget(makeLabel("DATASET CONFIGURATION", C_MUTED, 8, true));

        auto* fileRow = new QHBoxLayout();
        fileRow->setSpacing(8);
        fileRow->addWidget(pathInput, 1);
        fileRow->addWidget(browseButton);
        vl->addLayout(fileRow);

        auto* batchRow = new QHBoxLayout();
        batchRow->setSpacing(8);
        batchRow->addWidget(makeLabel("Batch Size:", C_MUTED, 9));
        batchRow->addWidget(batchInput);
        batchRow->addStretch();
        batchRow->addWidget(runButton);
        vl->addLayout(batchRow);

        root->addWidget(frame);
    }

    // ── Pipeline Phases ──────────────────────────────────────────────────
    buildPhasePanel();
    {
        auto* frame = new QFrame();
        frame->setStyleSheet(
            QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
            .arg(C_SURFACE).arg(C_BORDER));
        frame->setMinimumHeight(105);

        auto* vl = new QVBoxLayout(frame);
        vl->setContentsMargins(14, 10, 14, 10);
        vl->setSpacing(4);
        vl->addWidget(makeLabel("PIPELINE PHASES", C_MUTED, 8, true));

        auto* phaseRow = new QHBoxLayout();
        phaseRow->setSpacing(0);

        // Helper to build one phase column
        auto makePhase = [&](QLabel*& iconOut, QLabel*& lblOut,
                              const QString& iconText, const QString& labelText,
                              int minW) {
            auto* col = new QVBoxLayout();
            col->setSpacing(2);
            col->setAlignment(Qt::AlignCenter);
            iconOut = makeLabel(iconText, C_MUTED, 16);
            iconOut->setAlignment(Qt::AlignCenter);
            iconOut->setMinimumHeight(28);
            lblOut  = makeLabel(labelText, C_MUTED, 8);
            lblOut->setAlignment(Qt::AlignCenter);
            lblOut->setWordWrap(true);
            lblOut->setMinimumWidth(minW);
            col->addWidget(iconOut);
            col->addWidget(lblOut);
            return col;
        };

        auto arr = [&](){ auto* a = makeLabel("→", C_BORDER, 16, true);
                          a->setAlignment(Qt::AlignCenter);
                          a->setFixedWidth(32);
                          return a; };

        phaseRow->addStretch(1);
        phaseRow->addLayout(makePhase(phaseLoadIcon, phaseLoadLabel,
                                      "⬜", "Data Loading\nIdle", 85));
        phaseRow->addWidget(arr());
        phaseRow->addLayout(makePhase(phasePrepIcon, phasePrepLabel,
                                      "⬜", "Preprocessing\nIdle", 90));
        phaseRow->addWidget(arr());
        phaseRow->addLayout(makePhase(phaseInfIcon, phaseInfLabel,
                                      "⬜", "MPI Inference\nIdle", 90));
        phaseRow->addStretch(1);

        vl->addLayout(phaseRow);
        root->addWidget(frame);
    }

    // ── MPI Worker Cards ─────────────────────────────────────────────────
    buildWorkerGrid();
    root->addWidget(workerGridFrame);

    // ── Live Stats ───────────────────────────────────────────────────────
    buildStatsPanel();
    {
        auto* frame = new QFrame();
        frame->setStyleSheet(
            QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
            .arg(C_SURFACE).arg(C_BORDER));
        frame->setMinimumHeight(90);

        auto* hl = new QHBoxLayout(frame);
        hl->setContentsMargins(20, 8, 20, 8);
        hl->setSpacing(0);

        auto addStat = [&](const QString& icon, QLabel* count, const QString& name) {
            auto* col = new QVBoxLayout();
            col->setSpacing(1);
            col->setAlignment(Qt::AlignCenter);
            auto* ic = makeLabel(icon, C_TEXT, 14);
            ic->setAlignment(Qt::AlignCenter);
            count->setAlignment(Qt::AlignCenter);
            auto* nm = makeLabel(name, C_MUTED, 8);
            nm->setAlignment(Qt::AlignCenter);
            col->addWidget(ic);
            col->addWidget(count);
            col->addWidget(nm);
            hl->addLayout(col);
        };

        addStat("📦", rowsCountLabel,    "Rows Processed");
        hl->addStretch();
        addStat("⚠️",  attacksCountLabel, "Attacks Detected");
        hl->addStretch();
        addStat("⚡",  speedLabel,        "Rows / Second");

        root->addWidget(frame);
    }

    // ── Final Report Panel ───────────────────────────────────────────────
    summaryPanel = new QFrame();
    summaryPanel->setStyleSheet(
        QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
        .arg(C_SURFACE).arg(C_BORDER));
    {
        auto* vl = new QVBoxLayout(summaryPanel);
        vl->setContentsMargins(16, 14, 16, 14);
        vl->setSpacing(8);

        summaryTitle = makeLabel("📊  FINAL PROCESSING REPORT", C_GREEN, 10, true);
        summaryContent = new QLabel();
        summaryContent->setStyleSheet(
            QString("color:%1; font-size:9pt; background:transparent;").arg(C_TEXT));
        summaryContent->setWordWrap(true);
        summaryContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        vl->addWidget(summaryTitle);

        // Thin divider inside summary
        auto* div = new QFrame();
        div->setFixedHeight(1);
        div->setStyleSheet(
            QString("background-color:%1; border:none; margin:2px 0;").arg(C_BORDER));
        vl->addWidget(div);
        vl->addWidget(summaryContent);
    }
    summaryPanel->hide();
    root->addWidget(summaryPanel);
    // No addStretch — scroll area handles overflow
}

// ============================================================================
// CONTROL PANEL WIDGETS
// ============================================================================
void MainWindow::buildControlPanel()
{
    pathInput = new QLineEdit();
    pathInput->setPlaceholderText("Select a .bin dataset file...");

    browseButton = new QPushButton("Browse");
    browseButton->setObjectName("browseBtn");
    browseButton->setFixedWidth(80);

    batchInput = new QLineEdit("100000");
    batchInput->setFixedWidth(100);

    runButton = new QPushButton("▶  RUN ENGINE");
    runButton->setObjectName("runBtn");
}

// ============================================================================
// WORKER GRID
// Each card: 110×120px — icon, rank, status, stat line
// Cards sit in a horizontally-scrollable strip
// ============================================================================
void MainWindow::buildWorkerGrid()
{
    workerGridFrame = new QFrame();
    workerGridFrame->setStyleSheet(
        QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
        .arg(C_SURFACE).arg(C_BORDER));

    auto* outer = new QVBoxLayout(workerGridFrame);
    outer->setContentsMargins(14, 10, 14, 10);
    outer->setSpacing(6);

    // Header
    {
        auto* hdr = new QHBoxLayout();
        hdr->addWidget(makeLabel("MPI WORKER THREADS", C_MUTED, 8, true));
        hdr->addStretch();
        hdr->addWidget(makeLabel(
            QString("World Size: %1  (Master + %2 Workers)")
            .arg(m_mpi_size).arg(m_mpi_size - 1), C_MUTED, 8));
        outer->addLayout(hdr);
    }

    // Horizontal scroll strip
    auto* scroll = new QScrollArea();
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(132);   // 110px card + 8px margin top/bottom + scrollbar
    scroll->setWidgetResizable(false);
    scroll->setStyleSheet("background:transparent;");

    auto* strip = new QWidget();
    strip->setStyleSheet("background:transparent;");
    auto* hl = new QHBoxLayout(strip);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(10);
    hl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // ── Master card ────────────────────────────────
    {
        auto* card = new QFrame();
        card->setStyleSheet(
            QString("QFrame{background:#1c2133;border:2px solid %1;border-radius:8px;}").arg(C_CYAN));
        card->setFixedSize(110, 110);

        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(4, 4, 4, 4);
        vl->setSpacing(1);
        vl->setAlignment(Qt::AlignCenter);

        auto* ic = makeLabel("🖥️", C_CYAN, 18);
        ic->setAlignment(Qt::AlignCenter);
        auto* rk = makeLabel("MASTER", C_CYAN, 8, true);
        rk->setAlignment(Qt::AlignCenter);
        auto* st = makeLabel("Orchestrating", C_MUTED, 7);
        st->setAlignment(Qt::AlignCenter);
        auto* dt = makeLabel("Queue + Dispatch", C_MUTED, 6);
        dt->setAlignment(Qt::AlignCenter);

        vl->addWidget(ic);
        vl->addWidget(rk);
        vl->addWidget(st);
        vl->addWidget(dt);
        hl->addWidget(card);
    }

    // ── Worker cards ───────────────────────────────
    for (int i = 1; i < m_mpi_size; ++i) {
        auto* card = new QFrame();
        card->setStyleSheet(
            QString("QFrame{background:%1;border:1px solid %2;border-radius:8px;}")
            .arg(C_CARD_IDLE).arg(C_BORDER));
        card->setFixedSize(110, 110);
        workerCards.append(card);
        workerStates.append(WS_IDLE);
        workerBatchCounts.append(0);

        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(4, 4, 4, 4);
        vl->setSpacing(1);
        vl->setAlignment(Qt::AlignCenter);

        auto* ic = makeLabel("⬜", C_MUTED, 18);
        ic->setAlignment(Qt::AlignCenter);
        workerIconLabels.append(ic);

        auto* rk = makeLabel(QString("WORKER %1").arg(i), C_MUTED, 8, true);
        rk->setAlignment(Qt::AlignCenter);
        workerRankLabels.append(rk);

        auto* st = makeLabel("Idle", C_MUTED, 7);
        st->setAlignment(Qt::AlignCenter);
        workerStatusLabels.append(st);

        auto* dt = makeLabel("—", C_MUTED, 6);
        dt->setAlignment(Qt::AlignCenter);
        dt->setWordWrap(true);
        workerStatLabels.append(dt);

        vl->addWidget(ic);
        vl->addWidget(rk);
        vl->addWidget(st);
        vl->addWidget(dt);
        hl->addWidget(card);
    }

    hl->addSpacing(4);
    strip->setMinimumWidth(m_mpi_size * 120 + 10);
    scroll->setWidget(strip);
    outer->addWidget(scroll);
}

// ============================================================================
// STATS LABELS
// ============================================================================
void MainWindow::buildStatsPanel()
{
    rowsCountLabel    = makeLabel("—", C_CYAN,  16, true);
    attacksCountLabel = makeLabel("—", C_RED,   16, true);
    speedLabel        = makeLabel("—", C_GREEN, 16, true);
}

void MainWindow::buildPhasePanel() { /* content inline in buildUI */ }

// ============================================================================
// FILE BROWSE
// ============================================================================
void MainWindow::onSelectFile()
{
    QString file = QFileDialog::getOpenFileName(
        this, "Select Binary Dataset", "", "Binary Files (*.bin)");
    if (!file.isEmpty()) pathInput->setText(file);
}

// ============================================================================
// RUN CLICKED
// ============================================================================
void MainWindow::onRunClicked()
{
    m_batchSize     = batchInput->text().toInt();
    std::string path = pathInput->text().toStdString();

    // ── Reset all UI ───────────────────────────────────────────────────────
    m_hasAlerted    = false;
    m_runInProgress = true;
    runButton->setEnabled(false);
    alertBanner->hide();
    summaryPanel->hide();
    rowsCountLabel->setText("0");
    attacksCountLabel->setText("0");
    speedLabel->setText("—");

    // Reset worker cards
    for (int i = 0; i < workerCards.size(); ++i) {
        setWorkerCardState(i, WS_IDLE);
        workerStatusLabels[i]->setText("Waiting...");
        workerStatLabels[i]->setText("—");
        workerBatchCounts[i] = 0;
    }

    // ── Set phases to RUNNING immediately (no QTimer — avoids race condition) ──
    // Calling the slot directly from the UI thread is safe and instant.
    onPhaseUpdate(0, PS_RUNNING, "Data Loading\nStreaming...");
    onPhaseUpdate(1, PS_RUNNING, "Preprocessing\nWaiting for data...");
    onPhaseUpdate(2, PS_RUNNING, "MPI Inference\nInitializing...");

    // ── Spawn controller thread ────────────────────────────────────────────
    std::thread controller([this, path]() {
        BatchManager<std::vector<float>> raw_queue;
        BatchManager<std::vector<float>> clean_queue;

        double loadActiveT  = 0.0;
        double prepActiveT  = 0.0;
        double global_start = MPI_Wtime();

        // Update phase labels to "active"
        QMetaObject::invokeMethod(this, "onPhaseUpdate",
            Q_ARG(int, 0), Q_ARG(int, (int)PS_RUNNING),
            Q_ARG(QString, QString("Data Loading\nStreaming batches...")));

        std::thread loaderThread(DataLoader::streamBinaryFile,
            path, m_batchSize, 80, std::ref(raw_queue), std::ref(loadActiveT));

        QMetaObject::invokeMethod(this, "onPhaseUpdate",
            Q_ARG(int, 1), Q_ARG(int, (int)PS_RUNNING),
            Q_ARG(QString, QString("Preprocessing\nRunning OpenMP...")));

        std::thread preprocessorThread(runPreprocessor,
            std::ref(raw_queue), std::ref(clean_queue), std::ref(prepActiveT));

        QMetaObject::invokeMethod(this, "onPhaseUpdate",
            Q_ARG(int, 2), Q_ARG(int, (int)PS_RUNNING),
            Q_ARG(QString, QString("MPI Inference\nDispatching batches...")));

        // ── Run inference (blocks until all batches processed) ────────────
        ProcessingStats stats = MPIEngine::runMasterInference(
            clean_queue, m_mpi_size, 39, this);

        if (loaderThread.joinable())       loaderThread.join();
        if (preprocessorThread.joinable()) preprocessorThread.join();

        double global_end    = MPI_Wtime();
        stats.load_time       = loadActiveT;
        stats.preprocess_time = prepActiveT;
        stats.total_time      = global_end - global_start;
        stats.rows_per_second = stats.total_rows /
                                (stats.total_time > 0 ? stats.total_time : 1);

        // ── Mark phases done ───────────────────────────────────────────────
        QMetaObject::invokeMethod(this, "onPhaseUpdate", Q_ARG(int, 0),
            Q_ARG(int, (int)PS_DONE),
            Q_ARG(QString, QString("Data Loading\n✓ Done in %1s").arg(loadActiveT, 0,'f',2)));
        QMetaObject::invokeMethod(this, "onPhaseUpdate", Q_ARG(int, 1),
            Q_ARG(int, (int)PS_DONE),
            Q_ARG(QString, QString("Preprocessing\n✓ Done in %1s").arg(prepActiveT, 0,'f',2)));
        QMetaObject::invokeMethod(this, "onPhaseUpdate", Q_ARG(int, 2),
            Q_ARG(int, (int)PS_DONE),
            Q_ARG(QString, QString("MPI Inference\n✓ Done in %1s").arg(stats.inference_time, 0,'f',2)));

        // ── Final live stats and report ────────────────────────────────────
        QMetaObject::invokeMethod(this, "onLiveStatsUpdate",
            Q_ARG(qlonglong, (qlonglong)stats.total_rows),
            Q_ARG(qlonglong, (qlonglong)stats.total_attacks));
        QMetaObject::invokeMethod(this, "showFinalStats",
            Q_ARG(ProcessingStats, stats));
    });

    controller.detach();
}

// ============================================================================
// updateLog — called by logMessage signal (from mpi_engine via invokeMethod)
// Parses [WORKER N] text and drives card state transitions
// ============================================================================
void MainWindow::updateLog(const QString& msg)
{
    parseWorkerLog(msg);
}

// ============================================================================
// WORKER LOG PARSER
//
// Worker log sequence per run:
//   1. "Engine online and model loaded."         → WS_LOADING
//   2. "Waiting for next job..."                 → WS_IDLE, then deferred→WS_ACTIVE
//      [processes all batches silently inside MPI inner loop]
//   3. "Job finished. Resetting for next run."   → WS_DONE
// ============================================================================
void MainWindow::parseWorkerLog(const QString& msg)
{
    QRegularExpression re(R"(\[WORKER (\d+)\] (.+))");
    auto match = re.match(msg);
    if (!match.hasMatch()) return;

    int     rank    = match.captured(1).toInt();
    QString text    = match.captured(2).trimmed();
    int     cardIdx = rank - 1;

    if (cardIdx < 0 || cardIdx >= workerCards.size()) return;

    if (text.contains("model loaded", Qt::CaseInsensitive) ||
        text.contains("Engine online", Qt::CaseInsensitive))
    {
        // Model is loaded — worker is ready but not yet processing
        setWorkerCardState(cardIdx, WS_LOADING);
        workerStatusLabels[cardIdx]->setText("Model Loaded");
        workerStatLabels[cardIdx]->setText("XGBoost ready");
    }
    else if (text.contains("Waiting for next job", Qt::CaseInsensitive))
    {
        // Worker says "waiting". If a run is in progress, it WILL receive data
        // shortly after. Show idle briefly, then flip to ACTIVE after 400ms.
        setWorkerCardState(cardIdx, WS_IDLE);
        workerStatusLabels[cardIdx]->setText("Waiting...");
        workerStatLabels[cardIdx]->setText("—");

        if (m_runInProgress) {
            // Immediately show ACTIVE — worker will receive its first batch
            // from the master within the next scheduler tick
            setWorkerCardState(cardIdx, WS_ACTIVE);
            workerStatusLabels[cardIdx]->setText("Processing data...");
            workerStatLabels[cardIdx]->setText("Running XGBoost");
        }
    }
    else if (text.contains("Job finished", Qt::CaseInsensitive) ||
             text.contains("Resetting for next run", Qt::CaseInsensitive))
    {
        // Job complete — worker finished all batches for this run.
        // Estimate rows: total data / (world_size-1). Not exact because
        // master dispatches asynchronously, but a fair approximation.
        int numWorkers = m_mpi_size - 1;
        setWorkerCardState(cardIdx, WS_DONE);
        workerStatusLabels[cardIdx]->setText("Job Complete");
        workerStatLabels[cardIdx]->setText(
            QString("~%1 rows inferred")
            .arg(numWorkers > 0 ?
                 QString::number(rowsCountLabel->text().toLongLong() / numWorkers) :
                 "?"));
    }
    else if (text.contains("CRITICAL", Qt::CaseInsensitive) ||
             text.contains("XGBoost Error", Qt::CaseInsensitive) ||
             text.contains("failed", Qt::CaseInsensitive))
    {
        setWorkerCardState(cardIdx, WS_ERROR);
        workerStatusLabels[cardIdx]->setText("Error!");
        workerStatLabels[cardIdx]->setText(text.left(30));
    }
    else if (text.contains("Shutting down", Qt::CaseInsensitive))
    {
        setWorkerCardState(cardIdx, WS_DONE);
        workerStatusLabels[cardIdx]->setText("Offline");
        workerStatLabels[cardIdx]->setText("Graceful exit");
    }
}

// ============================================================================
// ALERT — throttled, shows banner once per run
// ============================================================================
void MainWindow::showAlert(const QString&)
{
    if (!m_hasAlerted) {
        alertBanner->show();
        m_hasAlerted = true;
    }
}

// ============================================================================
// SET WORKER CARD STATE (visual only)
// ============================================================================
void MainWindow::setWorkerCardState(int cardIdx, int state)
{
    if (cardIdx < 0 || cardIdx >= workerCards.size()) return;

    struct V { const char* bg; const char* border; const char* icon; const char* col; };
    static const V visuals[] = {
        { C_CARD_IDLE,   C_BORDER,  "⬜", C_MUTED   }, // IDLE
        { C_CARD_LOAD,   "#4c5fd5", "🔵", "#93c5fd" }, // LOADING (model ready)
        { C_CARD_ACTIVE, C_CYAN,    "🟢", C_CYAN    }, // ACTIVE (processing)
        { C_CARD_DONE,   C_GREEN,   "✅", C_GREEN   }, // DONE
        { C_CARD_ERROR,  C_RED,     "🔴", C_RED     }, // ERROR
    };
    if (state < 0 || state > 4) return;
    const auto& v = visuals[state];

    workerCards[cardIdx]->setStyleSheet(
        QString("QFrame{background:%1;border:2px solid %2;border-radius:8px;}")
        .arg(v.bg).arg(v.border));
    workerIconLabels[cardIdx]->setText(v.icon);
    workerRankLabels[cardIdx]->setStyleSheet(
        QString("color:%1;font-size:8pt;font-weight:bold;background:transparent;").arg(v.col));
    workerStatusLabels[cardIdx]->setStyleSheet(
        QString("color:%1;font-size:7pt;background:transparent;").arg(v.col));
    workerStatLabels[cardIdx]->setStyleSheet(
        QString("color:%1;font-size:6pt;background:transparent;").arg(
            state == WS_ACTIVE ? C_CYAN : C_MUTED));
    workerStates[cardIdx] = state;
}

// ============================================================================
// SLOT: WORKER STATE CHANGED
// ============================================================================
void MainWindow::onWorkerStateChanged(int rank, int state, const QString& label)
{
    setWorkerCardState(rank, state);
    if (rank >= 0 && rank < workerStatusLabels.size())
        workerStatusLabels[rank]->setText(label);
}

// ============================================================================
// SLOT: PHASE UPDATE
// ============================================================================
void MainWindow::onPhaseUpdate(int phase, int state, const QString& label)
{
    QLabel* icon = nullptr;
    QLabel* lbl  = nullptr;
    if      (phase == 0) { icon = phaseLoadIcon; lbl = phaseLoadLabel; }
    else if (phase == 1) { icon = phasePrepIcon; lbl = phasePrepLabel; }
    else if (phase == 2) { icon = phaseInfIcon;  lbl = phaseInfLabel;  }
    if (!icon || !lbl) return;

    struct PV { const char* emoji; const char* color; };
    static const PV pv[] = {
        { "⬜", C_MUTED  }, // IDLE
        { "🔄", C_ORANGE }, // RUNNING
        { "✅", C_GREEN  }, // DONE
        { "❌", C_RED    }, // ERROR
    };
    if (state < 0 || state > 3) return;

    icon->setText(pv[state].emoji);
    icon->setStyleSheet(
        QString("color:%1;font-size:16pt;background:transparent;").arg(pv[state].color));
    lbl->setText(label);
    lbl->setStyleSheet(
        QString("color:%1;font-size:8pt;background:transparent;").arg(pv[state].color));
}

// ============================================================================
// SLOT: LIVE STATS
// ============================================================================
void MainWindow::onLiveStatsUpdate(qlonglong rows, qlonglong attacks)
{
    rowsCountLabel->setText(QString::number(rows));
    attacksCountLabel->setText(QString::number(attacks));
}

// ============================================================================
// FINAL STATS — polished two-column report
// ============================================================================
void MainWindow::showFinalStats(ProcessingStats stats)
{
    m_runInProgress = false;

    // Update live stats
    rowsCountLabel->setText(QString::number(stats.total_rows));
    attacksCountLabel->setText(QString::number(stats.total_attacks));
    speedLabel->setText(QString::number(stats.rows_per_second, 'f', 0));

    // Update any worker cards still showing ACTIVE → DONE
    for (int i = 0; i < workerCards.size(); ++i) {
        if (workerStates[i] == WS_ACTIVE || workerStates[i] == WS_IDLE) {
            setWorkerCardState(i, WS_DONE);
            workerStatusLabels[i]->setText("Job Complete");
            int numWorkers = m_mpi_size - 1;
            long estRows = (numWorkers > 0) ? stats.total_rows / numWorkers : 0;
            workerStatLabels[i]->setText(
                QString("~%1 rows inferred").arg(estRows));
        }
    }

    // ── Build polished HTML report ────────────────────────────────────────
    // Uses a borderless table with clear row grouping
    const char* NONE = C_SURFACE;   // unused but kept for symmetry
    (void)NONE;

    auto fmtNum = [](long v) {
        // Format with thousands separator
        QString s = QString::number(v);
        for (int i = s.length() - 3; i > 0; i -= 3)
            s.insert(i, ',');
        return s;
    };

    QString attackColor = stats.total_attacks > 0 ? C_RED : C_GREEN;
    QString safeStatus  = stats.total_attacks > 0
                          ? QString("<b style='color:%1;'>⚠ %2 threats detected</b>")
                                .arg(C_RED).arg(fmtNum(stats.total_attacks))
                          : QString("<b style='color:%1;'>✓ No threats detected</b>").arg(C_GREEN);

    QString html = QString(R"(
<table width='100%%' cellspacing='0' cellpadding='0' style='border-collapse:collapse;'>

  <tr>
    <td colspan='4' style='padding-bottom:8px;'>
      <span style='color:%1;font-size:10pt;font-weight:bold;'>Rows Processed: %2</span>
      &nbsp;&nbsp;&nbsp;
      %3
    </td>
  </tr>

  <tr>
    <td width='1px' style='background:%4; padding:0 1px 0 0;'>&nbsp;</td>
    <td width='48%%' style='padding:4px 12px 4px 10px;'>
      <span style='color:%5;font-size:8pt;'>⏱ PHASE TIMINGS</span><br/>
      <table width='100%%' cellspacing='2' cellpadding='0' style='margin-top:4px;'>
        <tr>
          <td style='color:%5;font-size:8pt;'>Data Loading</td>
          <td align='right' style='color:%6;font-size:8pt;font-weight:bold;'>%7 s</td>
        </tr>
        <tr>
          <td style='color:%5;font-size:8pt;'>Preprocessing&nbsp;<small style='color:%8;'>(OpenMP)</small></td>
          <td align='right' style='color:%6;font-size:8pt;font-weight:bold;'>%9 s</td>
        </tr>
        <tr>
          <td style='color:%5;font-size:8pt;'>MPI Inference&nbsp;<small style='color:%8;'>(×%10 workers)</small></td>
          <td align='right' style='color:%6;font-size:8pt;font-weight:bold;'>%11 s</td>
        </tr>
      </table>
    </td>
    <td width='8px'></td>
    <td width='48%%' style='padding:4px 10px;'>
      <span style='color:%5;font-size:8pt;'>🚀 PERFORMANCE</span><br/>
      <table width='100%%' cellspacing='2' cellpadding='0' style='margin-top:4px;'>
        <tr>
          <td style='color:%5;font-size:8pt;'>Total Wall Time</td>
          <td align='right' style='color:%12;font-size:8pt;font-weight:bold;'>%13 s</td>
        </tr>
        <tr>
          <td style='color:%5;font-size:8pt;'>Throughput</td>
          <td align='right' style='color:%14;font-size:8pt;font-weight:bold;'>%15 rows/s</td>
        </tr>
        <tr>
          <td style='color:%5;font-size:8pt;'>Workers Used</td>
          <td align='right' style='color:%6;font-size:8pt;font-weight:bold;'>%16</td>
        </tr>
      </table>
    </td>
  </tr>

</table>
)")
    .arg(C_CYAN)                                                   // %1  heading color
    .arg(fmtNum(stats.total_rows))                                 // %2  total rows
    .arg(safeStatus)                                               // %3  attack status
    .arg(C_BORDER)                                                 // %4  vertical divider
    .arg(C_MUTED)                                                  // %5  label color
    .arg(C_TEXT)                                                   // %6  value color
    .arg(stats.load_time,       0, 'f', 3)                        // %7  load time
    .arg(C_MUTED)                                                  // %8  dim sub-label
    .arg(stats.preprocess_time, 0, 'f', 3)                        // %9  prep time
    .arg(m_mpi_size - 1)                                          // %10 num workers
    .arg(stats.inference_time,  0, 'f', 3)                        // %11 inf time
    .arg(C_TEXT)                                                   // %12 total time color
    .arg(stats.total_time,      0, 'f', 3)                        // %13 total time
    .arg(C_GREEN)                                                  // %14 throughput color
    .arg(QString::number(stats.rows_per_second, 'f', 0))          // %15 rows/s
    .arg(m_mpi_size - 1);                                         // %16 num workers

    summaryContent->setText(html);
    summaryPanel->show();
    runButton->setEnabled(true);
}

// ============================================================================
// DESTRUCTOR
// ============================================================================
MainWindow::~MainWindow()
{
    MPIEngine::broadcastKillSignal(m_mpi_size);
}