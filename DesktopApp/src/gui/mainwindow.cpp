// ============================================================================
// mainwindow.cpp
// ============================================================================
#include "mainwindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QScrollArea>
#include <QRegularExpression>
#include <QShortcut>
#include <QFileInfo>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QProgressBar>
#include <thread>
#include <mpi.h>
#include "../engine/data_loader.h"
#include "../engine/preprocessor.h"
#include "../parallel/mpi_engine.h"

// ============================================================================
// PALETTE
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

static QLabel* makeLabel(const QString& text, const QString& color,
                          int ptSize = 10, bool bold = false)
{
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;font-size:%2pt;font-weight:%3;background:transparent;")
        .arg(color).arg(ptSize).arg(bold?"bold":"normal"));
    return l;
}

// helper: horizontal line
static QFrame* makeSep() {
    auto* f = new QFrame(); f->setFixedHeight(1);
    f->setStyleSheet(QString("background:%1;border:none;").arg(C_BORDER));
    return f;
}

// ============================================================================
// CONSTRUCTOR
// ============================================================================
MainWindow::MainWindow(int mpi_size, QWidget* parent)
    : QMainWindow(parent),
      m_mpi_size(mpi_size), m_batchSize(100000),
      m_hasAlerted(false), m_runInProgress(false),
      m_animPhase(false), m_historyVisible(false),
      m_lastSpeed(0.0), m_ganttWidth(400),
      pathInput(nullptr), batchInput(nullptr),
      runButton(nullptr), browseButton(nullptr), fileInfoLabel(nullptr),
      workerGridFrame(nullptr),
      phaseLoadIcon(nullptr), phaseLoadLabel(nullptr),
      phasePrepIcon(nullptr), phasePrepLabel(nullptr),
      phaseInfIcon(nullptr),  phaseInfLabel(nullptr),
      rowsCountLabel(nullptr), attacksCountLabel(nullptr), speedLabel(nullptr),
      attackGaugeBar(nullptr), attackPctLabel(nullptr),
      alertBanner(nullptr), alertLabel(nullptr),
      summaryPanel(nullptr), summaryTitle(nullptr), summaryContent(nullptr),
      exportButton(nullptr),
      throughputPanel(nullptr),
      tpLoadBar(nullptr), tpLoadLabel(nullptr),
      tpPrepBar(nullptr), tpPrepLabel(nullptr),
      tpInfBar(nullptr),  tpInfLabel(nullptr),
      historyPanel(nullptr), historyContent(nullptr),
      historyGrid(nullptr), historyToggleBtn(nullptr),
      m_animTimer(nullptr)
{
    qRegisterMetaType<ProcessingStats>("ProcessingStats");
    applyDarkTheme();
    buildUI();

    connect(browseButton, &QPushButton::clicked,   this, &MainWindow::onSelectFile);
    connect(runButton,    &QPushButton::clicked,   this, &MainWindow::onRunClicked);
    connect(this, &MainWindow::logMessage,         this, &MainWindow::updateLog);
    connect(this, &MainWindow::intrusionAlert,     this, &MainWindow::showAlert);
    connect(this, &MainWindow::workerStateChanged, this, &MainWindow::onWorkerStateChanged);
    connect(this, &MainWindow::phaseUpdate,        this, &MainWindow::onPhaseUpdate);
    connect(this, &MainWindow::liveStatsUpdate,    this, &MainWindow::onLiveStatsUpdate);

    // ── Keyboard shortcuts (Feature 9) ─────────────────────────────────────
    auto* scRun = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(scRun, &QShortcut::activated, this, &MainWindow::onRunClicked);
    auto* scOpen = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_O), this);
    connect(scOpen, &QShortcut::activated, this, &MainWindow::onSelectFile);

    // ── Animation timer (Feature 7) ────────────────────────────────────────
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(650);
    connect(m_animTimer, &QTimer::timeout, this, &MainWindow::onAnimTick);

    setWindowTitle("Parallel NIDS Engine  ·  CUI Lahore");
    setMinimumSize(800, 660);
    resize(980, 760);
}

// ============================================================================
// DARK THEME
// ============================================================================
void MainWindow::applyDarkTheme()
{
    setStyleSheet(QString(R"(
        QMainWindow,QWidget{background-color:%1;color:%2;font-family:'Segoe UI','Inter',sans-serif;}
        QLineEdit{background:%3;color:%2;border:1px solid %4;border-radius:6px;
                  padding:6px 10px;font-size:9pt;min-height:26px;}
        QLineEdit:focus{border:1px solid %5;}
        QPushButton#browseBtn{background:%3;color:%6;border:1px solid %4;border-radius:6px;
            padding:6px 14px;font-size:9pt;font-weight:bold;min-height:28px;}
        QPushButton#browseBtn:hover{background:%7;border-color:%6;}
        QPushButton#runBtn{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0e7490,stop:1 #0284c7);
            color:#fff;border:none;border-radius:8px;padding:10px 28px;
            font-size:11pt;font-weight:bold;letter-spacing:1px;min-height:38px;}
        QPushButton#runBtn:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0891b2,stop:1 #0369a1);}
        QPushButton#runBtn:disabled{background:%3;color:%6;}
        QPushButton#exportBtn{background:%3;color:%5;border:1px solid %5;border-radius:5px;
            padding:3px 10px;font-size:8pt;}
        QPushButton#exportBtn:hover{background:%7;}
        QPushButton#histToggle{background:transparent;color:%6;border:none;
            font-size:8pt;font-weight:bold;text-align:left;}
        QPushButton#histToggle:hover{color:%2;}
        QProgressBar{background:%3;border:1px solid %4;border-radius:4px;
            height:8px;text-align:center;}
        QProgressBar::chunk{border-radius:4px;}
        QScrollArea{border:none;background:transparent;}
        QScrollBar:vertical{background:%3;width:8px;border-radius:4px;}
        QScrollBar:horizontal{background:%3;height:8px;border-radius:4px;}
        QScrollBar::handle:vertical,QScrollBar::handle:horizontal
            {background:%4;border-radius:4px;min-height:20px;min-width:20px;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical,
        QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{height:0;width:0;}
    )")
    .arg(C_BG).arg(C_TEXT).arg(C_SURFACE).arg(C_BORDER)
    .arg(C_CYAN).arg(C_MUTED).arg(C_SURFACE2));
}

