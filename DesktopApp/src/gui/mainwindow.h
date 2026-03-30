#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // Constructor accepts the world_size from MPI
    explicit MainWindow(int world_size, QWidget* parent = nullptr);
    ~MainWindow();

private:
    int m_world_size;
};

#endif // MAINWINDOW_H