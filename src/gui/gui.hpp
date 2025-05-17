#pragma once

#include <QtWidgets>

#include "../factorioAPI/playerController.hpp"

namespace ComputerPlaysFactorio {

    class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        MainWindow();
        ~MainWindow();

    private slots:
        void SetFactorioPath();
        void ToggleFactorio();
        void ShowCommandLine(bool checked);
        void ShowRequestSender(bool checked);
        void About();

    private:
        PlayerController m_playerController;

        void CreateCentralWidget();
        void CreateActions();
        void CreateMenus();

        QWidget *m_commandLineTab;
        QWidget *m_requestSenderTab;
        QTabWidget *m_tabWidget;

        QMenu *m_menuFile;
        QAction *m_actSetFactorioPath;
        QAction *m_actToggleFactorio;
        QAction *m_actExit;
        QMenu *m_menuView;
        QMenu *m_menuViewDebug;
        QAction *m_actShowRequestSender;
        QAction *m_actShowCommandLine;
        QMenu *m_menuHelp;
        QAction *m_actAbout;
    };
}