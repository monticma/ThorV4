/// @file TestLuaEngine.cpp
/// @brief Tests unitaires pour LuaEngine.
///
/// Compilation :
///   g++ -std=c++17 -I ../../Include -I ../../Vendor/sol2 -I ../../Vendor/lua-5.4.7/src \
///       Src/Tests/TestLuaEngine.cpp Src/Agent/Scripting/LuaEngine.cpp \
///       build/Vendor/libthor_lua.a -o build/TestLuaEngine

#include "Agent/Scripting/LuaEngine.h"
#include <iostream>
#include <fstream>
#include <cassert>

int main()
{
    LuaEngine engine;

    // 1. Initialize
    assert(engine.initialize());
    std::cout << "1. initialize() OK" << std::endl;

    // 2. Run string
    assert(engine.runString("print('Hello from Lua!')"));
    std::cout << "2. runString() OK" << std::endl;

    // 3. thorLog
    assert(engine.runString("thorLog('INFO', 'Test message')"));
    std::cout << "3. thorLog() OK" << std::endl;

    // 4. Variables C++ ↔ Lua
    assert(engine.runString("x = 42"));
    assert(engine.state()["x"].get<int>() == 42);
    std::cout << "4. variables OK (x=42)" << std::endl;

    // 5. Math library (sandbox: math is allowed)
    assert(engine.runString("y = math.sin(0)"));
    assert(engine.state()["y"].get<double>() == 0.0);
    std::cout << "5. math OK (sin(0)=0)" << std::endl;

    // 6. Error handling
    assert(!engine.runString("thisDoesNotExist()"));
    std::cout << "6. error caught: " << engine.lastError().substr(0, 40) << "..." << std::endl;

    // 7. Sandbox: os is NOT available
    assert(!engine.runString("os.exit()"));
    std::cout << "7. sandbox OK (os blocked)" << std::endl;

    // 8. Sandbox: io is NOT available
    assert(!engine.runString("io.open('/etc/passwd')"));
    std::cout << "8. sandbox OK (io blocked)" << std::endl;

    // 9. Load script from file
    {
        std::ofstream f("/tmp/thor_test_script.lua");
        f << "print('File loaded!')\nanswer = 99\n";
        f.close();
        assert(engine.loadScriptFile("/tmp/thor_test_script.lua"));
        assert(engine.state()["answer"].get<int>() == 99);
        std::cout << "9. loadScriptFile() OK" << std::endl;
    }

    std::cout << std::endl << "ALL LuaEngine tests PASSED" << std::endl;
    return 0;
}
