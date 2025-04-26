// #include <iostream>

// #include "factorioAPI.hpp"
// #include "thread.hpp"

// #include <string>
// #include <tuple>
// #include "serializer.hpp"

// using namespace ComputerPlaysFactorio;

// enum MyEnum : uint32_t {
//     EZ
// };

// template<class T>
// struct Test {

//     int32_t a;
//     bool b;
//     MyEnum c;
//     unsigned short d[4];
//     std::string e;
//     std::vector<T> f;
//     std::map<std::string, uint64_t> g;
//     float h;
//     double i;

//     constexpr static auto properties = MakeSerializerProperties(
//         &Test<T>::a, &Test<T>::b, &Test<T>::c, &Test<T>::d,
//         &Test<T>::e, &Test<T>::f, &Test<T>::g, &Test<T>::h, &Test<T>::i
//     );
// };

// int main() {
//     Test<std::string> t;
//     t.a = 23234;
//     t.b = true;
//     t.c = EZ;
//     t.d[0] = 1;
//     t.d[1] = 2;
//     t.d[2] = 3;
//     t.d[3] = 4;
//     t.e = "Je suis un gros caca";
//     t.f = {"a", "aepra", "ffff"};
//     t.g = {{"era", 50}, {"eraerrrr", 23}};
//     t.h = 20.32442323f;
//     t.i = 9404.2308949344;

//     std::cout << 
//         t.a << "; " <<
//         t.b << "; " <<
//         (int)t.c << "; " <<
//         t.d[0] << "; " <<
//         t.d[1] << "; " <<
//         t.d[2] << "; " <<
//         t.d[3] << "; " <<
//         t.e << "; " <<
//         t.f[0] << "; " <<
//         t.f[1] << "; " <<
//         t.f[2] << "; " <<
//         (*t.g.begin()).first << ":" <<
//         (*t.g.begin()).second << "; " <<
//         (*++t.g.begin()).first << ":" <<
//         (*++t.g.begin()).second << "; " <<
//         t.h << "; " <<
//         t.i << "; " << std::endl;
//     // std::string s;
//     // Serialize(t, s);
//     // std::cout << "s: " << s << std::endl;
    
//     // Request<Test<std::string>> t_;
//     // size_t i = 0;
//     // bool success = Deserialize(s, i, t_);
//     // std::cout << success << std::endl;

//     // std::cout << 
//     //     (int)t_.name << "; " <<
//     //     t_.id << "; " <<
//     //     t_.data.a << "; " <<
//     //     t_.data.b << "; " <<
//     //     (int)t_.data.c << "; " <<
//     //     t_.data.d[0] << "; " <<
//     //     t_.data.d[1] << "; " <<
//     //     t_.data.d[2] << "; " <<
//     //     t_.data.d[3] << "; " <<
//     //     t_.data.e << "; " <<
//     //     t_.data.f[0] << "; " <<
//     //     t_.data.f[1] << "; " <<
//     //     t_.data.f[2] << "; " <<
//     //     (*t_.data.g.begin()).first << ":" <<
//     //     (*t_.data.g.begin()).second << "; " <<
//     //     (*++t_.data.g.begin()).first << ":" <<
//     //     (*++t_.data.g.begin()).second << "; " <<
//     //     t.data.h << "; " <<
//     //     t.data.i << "; " << std::endl;

//     FactorioInstance::InitStatic();
//     ThreadPool::Start(4);
    
//     FactorioInstance instance("name", true);
//     instance.printStdout = true;

//     instance.Start([t](FactorioInstance &instance) {
//         instance.SendRequest(TEST_REQUEST_NAME, t);
//     });

//     instance.Join();

//     return 0;
// }

#include "gui.hpp"
#include "factorioAPI.hpp"
#include "thread.hpp"

using namespace ComputerPlaysFactorio;

int main(int argc, char *argv[]) {
    FactorioInstance::InitStatic();
    ThreadPool::Start(4);

    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    int status = app.exec();

    ThreadPool::Stop();
    return status;
}