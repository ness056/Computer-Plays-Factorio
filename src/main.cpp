#include <iostream>

#include "factorioAPI.hpp"

#include <string>
#include <tuple>
#include "serializer.hpp"

using namespace ComputerPlaysFactorio;

enum MyEnum {
    EZ
};

template<class T>
struct Test {

    int32_t a;
    bool b;
    MyEnum c;
    unsigned short d[4];
    std::string e;
    std::vector<T> f;
    std::map<std::string, uint64_t> g;
    float h;
    double i;

    constexpr static auto properties = MakeSerializerProperties(
        &Test<T>::a, &Test<T>::b, &Test<T>::c, &Test<T>::d,
        &Test<T>::e, &Test<T>::f, &Test<T>::g, &Test<T>::h, &Test<T>::i
    );
};

int main() {
    Request<Test<std::string>> t;
    t.name = TEST_REQUEST_NAME;
    t.id = 32;
    t.data.a = 23234;
    t.data.b = true;
    t.data.c = EZ;
    t.data.d[0] = 1;
    t.data.d[1] = 2;
    t.data.d[2] = 3;
    t.data.d[3] = 4;
    t.data.e = "Je suis un gros caca";
    t.data.f = {"a", "aepra", "ffff"};
    t.data.g = {{"era", 50}, {"eraerrrr", 23}};
    t.data.h = 20.32442323f;
    t.data.i = 9404.2308949344;

    std::cout << 
        (int)t.name << "; " <<
        t.id << "; " <<
        t.data.a << "; " <<
        t.data.b << "; " <<
        (int)t.data.c << "; " <<
        t.data.d[0] << "; " <<
        t.data.d[1] << "; " <<
        t.data.d[2] << "; " <<
        t.data.d[3] << "; " <<
        t.data.e << "; " <<
        t.data.f[0] << "; " <<
        t.data.f[1] << "; " <<
        t.data.f[2] << "; " <<
        (*t.data.g.begin()).first << ":" <<
        (*t.data.g.begin()).second << "; " <<
        (*++t.data.g.begin()).first << ":" <<
        (*++t.data.g.begin()).second << "; " <<
        t.data.h << "; " <<
        t.data.i << "; " << std::endl;
    std::string s;
    Serialize(t, s);
    std::cout << "s: " << s << std::endl;
    
    Request<Test<std::string>> t_;
    size_t i = 0;
    bool success = Deserialize(s, i, t_);
    std::cout << success << std::endl;

    std::cout << 
        (int)t_.name << "; " <<
        t_.id << "; " <<
        t_.data.a << "; " <<
        t_.data.b << "; " <<
        (int)t_.data.c << "; " <<
        t_.data.d[0] << "; " <<
        t_.data.d[1] << "; " <<
        t_.data.d[2] << "; " <<
        t_.data.d[3] << "; " <<
        t_.data.e << "; " <<
        t_.data.f[0] << "; " <<
        t_.data.f[1] << "; " <<
        t_.data.f[2] << "; " <<
        (*t_.data.g.begin()).first << ":" <<
        (*t_.data.g.begin()).second << "; " <<
        (*++t_.data.g.begin()).first << ":" <<
        (*++t_.data.g.begin()).second << "; " <<
        t.data.h << "; " <<
        t.data.i << "; " << std::endl;

    // FactorioInstance::InitStatic();
    
    // FactorioInstance instance("name", true);
    // instance.printStdout = true;

    // instance.Start([](FactorioInstance &instance) {
    //     instance.SendRCON("testtest");
    //     instance.SendRequest(TEST_REQUEST_NAME, 20);
    // });


    // instance.Join();

    // return 0;
}