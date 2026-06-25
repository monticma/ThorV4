#pragma once

#include <vector>
#include <string>

// Forward declaration — DHParameter est défini dans Components/Robot.h
struct DHParameter;

// =============================================================================
// DHKinematics — Solveur cinématique générique Denavit-Hartenberg
// =============================================================================
//
// Calcule la cinématique directe (FK) et inverse (IK) pour tout robot série
// défini par des paramètres DH.
//
// FK : angles joints → position cartésienne (x, y, z)
// IK : position cartésienne (x, y, z) → angles joints
//
// L'IK utilise un solveur itératif (Newton-Raphson avec amortissement
// Levenberg-Marquardt) qui gère les singularités.
//
// Usage :
//   DHKinematics kin;
//   kin.loadFromDhParams(robot.kinematics.dhParameters);
//
//   std::vector<double> joints = {0.5, 1.2, 50.0};
//   double x, y, z;
//   kin.forwardKinematics(joints, x, y, z);
//
//   std::vector<double> solution;
//   if (kin.inverseKinematics(100.0, 200.0, 50.0, solution)) {
//       // solution contient les angles joints
//   }
// =============================================================================

/// @brief Solveur cinématique générique basé sur les paramètres DH.
class DHKinematics
{
public:
    DHKinematics();

    /// @brief Charge les paramètres DH (depuis le JSON du composant Robot).
    /// @param dhParams Liste des paramètres DH.
    /// @return true si le chargement a réussi.
    bool loadFromDhParams(const std::vector<DHParameter>& dhParams);

    /// @brief Cinématique directe : angles joints → position cartésienne.
    /// @param jointValues Angles joints (radians pour les révolutes, mm pour les prismatiques).
    /// @param xOut [out] Position X (mm).
    /// @param yOut [out] Position Y (mm).
    /// @param zOut [out] Position Z (mm).
    void forwardKinematics(const std::vector<double>& jointValues,
                           double& xOut, double& yOut, double& zOut);

    /// @brief Cinématique inverse : position cartésienne → angles joints.
    ///
    /// Utilise Newton-Raphson avec amortissement Levenberg-Marquardt.
    /// Pour le robot ATM-100 frog-leg, la solution analytique est utilisée
    /// (plus rapide et plus stable que la méthode numérique générique).
    ///
    /// @param x,y,z Position cartésienne cible (mm).
    /// @param jointValuesOut [out] Angles joints calculés.
    /// @return true si une solution a été trouvée.
    bool inverseKinematics(double x, double y, double z,
                           std::vector<double>& jointValuesOut);

    /// @brief Dernier message d'erreur.
    std::string lastError() const;

    /// @brief Nombre de joints (DH parameters chargés).
    int jointCount() const;

private:
    std::vector<DHParameter> mDhParams;
    std::string mLastError;

    // Paramètres IK
    static const int    mMaxIterations = 100;
    static const double mTolerance;       // 0.001 mm
    static const double mLambdaInitial;   // 0.01 (Levenberg-Marquardt)

    /// @brief Calcule la matrice de transformation homogène pour un joint.
    /// @param dh    Paramètres DH du joint.
    /// @param value Valeur du joint (θ pour révolute, d pour prismatique).
    /// @param matrixOut [out] Matrice 4×4 (16 doubles, row-major).
    void computeTransform(const DHParameter& dh, double value,
                          double matrixOut[16]);

    /// @brief Multiplie deux matrices 4×4 : C = A × B.
    static void multiplyMatrix(const double a[16], const double b[16],
                               double result[16]);

    /// @brief Calcule l'erreur entre la position FK courante et la cible.
    static double computeError(double x, double y, double z,
                               double targetX, double targetY, double targetZ);

    /// @brief IK numérique générique (Newton-Raphson + Levenberg-Marquardt).
    bool solveIKNumeric(double targetX, double targetY, double targetZ,
                        std::vector<double>& jointValuesOut);

    /// @brief IK analytique pour robot SCARA 2R + P (frog-leg ATM-100).
    bool solveIKScara(double targetX, double targetY, double targetZ,
                      std::vector<double>& jointValuesOut);
};
