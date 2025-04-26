#pragma once

#include <QtWidgets>

#include "factorioAPI.hpp"

namespace ComputerPlaysFactorio {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private slots:
    void SetFactorioPath();
    void ToggleFactorio();
    void ShowCommandLine(bool checked);
    void ShowRequestSender(bool checked);
    void About();

private:
    void FactorioTerminated();

    FactorioInstance m_graphicalInstance;

    void CreateActions();
    void CreateMenus();

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