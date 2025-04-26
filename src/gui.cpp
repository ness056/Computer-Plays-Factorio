#include "gui.hpp"

namespace ComputerPlaysFactorio {

MainWindow::MainWindow() : m_graphicalInstance("Graphical", true) {
    setWindowTitle("Computer Plays Factorio");
    setMinimumSize(200, 200);

    QWidget *widget = new QWidget;
    setCentralWidget(widget);

    QHBoxLayout *layout = new QHBoxLayout;
    widget->setLayout(layout);

    CreateActions();
    CreateMenus();

    m_graphicalInstance.printStdout = true;
    m_graphicalInstance.SetTerminateCallback([this](FactorioInstance&, int) {
        FactorioTerminated();
    });
}

void MainWindow::FactorioTerminated() {
    m_actToggleFactorio->setText("Start Factorio");
}

void MainWindow::CreateActions() {
    m_actSetFactorioPath = new QAction("Select Factorio", this);
    m_actSetFactorioPath->setStatusTip("Select the factorio binary path.");
    connect(m_actSetFactorioPath, &QAction::triggered, this, &MainWindow::SetFactorioPath);
    
    m_actToggleFactorio = new QAction("Start Factorio", this);
    connect(m_actToggleFactorio, &QAction::triggered, this, &MainWindow::ToggleFactorio);
    
    m_actExit = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::ApplicationExit),"Exit", this);
    m_actExit->setShortcut(QKeySequence::Close);
    connect(m_actExit, &QAction::triggered, this, &MainWindow::close);
    
    m_actShowRequestSender = new QAction("Request Sender", this);
    m_actShowRequestSender->setCheckable(true);
    connect(m_actShowRequestSender, &QAction::triggered, this, &MainWindow::ShowRequestSender);
    
    m_actShowCommandLine = new QAction("Command Line", this);
    m_actShowCommandLine->setCheckable(true);
    connect(m_actShowCommandLine, &QAction::triggered, this, &MainWindow::ShowCommandLine);
    
    m_actAbout = new QAction("About", this);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::About);
}

void MainWindow::CreateMenus() {
    m_menuFile = menuBar()->addMenu("File");
    m_menuFile->addAction(m_actSetFactorioPath);
    m_menuFile->addAction(m_actToggleFactorio);
    m_menuFile->addSeparator();
    m_menuFile->addAction(m_actExit);

    m_menuView = menuBar()->addMenu("View");
    m_menuViewDebug = m_menuView->addMenu("Debug");
    m_menuViewDebug->addAction(m_actShowCommandLine);
    m_menuViewDebug->addAction(m_actShowRequestSender);

    m_menuHelp = menuBar()->addMenu("Help");
    m_menuHelp->addAction(m_actAbout);
}

void MainWindow::SetFactorioPath() {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (dialog.exec()) {
        auto fileNames = dialog.selectedFiles();
        const auto fileName = fileNames[0].toStdString();

        std::string message;
        if (FactorioInstance::IsFactorioPathValid(fileName, message)) {
            FactorioInstance::SetFactorioPath(fileName);
        } else {
            QMessageBox msgBox(QMessageBox::Warning, "Invalid path", QString::fromStdString(message));
            msgBox.exec();
        }
    }
}

void MainWindow::ToggleFactorio() {
    if (m_graphicalInstance.Running()) {
        m_graphicalInstance.Stop();
        m_actToggleFactorio->setText("Start Factorio");
    } else {
        std::string message;
        if (!m_graphicalInstance.Start(nullptr, &message)) {
            QMessageBox msgBox(QMessageBox::Warning, "Could not start Factorio", QString::fromStdString(message));
            msgBox.exec();
        } else {
            m_actToggleFactorio->setText("Stop Factorio");
        }
    }
}

void MainWindow::ShowCommandLine(bool checked) {
    std::cout << __LINE__ << " " << checked << std::endl;
}

void MainWindow::ShowRequestSender(bool checked) {
    std::cout << __LINE__ << " " << checked << std::endl;
}

void MainWindow::About() {
    std::cout << __LINE__ << std::endl;
}
}