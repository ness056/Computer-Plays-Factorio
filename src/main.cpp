#include <iostream>

#include "factorioAPI.hpp"

#include <string>
#include <tuple>
#include "serializer.hpp"

using namespace ComputerPlaysFactorio;

struct Test {
    int32_t a;
    int64_t b;
    signed char c;
    std::string d;
    unsigned short e[4];

    constexpr static auto properties = MakeSerializerProperties(
        &Test::a, &Test::b, &Test::c, &Test::d, &Test::e
    );
};

int main() {
    Request<Test> t;
    t.name = TEST_REQUEST_NAME;
    t.id = 32;
    t.data.a = 23234;
    t.data.b = -23333333333333333;
    t.data.c = -100;
    t.data.d = "Je suis un gros caca";
    t.data.e[0] = 1;
    t.data.e[1] = 2;
    t.data.e[2] = 3;
    t.data.e[3] = 4;
    std::cout << "test" << std::endl;
    std::cout << t.*(std::get<0>(Request<Test>::properties).member) << std::endl;
    std::cout << t.*(std::get<1>(Request<Test>::properties).member) << std::endl;
    std::cout << (t.*(std::get<2>(Request<Test>::properties).member)).b << std::endl;
    std::cout << "test" << std::endl;

    std::cout << 
        (int)t.name << "; " <<
        t.id << "; " <<
        t.data.a << "; " <<
        t.data.b << "; " <<
        t.data.c << "; " <<
        t.data.d << "; " <<
        t.data.e[0] << "; " <<
        t.data.e[1] << "; " <<
        t.data.e[2] << "; " <<
        t.data.e[3] << std::endl;
    std::string s;
    Serialize(t, s);
    std::cout << "s: " << s << std::endl;
    
    Request<Test> t_;
    size_t i = 0;
    Deserialize(s, i, t_);

    std::cout << 
        (int)t_.name << "; " <<
        t_.id << "; " <<
        t_.data.a << "; " <<
        t_.data.b << "; " <<
        t_.data.c << "; " <<
        t_.data.d << "; " <<
        t_.data.e[0] << "; " <<
        t_.data.e[1] << "; " <<
        t_.data.e[2] << "; " <<
        t_.data.e[3] << std::endl;

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