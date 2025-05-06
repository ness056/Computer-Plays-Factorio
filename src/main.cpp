#include "gui.hpp"
#include "factorioAPI.hpp"
#include "thread.hpp"

using namespace ComputerPlaysFactorio;

int main(int argc, char *argv[]) {
    ThreadPool::Start(4);
    FactorioInstance::InitStatic();

    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    int status = app.exec();

    ThreadPool::Stop();
    return status;
}