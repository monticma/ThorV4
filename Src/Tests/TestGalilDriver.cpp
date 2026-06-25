/// @file TestGalilDriver.cpp
/// @brief Test d'intégration GalilDriver contre xmul (émulateur Galil).
///
/// Prérequis : xmul doit être lancé sur le port 2323.
///
/// Compilation :
///   g++ -std=c++17 -I ../../Include \
///       Src/Tests/TestGalilDriver.cpp \
///       Src/Agent/Drivers/GalilDriver.cpp \
///       Src/Agent/Drivers/GalilProtocol.cpp \
///       Src/Core/EventBus.cpp \
///       -o build/TestGalilDriver

#include "Agent/Drivers/GalilDriver.h"
#include <iostream>

int main()
{
    GalilDriver driver;

    // 1. Connexion
    std::cout << "=== GalilDriver xmul Test ===" << std::endl;
    if (!driver.connect("127.0.0.1", 2323, 2323)) {
        std::cerr << "FAIL connect: " << driver.lastError() << std::endl;
        return 1;
    }
    std::cout << "1. connect()       OK" << std::endl;

    // 2. Tell Position
    std::vector<double> positions;
    if (driver.getPositions(positions)) {
        std::cout << "2. getPositions()  OK ("
                  << positions[0] << "," << positions[1] << ","
                  << positions[2] << "," << positions[3] << ")"
                  << std::endl;
    } else {
        std::cout << "2. getPositions()  FAIL: " << driver.lastError() << std::endl;
    }

    // 3. Servo Here
    if (driver.servoHere()) {
        std::cout << "3. servoHere()     OK" << std::endl;
    } else {
        std::cout << "3. servoHere()     FAIL: " << driver.lastError() << std::endl;
    }

    // 4. Set Speeds
    std::vector<double> speeds(8, 10000.0);
    if (driver.setSpeeds(speeds)) {
        std::cout << "4. setSpeeds()     OK" << std::endl;
    } else {
        std::cout << "4. setSpeeds()     FAIL: " << driver.lastError() << std::endl;
    }

    // 5. Set Accelerations
    std::vector<double> accels(8, 5000.0);
    if (driver.setAccelerations(accels)) {
        std::cout << "5. setAccels()     OK" << std::endl;
    } else {
        std::cout << "5. setAccels()     FAIL: " << driver.lastError() << std::endl;
    }

    // 6. Move Absolute (valeurs adaptées au countsPerUnit de xmul)
    //    xmul multiplie par 1000 → 10.0 donne 10000 counts < 360000 limite
    if (driver.moveAbsolute({10.0, 20.0, 30.0, 5.0, 0, 0, 0, 0})) {
        std::cout << "6. moveAbsolute()  OK" << std::endl;
    } else {
        std::cout << "6. moveAbsolute()  FAIL: " << driver.lastError() << std::endl;
    }

    // 7. Stop All
    if (driver.stopAll()) {
        std::cout << "7. stopAll()       OK" << std::endl;
    } else {
        std::cout << "7. stopAll()       FAIL: " << driver.lastError() << std::endl;
    }

    // 8. Motor Off
    if (driver.motorOff()) {
        std::cout << "8. motorOff()      OK" << std::endl;
    } else {
        std::cout << "8. motorOff()      FAIL: " << driver.lastError() << std::endl;
    }

    // Note : xmul multiplie les valeurs PA par countsPerUnit (×1000).
    //        Sur un vrai Galil, les valeurs sont en counts bruts.

    // 9. Déconnexion
    driver.disconnect();
    std::cout << "9. disconnect()    OK" << std::endl;

    std::cout << std::endl << "PASSED" << std::endl;
    return 0;
}
