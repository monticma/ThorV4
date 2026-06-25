/// @file TestDHKinematics.cpp
/// @brief Tests unitaires pour DHKinematics (FK/IK).
///
/// Compilation :
///   g++ -std=c++17 -I ../../Include \
///       Src/Tests/TestDHKinematics.cpp \
///       Src/Agent/Kinematics/DHKinematics.cpp \
///       -o build/TestDHKinematics

#include "Agent/Kinematics/DHKinematics.h"
#include "Agent/Components/Robot.h"
#include <iostream>
#include <cmath>
#include <cassert>

int main()
{
    // Paramètres DH de l'ATM-100 frog-leg SCARA
    std::vector<DHParameter> params(3);
    params[0].a = 107.95; params[0].alpha = 0.0; params[0].d = 0.0;
    params[0].type = "revolute";
    params[1].a = 107.95; params[1].alpha = 0.0; params[1].d = 0.0;
    params[1].type = "revolute";
    params[2].a = 0.0;    params[2].alpha = 0.0; params[2].d = 0.0;
    params[2].type = "prismatic";

    DHKinematics kin;
    assert(kin.loadFromDhParams(params));
    std::cout << "1. load OK (" << kin.jointCount() << " joints)" << std::endl;

    double x, y, z;

    // FK : bras tendu → x = a1+a2 = 215.9
    kin.forwardKinematics({0.0, 0.0, 0.0}, x, y, z);
    std::cout << "2. FK(0,0,0) = (" << x << "," << y << "," << z << ")" << std::endl;
    assert(std::abs(x - 215.9) < 0.01 && std::abs(y) < 0.01);

    // FK : rotation 90° → x=0, y=215.9
    kin.forwardKinematics({M_PI/2, 0.0, 50.0}, x, y, z);
    std::cout << "3. FK(π/2,0,50) = (" << x << "," << y << "," << z << ")" << std::endl;
    assert(std::abs(x) < 0.01 && std::abs(y - 215.9) < 0.01 && std::abs(z - 50.0) < 0.01);

    std::vector<double> joints;

    // IK : point au reach max → q0=0, q1=0
    assert(kin.inverseKinematics(215.9, 0.0, 0.0, joints));
    std::cout << "4. IK(215.9,0,0) = (" << joints[0] << "," << joints[1] << "," << joints[2] << ")" << std::endl;
    assert(std::abs(joints[0]) < 0.01 && std::abs(joints[1]) < 0.01);

    // IK : 90° → q0=π/2
    assert(kin.inverseKinematics(0.0, 215.9, 50.0, joints));
    std::cout << "5. IK(0,215.9,50) = (" << joints[0] << "," << joints[1] << "," << joints[2] << ")" << std::endl;
    assert(std::abs(joints[0] - M_PI/2) < 0.01 && std::abs(joints[2] - 50.0) < 0.01);

    // IK : hors de portée
    assert(!kin.inverseKinematics(1000.0, 0.0, 0.0, joints));
    std::cout << "6. Out of reach: " << kin.lastError() << std::endl;

    // Round-trip FK→IK→FK
    std::vector<double> orig = {0.5, 1.2, 30.0};
    kin.forwardKinematics(orig, x, y, z);
    assert(kin.inverseKinematics(x, y, z, joints));
    kin.forwardKinematics(joints, x, y, z);
    double dx = x - 107.95*(std::cos(orig[0])+std::cos(orig[0]+orig[1]));
    double dy = y - 107.95*(std::sin(orig[0])+std::sin(orig[0]+orig[1]));
    std::cout << "7. Round-trip error: (" << dx << "," << dy << ") < 1e-13" << std::endl;
    assert(std::abs(dx) < 1e-12);

    std::cout << std::endl << "ALL PASSED" << std::endl;
    return 0;
}
