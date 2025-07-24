#include <cpptrace/cpptrace.hpp>

#include "gui.hpp"
#include "../utils/thread.hpp"
#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    MainWindow::MainWindow() {
        CreateLogFile();
        CreateProjectsDir();

        std::set_terminate([]() noexcept {
            try {
                auto ptr = std::current_exception();
                if (!ptr) {
                    Error("Terminate called without exception");
                    ShowStacktrace();
                } else {
                    std::rethrow_exception(ptr);
                }
            } catch (const std::exception &e) {
                Error("Exception of type {}: {}",
                    cpptrace::demangle(typeid(e).name()), e.what());
                ShowStacktrace();
            } catch (...) {
                Error("Unknown exception");
                ShowStacktrace();
            }

            MainWindow::Clean();
            abort();
        });

        ThreadPool::Start(4);

        setWindowTitle("Computer Plays Factorio");
        setMinimumSize(200, 200);
        resize(800, 600);

        CreateCentralWidget();
        CreateActions();
        CreateMenus();

        connect(&m_bot.GetFactorioInstance(), &FactorioInstance::Started, [this]() {
            m_actToggleFactorio->setText("Stop Factorio");
        });

        connect(&m_bot.GetFactorioInstance(), &FactorioInstance::Terminated, [this]() {
            m_actToggleFactorio->setText("Start Factorio");
        });
    }

    MainWindow::~MainWindow() {
        Clean();
    }

    void MainWindow::Clean() noexcept {
        try {
            Info("Cleaning up...");

            FactorioInstance::StopAll();
            ThreadPool::Stop();
            
            FactorioInstance::JoinAll();
            ThreadPool::Wait();
            
            ClearTempDirectory();
            
            CloseLogFile();
        } catch (const std::exception &e) {
            Error("Cleaning Failed: Exception of type {}: {}",
                cpptrace::demangle(typeid(e).name()), e.what());
            ShowStacktrace();
        } catch (...) {
            Error("Cleaning Failed: Unknown exception");
            ShowStacktrace();
        }
        
        std::cout << "Goodbye :)" << std::endl;
    }

    void MainWindow::CreateCentralWidget() {
        QPushButton *buttonTest = new QPushButton("Test");
        connect(buttonTest, &QPushButton::clicked, [this]() {
            // ThreadPool::QueueJob([this]{ m_bot.Test(); });
        });

        m_commandLineTab = new QWidget();
        m_requestSenderTab = new QWidget();

        m_tabWidget = new QTabWidget;
        m_tabWidget->addTab(m_commandLineTab, "Command Line");
        m_tabWidget->addTab(m_requestSenderTab, "Request Sender");
        m_tabWidget->setTabVisible(0, false);
        m_tabWidget->setTabVisible(1, false);

        QHBoxLayout *layout = new QHBoxLayout;
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

        m_actNewProject = new QAction("New Project", this);
        connect(m_actNewProject, &QAction::triggered, this, &MainWindow::NewProject);

        m_actOpenProject = new QAction("Open Project", this);
        connect(m_actOpenProject, &QAction::triggered, this, &MainWindow::OpenProject);

        m_actRunProject = new QAction("Run Project", this);
        connect(m_actRunProject, &QAction::triggered, this, &MainWindow::RunProject);
        
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
        m_menuFile->addAction(m_actNewProject);
        m_menuFile->addAction(m_actOpenProject);
        m_menuFile->addAction(m_actRunProject);
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
        if (m_bot.GetFactorioInstance().Running()) {
            m_bot.Stop();
        } else {
            std::string message;
            if (!m_bot.Start(&message)) {
                QMessageBox msgBox(QMessageBox::Warning, "Could not start Factorio", QString::fromStdString(message));
                msgBox.exec();
            }
        }
    }

    void MainWindow::NewProject() {
        QString qName = "New-Project";
        auto qPath = QDir(GetDefaultProjectDir());

        QDialog dialog(this);
        dialog.setWindowTitle("New Project");
        dialog.resize(600, 200);
        QVBoxLayout *layout = new QVBoxLayout();
        dialog.setLayout(layout);

        auto nameLayout = new QHBoxLayout();
        layout->addLayout(nameLayout);
        auto nameLabel = new QLabel("Project Name: ");
        auto nameLineEdit = new QLineEdit(qName);
        nameLayout->addWidget(nameLabel);
        nameLayout->addWidget(nameLineEdit);

        dialog.connect(nameLineEdit, &QLineEdit::textEdited, [&qName](const QString &text) {
            qName = text;
        });

        auto pathLayout = new QHBoxLayout();
        layout->addLayout(pathLayout);
        auto pathLabel = new QLabel("Path: ");
        auto pathLineEdit = new QLineEdit(qPath.absolutePath());
        auto pathButton = new QPushButton("Browse");
        pathLayout->addWidget(pathLabel);
        pathLayout->addWidget(pathLineEdit);
        pathLayout->addWidget(pathButton);

        dialog.connect(pathLineEdit, &QLineEdit::textEdited, [&qPath](const QString &text) {
            qPath = QDir(text);
        });

        dialog.connect(pathButton, &QPushButton::clicked, [&qPath, pathLineEdit] {
            QFileDialog fileDialog;
            fileDialog.setFileMode(QFileDialog::Directory);
            if (fileDialog.exec()) {
                qPath = fileDialog.directory();
                pathLineEdit->setText(qPath.absolutePath());
            }
        });

        auto completePathLabel = new QLabel();
        completePathLabel->setMaximumWidth(500);
        completePathLabel->setAlignment(Qt::AlignTop);
        completePathLabel->setWordWrap(true);
        layout->addWidget(completePathLabel);

        auto updateCompletePathLabel = [completePathLabel, &qName, &qPath] {
            completePathLabel->setText("The project will be created in: " + qPath.absolutePath() + '/' + qName + '/');
        };
        dialog.connect(nameLineEdit, &QLineEdit::textChanged, updateCompletePathLabel);
        dialog.connect(pathLineEdit, &QLineEdit::textChanged, updateCompletePathLabel);
        updateCompletePathLabel();

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttonBox);
        auto ok = buttonBox->button(QDialogButtonBox::Ok);
        ok->setDefault(true);
        dialog.connect(ok, &QPushButton::clicked, [&dialog, &qPath, &qName] {
            auto path = qPath.filesystemAbsolutePath();
            if (!std::filesystem::is_directory(path)) {
                QMessageBox msgBox(QMessageBox::Warning, "Failed", "The given directory does not exist.");
                msgBox.exec();
            } else if (std::filesystem::is_directory(path / qName.toStdString())) {
                QMessageBox msgBox(QMessageBox::Warning, "Failed", "A directory named " + qName + " already exists.");
                msgBox.exec();
            } else {
                dialog.accept();
            }
        });

        auto cancel = buttonBox->button(QDialogButtonBox::Cancel);
        dialog.connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);

        if (dialog.exec()) {
            auto name = qName.toStdString();
            auto path = qPath.filesystemAbsolutePath();

            m_projectDirectory = path / name;
            std::filesystem::create_directories(m_projectDirectory);

            std::ofstream projectFile(m_projectDirectory / "project.json");
            projectFile << std::format(
R"({{
    "name": "{}",
    "author": "",
    "version": "0.0.0",
    "description": ""
}})", name);
            projectFile.close();

            std::ofstream mainFile(m_projectDirectory / "main.lua");
            mainFile.close();
        }
    }

    void MainWindow::OpenProject() {
        QFileDialog dialog(this, "Open a project", QString::fromStdString(GetDefaultProjectDir().string()));
        dialog.setFileMode(QFileDialog::Directory);
        if (dialog.exec()) {
            const auto path = dialog.directory().filesystemPath();

            if (!std::filesystem::exists(path)) {
                QMessageBox msgBox(QMessageBox::Warning, "Invalid path", "You must select a directory.");
                msgBox.exec();
            } else if (!std::filesystem::exists(path / "main.lua")) {
                QMessageBox msgBox(QMessageBox::Warning, "Invalid path", "This directory is not a project");
                msgBox.exec();
            } else {
                m_projectDirectory = path;
            }
        }
    }

    void MainWindow::RunProject() {
        if (std::filesystem::exists(m_projectDirectory / "main.lua")) {
            m_bot.Exec(m_projectDirectory / "main.lua");
        } else {
            QMessageBox msgBox(QMessageBox::Warning, "Error", "You must open or create a project");
            msgBox.exec();
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