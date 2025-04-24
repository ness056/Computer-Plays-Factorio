#include "mainWindow.hpp"

namespace ComputerPlaysFactorio {

MainWindow::MainWindow() {
    setWindowTitle("Computer Plays Factorio");
    
    auto menuBar = new QMenuBar(this);

    auto file = menuBar->addMenu("File");
    file->addAction("Select Factorio");
    file->addAction("Start Factorio");
    file->addSeparator();
    file->addAction("Exit");

    auto view = menuBar->addMenu("View");
    auto debug = view->addMenu("Debug");
    auto requestSender = debug->addAction("Request Sender");
    requestSender->setCheckable(true);

    auto central
}
}