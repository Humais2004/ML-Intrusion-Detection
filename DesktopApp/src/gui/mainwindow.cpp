#include "mainwindow.h"
#include <QLabel>

MainWindow::MainWindow(int world_size, QWidget* parent)
    : QMainWindow(parent), m_world_size(world_size)
{
    // Set a basic window size and title
    resize(800, 600);
    setWindowTitle("NIDS Distributed Engine - Master Node");

    // Just a temporary label to prove it works
    QLabel* label = new QLabel("GUI is Running. Workers are waiting in the background.", this);
    label->setGeometry(50, 50, 400, 50);
}

MainWindow::~MainWindow() {
}