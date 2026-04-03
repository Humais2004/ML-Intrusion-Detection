#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QFrame>
#include <QList>
#include <QScrollArea>
#include "../common/packet_types.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(int mpi_size, QWidget* parent = nullptr);
    ~MainWindow();

signals:
    // Core signals (kept — used by mpi_engine.cpp via invokeMethod)
    void logMessage(const QString& msg);
    void intrusionAlert(const QString& attackType);

    // Rich UI signals (used from UI thread via emit in onRunClicked resets)
    void workerStateChanged(int rank, int state, const QString& label);
    void phaseUpdate(int phase, int state, const QString& label);
    void liveStatsUpdate(qlonglong rows, qlonglong attacks);

private slots:
    void onSelectFile();
    void onRunClicked();

    // Kept for mpi_engine.cpp invokeMethod calls
    void updateLog(const QString& msg);
    void showAlert(const QString& attackType);
    void showFinalStats(ProcessingStats stats);

    // Rich UI slots (called via invokeMethod from background thread)
    void onWorkerStateChanged(int rank, int state, const QString& label);
    void onPhaseUpdate(int phase, int state, const QString& label);
    void onLiveStatsUpdate(qlonglong rows, qlonglong attacks);

private:
    void buildUI();
    void applyDarkTheme();
    void buildWorkerGrid();
    void buildPhasePanel();   // stub — content inline in buildUI
    void buildStatsPanel();
    void buildControlPanel();
    void parseWorkerLog(const QString& msg);
    void setWorkerCardState(int cardIdx, int state);

    int  m_mpi_size;
    int  m_batchSize;         // stored at run start, used for stat estimation
    bool m_hasAlerted;
    bool m_runInProgress;     // true while a run is active

    // --- Control Panel ---
    QLineEdit*   pathInput;
    QLineEdit*   batchInput;
    QPushButton* runButton;
    QPushButton* browseButton;

    // --- Worker Thread Cards ---
    QFrame*           workerGridFrame;
    QList<QFrame*>    workerCards;
    QList<QLabel*>    workerIconLabels;    // emoji icon
    QList<QLabel*>    workerRankLabels;    // "W1", "W2", etc.
    QList<QLabel*>    workerStatusLabels;  // "Idle", "Processing...", etc.
    QList<QLabel*>    workerStatLabels;    // 4th line: rows processed / batch count
    QList<int>        workerStates;
    QList<int>        workerBatchCounts;   // batches processed per worker (estimated)

    // --- Phase Status Panel ---
    QLabel* phaseLoadIcon;
    QLabel* phaseLoadLabel;
    QLabel* phasePrepIcon;
    QLabel* phasePrepLabel;
    QLabel* phaseInfIcon;
    QLabel* phaseInfLabel;

    // --- Live Stats ---
    QLabel* rowsCountLabel;
    QLabel* attacksCountLabel;
    QLabel* speedLabel;

    // --- Alert Banner ---
    QFrame* alertBanner;
    QLabel* alertLabel;

    // --- Summary Panel ---
    QFrame* summaryPanel;
    QLabel* summaryTitle;
    QLabel* summaryContent;

    // Worker state enum
    enum WorkerState { WS_IDLE = 0, WS_LOADING = 1, WS_ACTIVE = 2, WS_DONE = 3, WS_ERROR = 4 };

    // Phase state enum
    enum PhaseState { PS_IDLE = 0, PS_RUNNING = 1, PS_DONE = 2, PS_ERROR = 3 };
};

#endif // MAINWINDOW_H