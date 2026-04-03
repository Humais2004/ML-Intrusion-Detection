#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QFrame>
#include <QList>
#include <QScrollArea>
#include <QProgressBar>
#include <QTimer>
#include <QDateTime>
#include <QShortcut>
#include <QGridLayout>
#include "../common/packet_types.h"

// ─── Run history record ──────────────────────────────────────────────────────
struct RunRecord {
    QString    dateTime;
    QString    fileName;
    long long  totalRows;
    long long  totalAttacks;
    double     totalTime;
    double     rowsPerSecond;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(int mpi_size, QWidget* parent = nullptr);
    ~MainWindow();

signals:
    void logMessage(const QString& msg);
    void intrusionAlert(const QString& attackType);
    void workerStateChanged(int rank, int state, const QString& label);
    void phaseUpdate(int phase, int state, const QString& label);
    void liveStatsUpdate(qlonglong rows, qlonglong attacks);

private slots:
    void onSelectFile();
    void onRunClicked();
    void updateLog(const QString& msg);
    void showAlert(const QString& attackType);
    void showFinalStats(ProcessingStats stats);
    void onWorkerStateChanged(int rank, int state, const QString& label);
    void onPhaseUpdate(int phase, int state, const QString& label);
    void onLiveStatsUpdate(qlonglong rows, qlonglong attacks);
    void onAnimTick();           // worker pulse animation
    void onHistoryToggle();      // expand/collapse run history
    void onExportReport();       // save report to file

private:
    // ── Build helpers ──────────────────────────────────────────────────────
    void buildUI();
    void applyDarkTheme();
    void buildControlPanel();
    void buildWorkerGrid();
    void buildPhasePanel();
    void buildStatsPanel();
    void buildThroughputPanel();
    void buildRunHistoryPanel();

    // ── Runtime helpers ────────────────────────────────────────────────────
    void parseWorkerLog(const QString& msg);
    void setWorkerCardState(int cardIdx, int state);
    void updateFileInfo(const QString& filePath);
    void updateAttackGauge(long long rows, long long attacks);
    void updateThroughputViz(const ProcessingStats& s);
    void pushRunHistory(const ProcessingStats& s, const QString& file);
    void refreshHistoryTable();

    // ── Core state ─────────────────────────────────────────────────────────
    int  m_mpi_size;
    int  m_batchSize;
    bool m_hasAlerted;
    bool m_runInProgress;
    bool m_animPhase;        // toggles each anim tick
    bool m_historyVisible;
    QString m_lastFile;
    double  m_lastSpeed;     // for ETA estimation
    QString m_lastReportText;  // for export

    // ── Run history ────────────────────────────────────────────────────────
    QList<RunRecord> m_runHistory;

    // ── Control Panel ──────────────────────────────────────────────────────
    QLineEdit*   pathInput;
    QLineEdit*   batchInput;
    QPushButton* runButton;
    QPushButton* browseButton;
    QLabel*      fileInfoLabel;  // dataset info row

    // ── Worker Cards ───────────────────────────────────────────────────────
    QFrame*           workerGridFrame;
    QList<QFrame*>    workerCards;
    QList<QLabel*>    workerIconLabels;
    QList<QLabel*>    workerRankLabels;
    QList<QLabel*>    workerStatusLabels;
    QList<QLabel*>    workerStatLabels;
    QList<int>        workerStates;
    QList<int>        workerBatchCounts;

    // ── Pipeline Phases ────────────────────────────────────────────────────
    QLabel* phaseLoadIcon;   QLabel* phaseLoadLabel;
    QLabel* phasePrepIcon;   QLabel* phasePrepLabel;
    QLabel* phaseInfIcon;    QLabel* phaseInfLabel;

    // ── Live Stats ─────────────────────────────────────────────────────────
    QLabel*       rowsCountLabel;
    QLabel*       attacksCountLabel;
    QLabel*       speedLabel;
    QProgressBar* attackGaugeBar;   // colored attack-rate bar
    QLabel*       attackPctLabel;   // "12.3% attack rate"

    // ── Alert Banner ───────────────────────────────────────────────────────
    QFrame* alertBanner;
    QLabel* alertLabel;

    // ── Summary Panel ──────────────────────────────────────────────────────
    QFrame*      summaryPanel;
    QLabel*      summaryTitle;
    QLabel*      summaryContent;
    QPushButton* exportButton;

    // ── Throughput Visualization (post-run Gantt) ───────────────────────────
    QFrame* throughputPanel;
    QFrame* tpLoadBar;   QLabel* tpLoadLabel;
    QFrame* tpPrepBar;   QLabel* tpPrepLabel;
    QFrame* tpInfBar;    QLabel* tpInfLabel;
    int     m_ganttWidth;  // full bar width in px

    // ── Run History Panel ─────────────────────────────────────────────────
    QFrame*      historyPanel;
    QWidget*     historyContent;
    QGridLayout* historyGrid;
    QPushButton* historyToggleBtn;
    // 5 rows × 6 columns of labels (date, file, rows, attacks, time, speed)
    QList<QLabel*> histHistLabels;

    // ── Animation timer ────────────────────────────────────────────────────
    QTimer* m_animTimer;

    // ── Enums ──────────────────────────────────────────────────────────────
    enum WorkerState { WS_IDLE=0, WS_LOADING=1, WS_ACTIVE=2, WS_DONE=3, WS_ERROR=4 };
    enum PhaseState  { PS_IDLE=0, PS_RUNNING=1, PS_DONE=2,   PS_ERROR=3 };
};

#endif // MAINWINDOW_H