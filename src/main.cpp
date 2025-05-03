#include <iostream>
#include <string>
#include <tuple>
#include "serializer.hpp"
#include "factorioAPI.hpp"

using namespace ComputerPlaysFactorio;

enum MyEnum : uint32_t {
    EZ
};

template<class T>
struct Test {
    int32_t a;
    bool b;
    MyEnum c;
    std::string d;
    std::vector<T> e;
    std::map<std::string, uint64_t> f;
    float g;
    double h;
    RequestDataless i;

    constexpr static auto properties = std::make_tuple(
        SerializerProperty("a", &Test<T>::a),
        SerializerProperty("b", &Test<T>::b),
        SerializerProperty("c", &Test<T>::c),
        SerializerProperty("d", &Test<T>::d),
        SerializerProperty("e", &Test<T>::e),
        SerializerProperty("f", &Test<T>::f),
        SerializerProperty("g", &Test<T>::g),
        SerializerProperty("h", &Test<T>::h),
        SerializerProperty("i", &Test<T>::i)
    );
};

int main() {
    Test<std::string> t;
    t.a = 23234;
    t.b = true;
    t.c = EZ;
    t.d = "Je suis un gros caca";
    t.e = {"a", "aepra", "ffff"};
    t.f = {{"era", 50}, {"eraerrrr", 23}};
    t.g = 20.32442323f;
    t.h = 9404.2308949344;
    t.i.id = 10;
    t.i.name = TEST_REQUEST_NAME;

    std::cout << 
        t.a << "; " <<
        t.b << "; " <<
        (int)t.c << "; " <<
        t.d << "; " <<
        t.e[0] << "; " <<
        t.e[1] << "; " <<
        t.e[2] << "; " <<
        (*t.f.begin()).first << ":" <<
        (*t.f.begin()).second << "; " <<
        (*++t.f.begin()).first << ":" <<
        (*++t.f.begin()).second << "; " <<
        t.g << "; " <<
        t.h << "; " <<
        t.i.id << "; " <<
        t.i.name << "; " << std::endl;

    auto json = ToJson(t);
    std::cout << "json: " << QJsonDocument(json).toJson().toStdString() << std::endl;

    Test<std::string> t_;
    FromJson(json, t_);
    std::cout << 
        t_.a << "; " <<
        t_.b << "; " <<
        (int)t_.c << "; " <<
        t_.d << "; " <<
        t_.e[0] << "; " <<
        t_.e[1] << "; " <<
        t_.e[2] << "; " <<
        (*t_.f.begin()).first << ":" <<
        (*t_.f.begin()).second << "; " <<
        (*++t_.f.begin()).first << ":" <<
        (*++t_.f.begin()).second << "; " <<
        t_.g << "; " <<
        t_.h << "; " <<
        t_.i.id << "; " <<
        t_.i.name << "; " << std::endl;
}

// #include "gui.hpp"
// #include "factorioAPI.hpp"
// #include "thread.hpp"

// using namespace ComputerPlaysFactorio;

// int main(int argc, char *argv[]) {
//     FactorioInstance::InitStatic();
//     ThreadPool::Start(4);

//     QApplication app(argc, argv);
//     MainWindow w;
//     w.show();
//     int status = app.exec();

//     ThreadPool::Stop();
//     return status;
// }