/// @file TestGalilProtocol.cpp
/// @brief Tests unitaires pour GalilProtocol (codec pur, pas de réseau).
///
/// Compilation :
///   g++ -std=c++17 -I ../../Include Src/Tests/TestGalilProtocol.cpp \
///       Src/Agent/Drivers/GalilProtocol.cpp -o build/TestGalilProtocol

#include "Agent/Drivers/GalilProtocol.h"
#include <iostream>
#include <cassert>

int main()
{
    // --- Encode ---
    std::string cmd = GalilProtocol::encodeMoveAbsolute({1000.0, 2000.0, 5000.0});
    std::cout << "encodeMoveAbsolute: " << cmd << std::endl;
    assert(cmd == "PAA=1000,B=2000,C=5000");

    cmd = GalilProtocol::encodeMoveRelative({100.0, -50.0});
    std::cout << "encodeMoveRelative: " << cmd << std::endl;
    assert(cmd == "PRA=100,B=-50");

    cmd = GalilProtocol::encodeBeginMotion({0, 1, 2});
    std::cout << "encodeBeginMotion:  " << cmd << std::endl;
    assert(cmd == "BGABC");

    cmd = GalilProtocol::encodeStopAll();
    std::cout << "encodeStopAll:      " << cmd << std::endl;
    assert(cmd == "ST");

    cmd = GalilProtocol::encodeMotorOff();
    std::cout << "encodeMotorOff:     " << cmd << std::endl;
    assert(cmd == "MO");

    cmd = GalilProtocol::encodeServoHere();
    std::cout << "encodeServoHere:    " << cmd << std::endl;
    assert(cmd == "SH");

    cmd = GalilProtocol::encodeHome(2);
    std::cout << "encodeHome(2):      " << cmd << std::endl;
    assert(cmd == "HMC");

    cmd = GalilProtocol::encodeTellPosition();
    std::cout << "encodeTellPosition: " << cmd << std::endl;
    assert(cmd == "TP");

    cmd = GalilProtocol::encodeSpeed({25000.0, 25000.0, 25000.0});
    std::cout << "encodeSpeed:        " << cmd << std::endl;
    assert(cmd == "SP25000,25000,25000");

    cmd = GalilProtocol::encodeAcceleration({10000.0, 10000.0});
    std::cout << "encodeAcceleration: " << cmd << std::endl;
    assert(cmd == "AC10000,10000");

    cmd = GalilProtocol::encodeAfterMotion({0, 1});
    std::cout << "encodeAfterMotion:  " << cmd << std::endl;
    assert(cmd == "AMAB");

    // --- Decode ---
    std::string payload;
    bool isError;

    bool ok = GalilProtocol::decodeReply("1234,5678:\r\n", payload, isError);
    std::cout << "decodeReply OK:     payload='" << payload << "' isError=" << isError << std::endl;
    assert(ok && !isError && payload == "1234,5678");

    ok = GalilProtocol::decodeReply("bad command?\r\n", payload, isError);
    std::cout << "decodeReply ERR:    payload='" << payload << "' isError=" << isError << std::endl;
    assert(ok && isError && payload == "bad command");

    ok = GalilProtocol::decodeReply("", payload, isError);
    std::cout << "decodeReply empty:  " << (ok ? "OK" : "FAIL (expected)") << std::endl;
    assert(!ok);

    // --- Parse ---
    std::vector<double> values;
    ok = GalilProtocol::parseAxisValues("1000,2000,5000", values, 3);
    std::cout << "parseAxisValues:    [0]=" << values[0] << " [1]=" << values[1] << " [2]=" << values[2] << std::endl;
    assert(ok && values.size() == 3 && values[0] == 1000.0);

    ok = GalilProtocol::parseAxisValues("1000,2000", values, 3);
    std::cout << "parseAxisValues(3): " << (ok ? "OK" : "FAIL (expected)") << std::endl;
    assert(!ok);

    values.clear();
    ok = GalilProtocol::parseAxisValues("", values, 0);
    std::cout << "parseAxisValues(''): " << (ok ? "OK" : "FAIL") << " size=" << values.size() << std::endl;
    assert(ok && values.empty());

    std::cout << std::endl << "ALL TESTS PASSED" << std::endl;
    return 0;
}
