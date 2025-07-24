#pragma once

#include <QtWidgets>

#include "../bot/bot.hpp"

namespace ComputerPlaysFactorio {

    class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        MainWindow();
        ~MainWindow();

    private slots:
        void SetFactorioPath();
        void ToggleFactorio();
        void NewProject();
        void OpenProject();
        void RunProject();
        void ShowCommandLine(bool checked);
        void ShowRequestSender(bool checked);
        void About();

    private:
        static void Clean() noexcept;

        std::filesystem::path m_projectDirectory;
        Bot m_bot;

        void CreateCentralWidget();
        void CreateActions();
        void CreateMenus();

        QWidget *m_commandLineTab;
        QWidget *m_requestSenderTab;
        QTabWidget *m_tabWidget;

        QMenu *m_menuFile;
        QAction *m_actSetFactorioPath;
        QAction *m_actToggleFactorio;
        QAction *m_actNewProject;
        QAction *m_actOpenProject;
        QAction *m_actRunProject;
        QAction *m_actExit;
        QMenu *m_menuView;
        QMenu *m_menuViewDebug;
        QAction *m_actShowRequestSender;
        QAction *m_actShowCommandLine;
        QMenu *m_menuHelp;
        QAction *m_actAbout;
    };
}