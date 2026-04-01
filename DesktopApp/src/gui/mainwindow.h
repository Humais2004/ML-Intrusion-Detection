#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include "../common/packet_types.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(int mpi_size, QWidget* parent = nullptr);
    ~MainWindow();

signals:
    // Signals to update UI from background threads
    void logMessage(const QString& msg);
    void intrusionAlert(const QString& attackType);

private slots:
    void onSelectFile();
    void onRunClicked();
    void updateLog(const QString& msg);
    void showAlert(const QString& attackType);
    void showFinalStats(ProcessingStats stats);

private:
    int m_mpi_size;
    bool m_hasAlerted; // Prevents UI freeze by limiting alert popups

    // UI Components
    QLineEdit* pathInput;
    QLineEdit* batchInput;
    QTextEdit* logWindow;
    QPushButton* runButton;
    QLabel* statusLabel;
};

#endif // MAINWINDOW_H