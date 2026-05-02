#pragma once

namespace ComputerPlaysFactorio {
    
    class App {
    public:
        App();
        ~App();

        void Run();

    private:
        void SetTerminate();
        void LoadConfig();
    };
}