// ============================================================================
// BUILD UI — root layout + all panels
// ============================================================================
void MainWindow::buildUI()
{
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* central = new QWidget();
    central->setStyleSheet(QString("background-color:%1;").arg(C_BG));
    scrollArea->setWidget(central);
    setCentralWidget(scrollArea);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16,16,16,16);
    root->setSpacing(10);

    // ── Title row (Feature 6 — system config badge) ────────────────────────
    {
        auto* row = new QHBoxLayout();
        row->addWidget(makeLabel("⚡  PARALLEL NIDS ENGINE", C_TEXT, 13, true));
        row->addStretch();

        // System config badge
        int cpuCores = QThread::idealThreadCount();
        int workers  = m_mpi_size - 1;
#ifdef _OPENMP
        QString sysInfo = QString("🖥 %1 CPU threads  ·  🔗 %2 MPI nodes  ·  ⚙ OpenMP")
                          .arg(cpuCores).arg(workers);
#else
        QString sysInfo = QString("🖥 %1 CPU threads  ·  🔗 %2 MPI nodes")
                          .arg(cpuCores).arg(workers);
#endif
        auto* sysLbl = makeLabel(sysInfo, C_MUTED, 8);
        row->addWidget(sysLbl);
        root->addLayout(row);
    }
    root->addWidget(makeLabel("CUI Lahore  ·  MPI + OpenMP + XGBoost", C_MUTED, 8));
    root->addWidget(makeSep());

    // ── Alert Banner ───────────────────────────────────────────────────────
    alertBanner = new QFrame();
    alertBanner->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #7f1d1d,stop:1 #450a0a);"
        "border:1px solid #f85149;border-radius:8px;");
    alertBanner->setMinimumHeight(44);
    { auto* r=new QHBoxLayout(alertBanner); r->setContentsMargins(12,6,12,6);
      alertLabel=makeLabel("🚨  INTRUSION DETECTED — Malicious traffic flagged!",C_RED,10,true);
      r->addWidget(alertLabel); }
    alertBanner->hide();
    root->addWidget(alertBanner);

    // ── Control Panel ──────────────────────────────────────────────────────
    buildControlPanel();
    {
        auto* frame = new QFrame();
        frame->setStyleSheet(
            QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
            .arg(C_SURFACE).arg(C_BORDER));
        auto* vl = new QVBoxLayout(frame);
        vl->setContentsMargins(14,12,14,12); vl->setSpacing(8);
        vl->addWidget(makeLabel("DATASET CONFIGURATION",C_MUTED,8,true));

        auto* fileRow=new QHBoxLayout(); fileRow->setSpacing(8);
        fileRow->addWidget(pathInput,1); fileRow->addWidget(browseButton);
        vl->addLayout(fileRow);

        // Feature 5 — dataset info row
        vl->addWidget(fileInfoLabel);

        auto* batchRow=new QHBoxLayout(); batchRow->setSpacing(8);
        batchRow->addWidget(makeLabel("Batch Size:",C_MUTED,9));
        batchRow->addWidget(batchInput);
        batchRow->addStretch();
        batchRow->addWidget(makeLabel("F5 = Run  ·  Ctrl+O = Browse",C_MUTED,7));
        batchRow->addSpacing(12);
        batchRow->addWidget(runButton);
        vl->addLayout(batchRow);

        root->addWidget(frame);
    }

    // ── Pipeline Phases ────────────────────────────────────────────────────
    buildPhasePanel();
    {
        auto* frame=new QFrame();
        frame->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
            .arg(C_SURFACE).arg(C_BORDER));
        frame->setMinimumHeight(105);
        auto* vl=new QVBoxLayout(frame);
        vl->setContentsMargins(14,10,14,10); vl->setSpacing(4);
        vl->addWidget(makeLabel("PIPELINE PHASES",C_MUTED,8,true));

        auto* phRow=new QHBoxLayout(); phRow->setSpacing(0);
        auto makePhase=[&](QLabel*& ic,QLabel*& lb,const QString& ico,const QString& lbText,int minW){
            auto* col=new QVBoxLayout(); col->setSpacing(2); col->setAlignment(Qt::AlignCenter);
            ic=makeLabel(ico,C_MUTED,16); ic->setAlignment(Qt::AlignCenter); ic->setMinimumHeight(28);
            lb=makeLabel(lbText,C_MUTED,8); lb->setAlignment(Qt::AlignCenter);
            lb->setWordWrap(true); lb->setMinimumWidth(minW);
            col->addWidget(ic); col->addWidget(lb); return col; };
        auto makeArr=[&](){ auto* a=makeLabel("→",C_BORDER,16,true);
                            a->setAlignment(Qt::AlignCenter); a->setFixedWidth(32); return a; };

        phRow->addStretch(1);
        phRow->addLayout(makePhase(phaseLoadIcon,phaseLoadLabel,"⬜","Data Loading\nIdle",85));
        phRow->addWidget(makeArr());
        phRow->addLayout(makePhase(phasePrepIcon,phasePrepLabel,"⬜","Preprocessing\nIdle",90));
        phRow->addWidget(makeArr());
        phRow->addLayout(makePhase(phaseInfIcon, phaseInfLabel, "⬜","MPI Inference\nIdle",90));
        phRow->addStretch(1);
        vl->addLayout(phRow);
        root->addWidget(frame);
    }

    // ── Worker Cards ───────────────────────────────────────────────────────
    buildWorkerGrid();
    root->addWidget(workerGridFrame);

    // ── Live Stats + Attack Gauge ──────────────────────────────────────────
    buildStatsPanel();
    {
        auto* frame=new QFrame();
        frame->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
            .arg(C_SURFACE).arg(C_BORDER));
        frame->setMinimumHeight(110);
        auto* vl=new QVBoxLayout(frame); vl->setContentsMargins(14,8,14,8); vl->setSpacing(6);

        // Top: 3 stat counters
        auto* stRow=new QHBoxLayout(); stRow->setSpacing(0);
        auto addStat=[&](const QString& ic,QLabel* cnt,const QString& nm){
            auto* col=new QVBoxLayout(); col->setSpacing(1); col->setAlignment(Qt::AlignCenter);
            auto* icL=makeLabel(ic,C_TEXT,14); icL->setAlignment(Qt::AlignCenter);
            cnt->setAlignment(Qt::AlignCenter);
            auto* nmL=makeLabel(nm,C_MUTED,8); nmL->setAlignment(Qt::AlignCenter);
            col->addWidget(icL); col->addWidget(cnt); col->addWidget(nmL);
            stRow->addLayout(col); };
        addStat("📦",rowsCountLabel,"Rows Processed");
        stRow->addStretch();
        addStat("⚠️",attacksCountLabel,"Attacks Detected");
        stRow->addStretch();
        addStat("⚡",speedLabel,"Rows / Second");
        vl->addLayout(stRow);

        // Feature 3 — attack rate gauge
        {
            auto* gRow=new QHBoxLayout(); gRow->setSpacing(8);
            gRow->addWidget(makeLabel("Attack Rate:",C_MUTED,8));
            gRow->addWidget(attackGaugeBar,1);
            gRow->addWidget(attackPctLabel);
            vl->addLayout(gRow);
        }
        root->addWidget(frame);
    }

    // ── Summary Panel ─────────────────────────────────────────────────────
    summaryPanel=new QFrame();
    summaryPanel->setStyleSheet(
        QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
        .arg(C_SURFACE).arg(C_BORDER));
    {
        auto* vl=new QVBoxLayout(summaryPanel);
        vl->setContentsMargins(16,14,16,14); vl->setSpacing(8);

        auto* hdr=new QHBoxLayout();
        summaryTitle=makeLabel("📊  FINAL PROCESSING REPORT",C_GREEN,10,true);
        hdr->addWidget(summaryTitle);
        hdr->addStretch();
        // Feature 8 — export button
        exportButton=new QPushButton("💾  Save Report");
        exportButton->setObjectName("exportBtn");
        connect(exportButton,&QPushButton::clicked,this,&MainWindow::onExportReport);
        hdr->addWidget(exportButton);
        vl->addLayout(hdr);
        vl->addWidget(makeSep());

        summaryContent=new QLabel();
        summaryContent->setStyleSheet(
            QString("color:%1;font-size:9pt;background:transparent;").arg(C_TEXT));
        summaryContent->setWordWrap(true);
        summaryContent->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        vl->addWidget(summaryContent);
    }
    summaryPanel->hide();
    root->addWidget(summaryPanel);

    // ── Throughput Visualization ───────────────────────────────────────────
    buildThroughputPanel();
    root->addWidget(throughputPanel);

    // ── Run History Panel ─────────────────────────────────────────────────
    buildRunHistoryPanel();
    root->addWidget(historyPanel);
}

