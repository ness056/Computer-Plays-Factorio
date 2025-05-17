#include "gui/gui.hpp"
#include "utils/thread.hpp"

using namespace ComputerPlaysFactorio;

int main(int argc, char *argv[]) {
    ThreadPool::Start(4);

    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    int status = app.exec();

    ThreadPool::Stop();
    return status;
}