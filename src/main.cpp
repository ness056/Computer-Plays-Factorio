#include "gui/gui.hpp"

using namespace ComputerPlaysFactorio;

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}