// ============================================================================
// CONTROL PANEL
// ============================================================================
void MainWindow::buildControlPanel()
{
    pathInput=new QLineEdit();
    pathInput->setPlaceholderText("Select a .bin dataset file…");

    browseButton=new QPushButton("Browse");
    browseButton->setObjectName("browseBtn");
    browseButton->setFixedWidth(80);

    batchInput=new QLineEdit("100000");
    batchInput->setFixedWidth(100);

    runButton=new QPushButton("▶  RUN ENGINE");
    runButton->setObjectName("runBtn");

    // Feature 1+5 — file info label
    fileInfoLabel=makeLabel("No file selected",C_MUTED,8);
}

// ============================================================================
// WORKER GRID
// ============================================================================
void MainWindow::buildWorkerGrid()
{
    workerGridFrame=new QFrame();
    workerGridFrame->setStyleSheet(
        QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
        .arg(C_SURFACE).arg(C_BORDER));

    auto* outer=new QVBoxLayout(workerGridFrame);
    outer->setContentsMargins(14,10,14,10); outer->setSpacing(6);

    auto* hdr=new QHBoxLayout();
    hdr->addWidget(makeLabel("MPI WORKER THREADS",C_MUTED,8,true));
    hdr->addStretch();
    hdr->addWidget(makeLabel(
        QString("World Size: %1  (1 Master + %2 Workers)").arg(m_mpi_size).arg(m_mpi_size-1),
        C_MUTED,8));
    outer->addLayout(hdr);

    auto* scroll=new QScrollArea();
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(132);
    scroll->setWidgetResizable(false);
    scroll->setStyleSheet("background:transparent;");

    auto* strip=new QWidget(); strip->setStyleSheet("background:transparent;");
    auto* hl=new QHBoxLayout(strip);
    hl->setContentsMargins(0,0,0,0); hl->setSpacing(10);
    hl->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);

    // Master card
    {
        auto* card=new QFrame();
        card->setStyleSheet(QString("QFrame{background:#1c2133;border:2px solid %1;border-radius:8px;}").arg(C_CYAN));
        card->setFixedSize(110,110);
        auto* vl=new QVBoxLayout(card); vl->setContentsMargins(4,4,4,4);
        vl->setSpacing(1); vl->setAlignment(Qt::AlignCenter);
        auto* ic=makeLabel("🖥️",C_CYAN,18); ic->setAlignment(Qt::AlignCenter);
        auto* rk=makeLabel("MASTER",C_CYAN,8,true); rk->setAlignment(Qt::AlignCenter);
        auto* st=makeLabel("Orchestrating",C_MUTED,7); st->setAlignment(Qt::AlignCenter);
        auto* dt=makeLabel("Queue + Dispatch",C_MUTED,6); dt->setAlignment(Qt::AlignCenter);
        vl->addWidget(ic); vl->addWidget(rk); vl->addWidget(st); vl->addWidget(dt);
        hl->addWidget(card);
    }

    for(int i=1;i<m_mpi_size;++i){
        auto* card=new QFrame();
        card->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:8px;}")
            .arg(C_CARD_IDLE).arg(C_BORDER));
        card->setFixedSize(110,110);
        workerCards.append(card); workerStates.append(WS_IDLE); workerBatchCounts.append(0);

        auto* vl=new QVBoxLayout(card); vl->setContentsMargins(4,4,4,4);
        vl->setSpacing(1); vl->setAlignment(Qt::AlignCenter);
        auto* ic=makeLabel("⬜",C_MUTED,18); ic->setAlignment(Qt::AlignCenter);
        workerIconLabels.append(ic);
        auto* rk=makeLabel(QString("WORKER %1").arg(i),C_MUTED,8,true); rk->setAlignment(Qt::AlignCenter);
        workerRankLabels.append(rk);
        auto* st=makeLabel("Idle",C_MUTED,7); st->setAlignment(Qt::AlignCenter);
        workerStatusLabels.append(st);
        auto* dt=makeLabel("—",C_MUTED,6); dt->setAlignment(Qt::AlignCenter); dt->setWordWrap(true);
        workerStatLabels.append(dt);
        vl->addWidget(ic); vl->addWidget(rk); vl->addWidget(st); vl->addWidget(dt);
        hl->addWidget(card);
    }
    hl->addSpacing(4);
    strip->setMinimumWidth(m_mpi_size*120+10);
    scroll->setWidget(strip);
    outer->addWidget(scroll);
}

