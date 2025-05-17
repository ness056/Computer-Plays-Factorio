#include "gui.hpp"

namespace ComputerPlaysFactorio {

    MainWindow::MainWindow() {
        setWindowTitle("Computer Plays Factorio");
        setMinimumSize(200, 200);
        resize(800, 600);

        CreateCentralWidget();
        CreateActions();
        CreateMenus();

        connect(&m_playerController.GetFactorioInstance(), &FactorioInstance::Started, [this]() {
            m_actToggleFactorio->setText("Stop Factorio");
        });

        connect(&m_playerController.GetFactorioInstance(), &FactorioInstance::Terminated, [this]() {
            m_actToggleFactorio->setText("Start Factorio");
        });
    }

    MainWindow::~MainWindow() {
        FactorioInstance::StopAll();
        FactorioInstance::JoinAll();
        ClearTempDirectory();
    }

    void MainWindow::CreateCentralWidget() {
        static MapPosition walkPosition;

        QDoubleSpinBox *xBox = new QDoubleSpinBox;
        xBox->setRange(-1e3, 1e3);
        QDoubleSpinBox *yBox = new QDoubleSpinBox;
        yBox->setRange(-1e3, 1e3);
        connect(xBox, &QDoubleSpinBox::valueChanged, [](double x) { walkPosition.x = x; });
        connect(yBox, &QDoubleSpinBox::valueChanged, [](double y) { walkPosition.y = y; });

        QPushButton *buttonTest = new QPushButton("Walk");
        connect(buttonTest, &QPushButton::clicked, [this]() {
            m_playerController.QueueWalk(walkPosition, [](const ResponseDataless&) {
                g_info << "Walk done" << std::endl;
            });
        });
        
        m_commandLineTab = new QWidget();
        m_requestSenderTab = new QWidget();

        m_tabWidget = new QTabWidget;
        m_tabWidget->addTab(m_commandLineTab, "Command Line");
        m_tabWidget->addTab(m_requestSenderTab, "Request Sender");
        m_tabWidget->setTabVisible(0, false);
        m_tabWidget->setTabVisible(1, false);

        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(xBox);
        layout->addWidget(yBox);
        layout->addWidget(buttonTest);
        layout->addWidget(m_tabWidget);

        QWidget *widget = new QWidget;
        widget->setLayout(layout);
        setCentralWidget(widget);
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
        
        m_actShowCommandLine = new QAction("Command Line", this);
        m_actShowCommandLine->setCheckable(true);
        connect(m_actShowCommandLine, &QAction::triggered, this, &MainWindow::ShowCommandLine);
        
        m_actShowRequestSender = new QAction("Request Sender", this);
        m_actShowRequestSender->setCheckable(true);
        connect(m_actShowRequestSender, &QAction::triggered, this, &MainWindow::ShowRequestSender);
        
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
        if (m_playerController.GetFactorioInstance().Running()) {
            m_playerController.Stop();
        } else {
            std::string message;
            if (!m_playerController.Start(0, &message)) {
                QMessageBox msgBox(QMessageBox::Warning, "Could not start Factorio", QString::fromStdString(message));
                msgBox.exec();
            }
        }
    }

    void MainWindow::ShowCommandLine(bool checked) {
        m_tabWidget->setTabVisible(m_tabWidget->indexOf(m_commandLineTab), checked);
    }

    void MainWindow::ShowRequestSender(bool checked) {
        m_tabWidget->setTabVisible(m_tabWidget->indexOf(m_requestSenderTab), checked);
    }

    void MainWindow::About() {
        QMessageBox msgBox(QMessageBox::Information, "About", "TODO");
        msgBox.exec();
    }
}