// ============================================================================
// STATS + ATTACK GAUGE (Feature 3)
// ============================================================================
void MainWindow::buildStatsPanel()
{
    rowsCountLabel    = makeLabel("—",C_CYAN,  16,true);
    attacksCountLabel = makeLabel("—",C_RED,   16,true);
    speedLabel        = makeLabel("—",C_GREEN, 16,true);

    attackGaugeBar = new QProgressBar();
    attackGaugeBar->setRange(0,1000);
    attackGaugeBar->setValue(0);
    attackGaugeBar->setTextVisible(false);
    attackGaugeBar->setFixedHeight(8);
    attackGaugeBar->setStyleSheet(
        QString("QProgressBar{background:%1;border:1px solid %2;border-radius:4px;}"
                "QProgressBar::chunk{background:%3;border-radius:4px;}")
        .arg(C_SURFACE2).arg(C_BORDER).arg(C_GREEN));

    attackPctLabel = makeLabel("—",C_MUTED,8);
}

void MainWindow::buildPhasePanel() { /* labels created inline */ }

// ============================================================================
// THROUGHPUT VISUALIZATION (post-run Gantt, Feature R)
// ============================================================================
void MainWindow::buildThroughputPanel()
{
    throughputPanel = new QFrame();
    throughputPanel->setStyleSheet(
        QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
        .arg(C_SURFACE).arg(C_BORDER));

    auto* vl=new QVBoxLayout(throughputPanel);
    vl->setContentsMargins(14,12,14,12); vl->setSpacing(8);

    auto* hdr=new QHBoxLayout();
    hdr->addWidget(makeLabel("⏱  PHASE TIMELINE",C_MUTED,8,true));
    hdr->addStretch();
    hdr->addWidget(makeLabel("proportional to wall-clock time",C_MUTED,7));
    vl->addLayout(hdr);

    // Helper to build one Gantt row
    auto makeRow=[&](QFrame*& bar, QLabel*& lbl, const QString& bgColor)-> QHBoxLayout* {
        auto* row = new QHBoxLayout(); row->setSpacing(8);
        bar=new QFrame(); bar->setFixedHeight(16);
        bar->setStyleSheet(
            QString("QFrame{background:%1;border-radius:4px;border:none;}").arg(bgColor));
        bar->setFixedWidth(0); // will be updated
        lbl=makeLabel("—",C_MUTED,8); lbl->setMinimumWidth(180);
        row->addWidget(bar); row->addWidget(lbl); row->addStretch();
        return row; };

    auto* r1=makeRow(tpLoadBar,tpLoadLabel,C_ORANGE);
    vl->addLayout(r1);
    auto* r2=makeRow(tpPrepBar,tpPrepLabel,"#4c5fd5");
    vl->addLayout(r2);
    auto* r3=makeRow(tpInfBar, tpInfLabel, C_CYAN);
    vl->addLayout(r3);

    throughputPanel->hide();
}

// ============================================================================
// RUN HISTORY (Feature 2)
// ============================================================================
void MainWindow::buildRunHistoryPanel()
{
    historyPanel = new QFrame();
    historyPanel->setStyleSheet(
        QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
        .arg(C_SURFACE).arg(C_BORDER));

    auto* outer=new QVBoxLayout(historyPanel);
    outer->setContentsMargins(14,10,14,10); outer->setSpacing(6);

    // Header row with toggle
    auto* hdr=new QHBoxLayout();
    historyToggleBtn=new QPushButton("▶  RUN HISTORY  (last 5 runs)");
    historyToggleBtn->setObjectName("histToggle");
    connect(historyToggleBtn,&QPushButton::clicked,this,&MainWindow::onHistoryToggle);
    hdr->addWidget(historyToggleBtn); hdr->addStretch();
    outer->addLayout(hdr);

    // Collapsible content
    historyContent=new QWidget();
    historyContent->setStyleSheet("background:transparent;");
    historyGrid=new QGridLayout(historyContent);
    historyGrid->setContentsMargins(0,4,0,0); historyGrid->setSpacing(4);

    // Column headers
    QStringList cols={"Date/Time","File","Rows","Attacks","Time (s)","Rows/s"};
    for(int c=0;c<cols.size();++c){
        auto* lbl=makeLabel(cols[c],C_MUTED,8,true);
        historyGrid->addWidget(lbl,0,c);
    }
    // 5 data rows × 6 cols = 30 labels
    for(int r=0;r<5;++r)
        for(int c=0;c<6;++c){
            auto* lbl=makeLabel("—",C_MUTED,8);
            historyGrid->addWidget(lbl,r+1,c);
            histHistLabels.append(lbl);
        }

    historyContent->hide();
    outer->addWidget(historyContent);
}

// ============================================================================
// FILE SELECT + INFO (Features 1 & 5)
// ============================================================================
void MainWindow::onSelectFile()
{
    QString file=QFileDialog::getOpenFileName(
        this,"Select Binary Dataset","","Binary Files (*.bin)");
    if(!file.isEmpty()){
        pathInput->setText(file);
        updateFileInfo(file);
    }
}

void MainWindow::updateFileInfo(const QString& filePath)
{
    m_lastFile = QFileInfo(filePath).fileName();
    QFileInfo fi(filePath);
    if(!fi.exists()){ fileInfoLabel->setText("File not found"); return; }

    qint64 bytes     = fi.size();
    double mbSize    = bytes / 1e6;
    long long estRows = bytes / (80 * 4);   // 80 features × 4 bytes per float

    QString info = QString("📁  %1 MB  ·  Est. %2 rows  ·  80 features")
                   .arg(mbSize, 0,'f',1)
                   .arg(QString::number(estRows));

    if(m_lastSpeed > 0){
        double estSec = estRows / m_lastSpeed;
        info += QString("  ·  ETA ~%1 s").arg(estSec,0,'f',1);
    }
    fileInfoLabel->setText(info);
    fileInfoLabel->setStyleSheet(
        QString("color:%1;font-size:8pt;background:transparent;").arg(C_CYAN));
}

// ============================================================================
// RUN CLICKED
// ============================================================================
void MainWindow::onRunClicked()
{
    m_batchSize      = batchInput->text().toInt();
    std::string path = pathInput->text().toStdString();

    m_hasAlerted    = false;
    m_runInProgress = true;
    runButton->setEnabled(false);
    alertBanner->hide();
    summaryPanel->hide();
    throughputPanel->hide();
    rowsCountLabel->setText("0");
    attacksCountLabel->setText("0");
    speedLabel->setText("—");
    attackPctLabel->setText("—");
    attackGaugeBar->setValue(0);
    attackGaugeBar->setStyleSheet(
        QString("QProgressBar{background:%1;border:1px solid %2;border-radius:4px;}"
                "QProgressBar::chunk{background:%3;border-radius:4px;}")
        .arg(C_SURFACE2).arg(C_BORDER).arg(C_GREEN));

    for(int i=0;i<workerCards.size();++i){
        setWorkerCardState(i,WS_IDLE);
        workerStatusLabels[i]->setText("Waiting...");
        workerStatLabels[i]->setText("—");
        workerBatchCounts[i]=0;
    }

    onPhaseUpdate(0,PS_RUNNING,"Data Loading\nStreaming...");
    onPhaseUpdate(1,PS_RUNNING,"Preprocessing\nWaiting for data...");
    onPhaseUpdate(2,PS_RUNNING,"MPI Inference\nInitializing...");

    std::thread controller([this,path](){
        BatchManager<std::vector<float>> raw_queue;
        BatchManager<std::vector<float>> clean_queue;
        double loadT=0.0, prepT=0.0;
        double t0=MPI_Wtime();

        QMetaObject::invokeMethod(this,"onPhaseUpdate",
            Q_ARG(int,0),Q_ARG(int,(int)PS_RUNNING),
            Q_ARG(QString,QString("Data Loading\nStreaming batches...")));
        std::thread loaderThread(DataLoader::streamBinaryFile,
            path,m_batchSize,80,std::ref(raw_queue),std::ref(loadT));

        QMetaObject::invokeMethod(this,"onPhaseUpdate",
            Q_ARG(int,1),Q_ARG(int,(int)PS_RUNNING),
            Q_ARG(QString,QString("Preprocessing\nRunning OpenMP...")));
        std::thread prepThread(runPreprocessor,
            std::ref(raw_queue),std::ref(clean_queue),std::ref(prepT));

        QMetaObject::invokeMethod(this,"onPhaseUpdate",
            Q_ARG(int,2),Q_ARG(int,(int)PS_RUNNING),
            Q_ARG(QString,QString("MPI Inference\nDispatching batches...")));

        ProcessingStats stats=MPIEngine::runMasterInference(
            clean_queue,m_mpi_size,39,this);

        if(loaderThread.joinable()) loaderThread.join();
        if(prepThread.joinable())   prepThread.join();

        double t1=MPI_Wtime();
        stats.load_time       = loadT;
        stats.preprocess_time = prepT;
        stats.total_time      = t1-t0;
        stats.rows_per_second = stats.total_rows/(stats.total_time>0?stats.total_time:1);

        QMetaObject::invokeMethod(this,"onPhaseUpdate",Q_ARG(int,0),
            Q_ARG(int,(int)PS_DONE),
            Q_ARG(QString,QString("Data Loading\n✓ Done in %1s").arg(loadT,0,'f',2)));
        QMetaObject::invokeMethod(this,"onPhaseUpdate",Q_ARG(int,1),
            Q_ARG(int,(int)PS_DONE),
            Q_ARG(QString,QString("Preprocessing\n✓ Done in %1s").arg(prepT,0,'f',2)));
        QMetaObject::invokeMethod(this,"onPhaseUpdate",Q_ARG(int,2),
            Q_ARG(int,(int)PS_DONE),
            Q_ARG(QString,QString("MPI Inference\n✓ Done in %1s").arg(stats.inference_time,0,'f',2)));

        QMetaObject::invokeMethod(this,"onLiveStatsUpdate",
            Q_ARG(qlonglong,(qlonglong)stats.total_rows),
            Q_ARG(qlonglong,(qlonglong)stats.total_attacks));
        QMetaObject::invokeMethod(this,"showFinalStats",
            Q_ARG(ProcessingStats,stats));
    });
    controller.detach();
}

// ============================================================================
// LOG → WORKER CARD PARSER
// ============================================================================
void MainWindow::updateLog(const QString& msg) { parseWorkerLog(msg); }

void MainWindow::parseWorkerLog(const QString& msg)
{
    QRegularExpression re(R"(\[WORKER (\d+)\] (.+))");
    auto m=re.match(msg);
    if(!m.hasMatch()) return;
    int rank=m.captured(1).toInt();
    QString text=m.captured(2).trimmed();
    int idx=rank-1;
    if(idx<0||idx>=workerCards.size()) return;

    if(text.contains("model loaded",Qt::CaseInsensitive)||
       text.contains("Engine online",Qt::CaseInsensitive)){
        setWorkerCardState(idx,WS_LOADING);
        workerStatusLabels[idx]->setText("Model Loaded");
        workerStatLabels[idx]->setText("XGBoost ready");
    } else if(text.contains("Waiting for next job",Qt::CaseInsensitive)){
        setWorkerCardState(idx,WS_IDLE);
        workerStatusLabels[idx]->setText("Waiting...");
        workerStatLabels[idx]->setText("—");
        if(m_runInProgress){
            setWorkerCardState(idx,WS_ACTIVE);
            workerStatusLabels[idx]->setText("Processing data...");
            workerStatLabels[idx]->setText("Running XGBoost");
        }
    } else if(text.contains("Job finished",Qt::CaseInsensitive)||
              text.contains("Resetting for next run",Qt::CaseInsensitive)){
        int numW=m_mpi_size-1;
        setWorkerCardState(idx,WS_DONE);
        workerStatusLabels[idx]->setText("Job Complete");
        workerStatLabels[idx]->setText(
            QString("~%1 rows inferred")
            .arg(numW>0?QString::number(rowsCountLabel->text().toLongLong()/numW):"?"));
    } else if(text.contains("CRITICAL",Qt::CaseInsensitive)||
              text.contains("failed",Qt::CaseInsensitive)){
        setWorkerCardState(idx,WS_ERROR);
        workerStatusLabels[idx]->setText("Error!");
        workerStatLabels[idx]->setText(text.left(28));
    } else if(text.contains("Shutting down",Qt::CaseInsensitive)){
        setWorkerCardState(idx,WS_DONE);
        workerStatusLabels[idx]->setText("Offline");
        workerStatLabels[idx]->setText("Graceful exit");
    }
}

// ============================================================================
// ALERT
// ============================================================================
void MainWindow::showAlert(const QString&)
{
    if(!m_hasAlerted){ alertBanner->show(); m_hasAlerted=true; }
}

// ============================================================================
// WORKER CARD STATE + ANIMATION CONTROL (Feature 7)
// ============================================================================
void MainWindow::setWorkerCardState(int idx, int state)
{
    if(idx<0||idx>=workerCards.size()) return;
    struct V{ const char* bg; const char* border; const char* icon; const char* col; };
    static const V vis[]={
        {C_CARD_IDLE, C_BORDER,  "⬜",C_MUTED },
        {C_CARD_LOAD, "#4c5fd5", "🔵","#93c5fd"},
        {C_CARD_ACTIVE,C_CYAN,   "🟢",C_CYAN  },
        {C_CARD_DONE, C_GREEN,   "✅",C_GREEN },
        {C_CARD_ERROR,C_RED,     "🔴",C_RED   },
    };
    if(state<0||state>4) return;
    const auto& v=vis[state];
    workerCards[idx]->setStyleSheet(
        QString("QFrame{background:%1;border:2px solid %2;border-radius:8px;}").arg(v.bg).arg(v.border));
    workerIconLabels[idx]->setText(v.icon);
    workerRankLabels[idx]->setStyleSheet(
        QString("color:%1;font-size:8pt;font-weight:bold;background:transparent;").arg(v.col));
    workerStatusLabels[idx]->setStyleSheet(
        QString("color:%1;font-size:7pt;background:transparent;").arg(v.col));
    workerStatLabels[idx]->setStyleSheet(
        QString("color:%1;font-size:6pt;background:transparent;").arg(
            state==WS_ACTIVE?C_CYAN:C_MUTED));
    workerStates[idx]=state;

    // Manage animation timer
    bool anyActive=false;
    for(int s:workerStates) if(s==WS_ACTIVE){anyActive=true;break;}
    if(anyActive && !m_animTimer->isActive()) m_animTimer->start();
    if(!anyActive && m_animTimer->isActive()) m_animTimer->stop();
}

// ============================================================================
// ANIMATION TICK (Feature 7) — pulses border of ACTIVE workers
// ============================================================================
void MainWindow::onAnimTick()
{
    m_animPhase=!m_animPhase;
    const char* border1=C_CYAN;
    const char* border2="#1a8fa8"; // dimmer cyan
    for(int i=0;i<workerCards.size();++i){
        if(workerStates[i]==WS_ACTIVE){
            workerCards[i]->setStyleSheet(
                QString("QFrame{background:%1;border:2px solid %2;border-radius:8px;}")
                .arg(C_CARD_ACTIVE)
                .arg(m_animPhase?border1:border2));
        }
    }
}

// ============================================================================
// HISTORY TOGGLE (Feature 2)
// ============================================================================
void MainWindow::onHistoryToggle()
{
    m_historyVisible=!m_historyVisible;
    historyContent->setVisible(m_historyVisible);
    historyToggleBtn->setText(
        m_historyVisible ? "▼  RUN HISTORY  (last 5 runs)" : "▶  RUN HISTORY  (last 5 runs)");
}

// ============================================================================
// RICH UI SLOTS
// ============================================================================
void MainWindow::onWorkerStateChanged(int rank, int state, const QString& label)
{
    setWorkerCardState(rank,state);
    if(rank>=0&&rank<workerStatusLabels.size()) workerStatusLabels[rank]->setText(label);
}

void MainWindow::onPhaseUpdate(int phase, int state, const QString& label)
{
    QLabel* ic=nullptr; QLabel* lb=nullptr;
    if(phase==0){ic=phaseLoadIcon;lb=phaseLoadLabel;}
    else if(phase==1){ic=phasePrepIcon;lb=phasePrepLabel;}
    else if(phase==2){ic=phaseInfIcon; lb=phaseInfLabel;}
    if(!ic||!lb) return;
    struct PV{const char* emoji;const char* col;};
    static const PV pv[]=
        {{"⬜",C_MUTED},{"🔄",C_ORANGE},{"✅",C_GREEN},{"❌",C_RED}};
    if(state<0||state>3) return;
    ic->setText(pv[state].emoji);
    ic->setStyleSheet(QString("color:%1;font-size:16pt;background:transparent;").arg(pv[state].col));
    lb->setText(label);
    lb->setStyleSheet(QString("color:%1;font-size:8pt;background:transparent;").arg(pv[state].col));
}

void MainWindow::onLiveStatsUpdate(qlonglong rows, qlonglong attacks)
{
    rowsCountLabel->setText(QString::number(rows));
    attacksCountLabel->setText(QString::number(attacks));
}

// ============================================================================
// ATTACK GAUGE UPDATE (Feature 3)
// ============================================================================
void MainWindow::updateAttackGauge(long long rows, long long attacks)
{
    if(rows<=0){ attackPctLabel->setText("—"); return; }
    double pct=(double)attacks*100.0/rows;
    int val=(int)(pct*10); // range 0–1000
    attackGaugeBar->setValue(qMin(val,1000));

    QString color = (pct<5.0) ? C_GREEN : (pct<20.0) ? C_ORANGE : C_RED;
    attackGaugeBar->setStyleSheet(
        QString("QProgressBar{background:%1;border:1px solid %2;border-radius:4px;}"
                "QProgressBar::chunk{background:%3;border-radius:4px;}")
        .arg(C_SURFACE2).arg(C_BORDER).arg(color));
    attackPctLabel->setText(QString("%1%").arg(pct,0,'f',2));
    attackPctLabel->setStyleSheet(
        QString("color:%1;font-size:8pt;font-weight:bold;background:transparent;").arg(color));
}

// ============================================================================
// THROUGHPUT GANTT UPDATE (Feature R)
// ============================================================================
void MainWindow::updateThroughputViz(const ProcessingStats& s)
{
    double total=s.total_time>0?s.total_time:1.0;
    m_ganttWidth=tpLoadBar->parentWidget()
                 ? qMax(200, tpLoadBar->parentWidget()->width()-220) : 400;

    auto setBar=[&](QFrame* bar, QLabel* lbl,
                    double t, const QString& name, const QString& col){
        int w=qMax(4,(int)(t/total*m_ganttWidth));
        bar->setFixedWidth(w);
        bar->setStyleSheet(
            QString("QFrame{background:%1;border-radius:4px;border:none;}").arg(col));
        lbl->setText(QString("%1   %2 s  (%3%)")
            .arg(name,-20).arg(t,0,'f',3).arg(t/total*100,0,'f',1));
        lbl->setStyleSheet(
            QString("color:%1;font-size:8pt;background:transparent;").arg(col));
    };
    setBar(tpLoadBar,tpLoadLabel,s.load_time,      "Data Loading",  C_ORANGE);
    setBar(tpPrepBar,tpPrepLabel,s.preprocess_time,"Preprocessing","#4c8af5");
    setBar(tpInfBar, tpInfLabel, s.inference_time, "MPI Inference", C_CYAN);
    throughputPanel->show();
}

// ============================================================================
// RUN HISTORY (Feature 2)
// ============================================================================
void MainWindow::pushRunHistory(const ProcessingStats& s, const QString& file)
{
    RunRecord r;
    r.dateTime    = QDateTime::currentDateTime().toString("MM/dd hh:mm:ss");
    r.fileName    = file.isEmpty()?"—":file;
    r.totalRows   = s.total_rows;
    r.totalAttacks= s.total_attacks;
    r.totalTime   = s.total_time;
    r.rowsPerSecond=s.rows_per_second;
    m_runHistory.prepend(r);
    if(m_runHistory.size()>5) m_runHistory.removeLast();
    refreshHistoryTable();
}

void MainWindow::refreshHistoryTable()
{
    for(int r=0;r<5;++r){
        auto* lbl0=histHistLabels[r*6+0];
        auto* lbl1=histHistLabels[r*6+1];
        auto* lbl2=histHistLabels[r*6+2];
        auto* lbl3=histHistLabels[r*6+3];
        auto* lbl4=histHistLabels[r*6+4];
        auto* lbl5=histHistLabels[r*6+5];
        if(r<m_runHistory.size()){
            const auto& rec=m_runHistory[r];
            QString col=(r==0)?C_TEXT:C_MUTED;
            auto apply=[&](QLabel* l,const QString& t){
                l->setText(t);
                l->setStyleSheet(QString("color:%1;font-size:8pt;background:transparent;").arg(col));
            };
            apply(lbl0,rec.dateTime);
            apply(lbl1,rec.fileName.left(16));
            apply(lbl2,QString::number(rec.totalRows));
            apply(lbl3,QString::number(rec.totalAttacks));
            apply(lbl4,QString::number(rec.totalTime,'f',3));
            apply(lbl5,QString::number(rec.rowsPerSecond,'f',0));
        } else {
            for(auto* l:{lbl0,lbl1,lbl2,lbl3,lbl4,lbl5})
                l->setText("—");
        }
    }
}

// ============================================================================
// FINAL STATS (calls all new update methods)
// ============================================================================
void MainWindow::showFinalStats(ProcessingStats stats)
{
    m_runInProgress = false;
    m_lastSpeed     = stats.rows_per_second;
    m_animTimer->stop();

    // Finalise worker cards
    for(int i=0;i<workerCards.size();++i){
        if(workerStates[i]==WS_ACTIVE||workerStates[i]==WS_IDLE){
            int numW=m_mpi_size-1;
            setWorkerCardState(i,WS_DONE);
            workerStatusLabels[i]->setText("Job Complete");
            workerStatLabels[i]->setText(
                numW>0 ? QString("~%1 rows").arg(stats.total_rows/numW) : "—");
        }
    }

    rowsCountLabel->setText(QString::number(stats.total_rows));
    attacksCountLabel->setText(QString::number(stats.total_attacks));
    speedLabel->setText(QString::number(stats.rows_per_second,'f',0));

    updateAttackGauge(stats.total_rows, stats.total_attacks);
    updateThroughputViz(stats);
    pushRunHistory(stats, m_lastFile);

    // Update file info label with actual speed-based ETA next time
    if(!pathInput->text().isEmpty()) updateFileInfo(pathInput->text());

    // Build report HTML
    auto fmtN=[](long long v){
        QString s=QString::number(v);
        for(int i=s.length()-3;i>0;i-=3) s.insert(i,',');
        return s; };

    QString attackColor=stats.total_attacks>0?C_RED:C_GREEN;
    QString safe=stats.total_attacks>0
        ? QString("<b style='color:%1;'>⚠ %2 threats</b>").arg(C_RED).arg(fmtN(stats.total_attacks))
        : QString("<b style='color:%1;'>✓ No threats</b>").arg(C_GREEN);

    m_lastReportText=
        QString("Date: %1\nFile: %2\n\nRows Processed : %3\nAttacks Detected: %4\n"
                "Attack Rate     : %5%\n\nData Loading    : %6 s\n"
                "Preprocessing   : %7 s\nMPI Inference   : %8 s\n"
                "Total Time      : %9 s\nThroughput      : %10 rows/s\n"
                "Workers Used    : %11")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
        .arg(m_lastFile)
        .arg(fmtN(stats.total_rows))
        .arg(fmtN(stats.total_attacks))
        .arg(stats.total_rows>0?(double)stats.total_attacks*100.0/stats.total_rows:0,0,'f',2)
        .arg(stats.load_time,0,'f',3)
        .arg(stats.preprocess_time,0,'f',3)
        .arg(stats.inference_time,0,'f',3)
        .arg(stats.total_time,0,'f',3)
        .arg(stats.rows_per_second,0,'f',0)
        .arg(m_mpi_size-1);

    QString html=QString(R"(
<table width='100%%' cellspacing='0' cellpadding='0' style='border-collapse:collapse;'>
<tr><td colspan='4' style='padding-bottom:8px;'>
  <span style='color:%1;font-size:10pt;font-weight:bold;'>%2 rows processed</span>
  &nbsp;&nbsp;&nbsp;%3
</td></tr>
<tr>
  <td width='1px' style='background:%4; padding:0 1px 0 0;'>&nbsp;</td>
  <td width='48%%' style='padding:4px 10px;'>
    <span style='color:%5;font-size:8pt;'>⏱ PHASE TIMINGS</span><br/>
    <table width='100%%' cellspacing='2' cellpadding='0' style='margin-top:4px;'>
      <tr><td style='color:%5;font-size:8pt;'>Data Loading</td>
          <td align='right' style='color:%6;font-size:8pt;font-weight:bold;'>%7 s</td></tr>
      <tr><td style='color:%5;font-size:8pt;'>Preprocessing <small style='color:%8;'>(OpenMP)</small></td>
          <td align='right' style='color:%6;font-size:8pt;font-weight:bold;'>%9 s</td></tr>
      <tr><td style='color:%5;font-size:8pt;'>MPI Inference <small style='color:%8;'>(×%10 workers)</small></td>
          <td align='right' style='color:%6;font-size:8pt;font-weight:bold;'>%11 s</td></tr>
    </table>
  </td>
  <td width='8px'></td>
  <td width='48%%' style='padding:4px 10px;'>
    <span style='color:%5;font-size:8pt;'>🚀 PERFORMANCE</span><br/>
    <table width='100%%' cellspacing='2' cellpadding='0' style='margin-top:4px;'>
      <tr><td style='color:%5;font-size:8pt;'>Total Wall Time</td>
          <td align='right' style='color:%6;font-size:8pt;font-weight:bold;'>%12 s</td></tr>
      <tr><td style='color:%5;font-size:8pt;'>Throughput</td>
          <td align='right' style='color:%13;font-size:8pt;font-weight:bold;'>%14 rows/s</td></tr>
      <tr><td style='color:%5;font-size:8pt;'>Attack Rate</td>
          <td align='right' style='color:%15;font-size:8pt;font-weight:bold;'>%16%</td></tr>
    </table>
  </td>
</tr>
</table>)")
    .arg(C_CYAN)
    .arg(fmtN(stats.total_rows))
    .arg(safe)
    .arg(C_BORDER)
    .arg(C_MUTED).arg(C_TEXT)
    .arg(stats.load_time,0,'f',3)
    .arg(C_MUTED)
    .arg(stats.preprocess_time,0,'f',3)
    .arg(m_mpi_size-1)
    .arg(stats.inference_time,0,'f',3)
    .arg(stats.total_time,0,'f',3)
    .arg(C_GREEN)
    .arg(QString::number(stats.rows_per_second,'f',0))
    .arg(attackColor)
    .arg(stats.total_rows>0?(double)stats.total_attacks*100.0/stats.total_rows:0,0,'f',2);

    summaryContent->setText(html);
    summaryPanel->show();
    runButton->setEnabled(true);
}

// ============================================================================
// EXPORT REPORT (Feature 8)
// ============================================================================
void MainWindow::onExportReport()
{
    QString path=QFileDialog::getSaveFileName(
        this,"Save Report","nids_report.txt","Text Files (*.txt)");
    if(path.isEmpty()) return;
    QFile f(path);
    if(f.open(QIODevice::WriteOnly|QIODevice::Text)){
        QTextStream out(&f);
        out<<"===== Parallel NIDS Engine — Run Report =====\n\n";
        out<<m_lastReportText<<"\n";
        f.close();
    }
}

// ============================================================================
// DESTRUCTOR
// ============================================================================
MainWindow::~MainWindow()
{
    MPIEngine::broadcastKillSignal(m_mpi_size);
}