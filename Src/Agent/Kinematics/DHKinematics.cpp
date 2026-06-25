#include "Agent/Kinematics/DHKinematics.h"
#include "Agent/Components/Robot.h"  // pour DHParameter

#include <cmath>
#include <cstring>
#include <iostream>

// =============================================================================
// Constantes
// =============================================================================

const double DHKinematics::mTolerance    = 0.001;   // 1 micron
const double DHKinematics::mLambdaInitial = 0.01;

// =============================================================================
// DHKinematics
// =============================================================================

DHKinematics::DHKinematics()
{
}

bool DHKinematics::loadFromDhParams(const std::vector<DHParameter>& dhParams)
{
    if (dhParams.empty()) {
        mLastError = "DHKinematics: empty DH parameters";
        return false;
    }

    mDhParams = dhParams;
    return true;
}

int DHKinematics::jointCount() const
{
    return static_cast<int>(mDhParams.size());
}

std::string DHKinematics::lastError() const
{
    return mLastError;
}

// =============================================================================
// Transform matrix
// =============================================================================

void DHKinematics::computeTransform(const DHParameter& dh, double value,
                                     double matrixOut[16])
{
    // T = [cosθ  -sinθ·cosα   sinθ·sinα   a·cosθ]
    //     [sinθ   cosθ·cosα  -cosθ·sinα   a·sinθ]
    //     [0      sinα         cosα         d     ]
    //     [0      0            0            1     ]

    // Déterminer θ et d selon le type de joint
    double theta = 0.0;
    double d     = 0.0;

    if (dh.type == "prismatic") {
        theta = 0.0;
        d     = value;
    } else {
        // Revolute (par défaut)
        theta = value;
        d     = dh.d;
    }

    // Si theta est une variable nommée, utiliser value
    // (le cas où theta est un string "q0", "q1" est géré par l'appelant)

    double ct = std::cos(theta);
    double st = std::sin(theta);
    double ca = std::cos(dh.alpha);
    double sa = std::sin(dh.alpha);

    // Row 0
    matrixOut[0]  = ct;
    matrixOut[1]  = -st * ca;
    matrixOut[2]  = st * sa;
    matrixOut[3]  = dh.a * ct;

    // Row 1
    matrixOut[4]  = st;
    matrixOut[5]  = ct * ca;
    matrixOut[6]  = -ct * sa;
    matrixOut[7]  = dh.a * st;

    // Row 2
    matrixOut[8]  = 0.0;
    matrixOut[9]  = sa;
    matrixOut[10] = ca;
    matrixOut[11] = d;

    // Row 3
    matrixOut[12] = 0.0;
    matrixOut[13] = 0.0;
    matrixOut[14] = 0.0;
    matrixOut[15] = 1.0;
}

void DHKinematics::multiplyMatrix(const double a[16], const double b[16],
                                   double result[16])
{
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += a[row * 4 + k] * b[k * 4 + col];
            }
            result[row * 4 + col] = sum;
        }
    }
}

// =============================================================================
// Forward Kinematics
// =============================================================================

void DHKinematics::forwardKinematics(const std::vector<double>& jointValues,
                                      double& xOut, double& yOut, double& zOut)
{
    double identity[16] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };

    double current[16];
    double temp[16];

    // Copier l'identité dans current
    std::memcpy(current, identity, sizeof(current));

    for (size_t i = 0; i < mDhParams.size() && i < jointValues.size(); ++i) {
        double tMatrix[16];
        computeTransform(mDhParams[i], jointValues[i], tMatrix);
        multiplyMatrix(current, tMatrix, temp);
        std::memcpy(current, temp, sizeof(current));
    }

    xOut = current[3];   // translation X
    yOut = current[7];   // translation Y
    zOut = current[11];  // translation Z
}

// =============================================================================
// Inverse Kinematics
// =============================================================================

bool DHKinematics::inverseKinematics(double x, double y, double z,
                                      std::vector<double>& jointValuesOut)
{
    if (mDhParams.size() < 3) {
        mLastError = "DHKinematics: need at least 3 DOF for IK";
        return false;
    }

    // Pour ATM-100 frog-leg SCARA (2 révolutes + 1 prismatique, α=0),
    // utiliser la solution analytique (plus rapide et plus stable).
    // Détection : 3 joints, tous avec α=0, les 2 premiers révolutes.
    bool isScara = (mDhParams.size() == 3);
    for (size_t i = 0; i < mDhParams.size() && isScara; ++i) {
        if (std::abs(mDhParams[i].alpha) > 1e-9) {
            isScara = false;
        }
    }
    if (isScara && mDhParams[0].type != "prismatic" &&
                  mDhParams[1].type != "prismatic") {
        return solveIKScara(x, y, z, jointValuesOut);
    }

    // Fallback : solveur numérique générique
    return solveIKNumeric(x, y, z, jointValuesOut);
}

// =============================================================================
// IK analytique SCARA 2R + P (ATM-100 frog-leg)
// =============================================================================
//
// Géométrie :
//   x = a1·cos(θ1) + a2·cos(θ1+θ2)
//   y = a1·sin(θ1) + a2·sin(θ1+θ2)
//   z = d3
//
// Solution :
//   d3 = z
//   cos(θ2) = (x² + y² - a1² - a2²) / (2·a1·a2)
//   θ2 = ±acos(cos(θ2))  (elbow up/down, on choisit la solution faisable)
//   θ1 = atan2(y, x) - atan2(a2·sin(θ2), a1 + a2·cos(θ2))

bool DHKinematics::solveIKScara(double targetX, double targetY, double targetZ,
                                 std::vector<double>& jointValuesOut)
{
    double a1 = mDhParams[0].a;
    double a2 = mDhParams[1].a;

    jointValuesOut.resize(3);

    // Z : le 3e joint est prismatique → d3 = z
    jointValuesOut[2] = targetZ;

    // Distance radiale au carré
    double r2 = targetX * targetX + targetY * targetY;
    double r = std::sqrt(r2);

    // Vérifier l'atteignabilité
    double reachMin = std::abs(a1 - a2);
    double reachMax = a1 + a2;

    if (r > reachMax + mTolerance) {
        mLastError = "DHKinematics: target out of reach (r="
                     + std::to_string(r) + " > max=" + std::to_string(reachMax) + ")";
        return false;
    }
    if (r < reachMin - mTolerance && reachMin > mTolerance) {
        mLastError = "DHKinematics: target too close (r="
                     + std::to_string(r) + " < min=" + std::to_string(reachMin) + ")";
        return false;
    }

    // Clamper r dans [reachMin, reachMax]
    if (r > reachMax) r = reachMax;
    if (r < reachMin) r = reachMin;

    // cos(θ2) = (r² - a1² - a2²) / (2·a1·a2)
    double cosTheta2 = (r2 - a1 * a1 - a2 * a2) / (2.0 * a1 * a2);

    // Clamper dans [-1, 1] (précision numérique)
    if (cosTheta2 > 1.0)  cosTheta2 = 1.0;
    if (cosTheta2 < -1.0) cosTheta2 = -1.0;

    // Deux solutions : elbow up (θ2 négatif) ou elbow down (θ2 positif)
    // Pour l'ATM-100, on utilise elbow up (θ2 négatif) — le bras se replie
    double theta2 = -std::acos(cosTheta2);

    // θ1 = atan2(y, x) - atan2(a2·sin(θ2), a1 + a2·cos(θ2))
    double theta1 = std::atan2(targetY, targetX)
                    - std::atan2(a2 * std::sin(theta2),
                                 a1 + a2 * std::cos(theta2));

    jointValuesOut[0] = theta1;
    jointValuesOut[1] = theta2;

    return true;
}

// =============================================================================
// IK numérique générique (Newton-Raphson + Levenberg-Marquardt)
// =============================================================================

double DHKinematics::computeError(double x, double y, double z,
                                   double targetX, double targetY, double targetZ)
{
    double dx = x - targetX;
    double dy = y - targetY;
    double dz = z - targetZ;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool DHKinematics::solveIKNumeric(double targetX, double targetY, double targetZ,
                                   std::vector<double>& jointValuesOut)
{
    int n = static_cast<int>(mDhParams.size());
    jointValuesOut.assign(n, 0.0);

    double lambda = mLambdaInitial;

    for (int iter = 0; iter < mMaxIterations; ++iter) {
        // FK courante
        double fx, fy, fz;
        forwardKinematics(jointValuesOut, fx, fy, fz);

        // Erreur
        double error = computeError(fx, fy, fz, targetX, targetY, targetZ);
        if (error < mTolerance) {
            return true;  // convergé
        }

        // Jacobienne numérique (différences finies)
        double delta = 1e-6;
        // Matrice jacobienne 3×n
        // J[i][j] = ∂(position_i)/∂(joint_j)
        // On alloue sur la pile pour éviter le heap
        double jacobian[3 * 6];  // max 6 joints
        if (n > 6) {
            mLastError = "DHKinematics: too many joints for numeric IK";
            return false;
        }

        for (int j = 0; j < n; ++j) {
            double orig = jointValuesOut[j];
            jointValuesOut[j] = orig + delta;

            double px, py, pz;
            forwardKinematics(jointValuesOut, px, py, pz);

            jacobian[0 * n + j] = (px - fx) / delta;
            jacobian[1 * n + j] = (py - fy) / delta;
            jacobian[2 * n + j] = (pz - fz) / delta;

            jointValuesOut[j] = orig;
        }

        // Équations normales : (JᵀJ + λI) Δθ = Jᵀe
        // où e = (targetX-fx, targetY-fy, targetZ-fz)
        double ex = targetX - fx;
        double ey = targetY - fy;
        double ez = targetZ - fz;

        // Pour n ≤ 3, on résout directement
        // Jᵀe
        double jte[6] = {0};
        for (int j = 0; j < n; ++j) {
            jte[j] = jacobian[0 * n + j] * ex
                   + jacobian[1 * n + j] * ey
                   + jacobian[2 * n + j] * ez;
        }

        // JᵀJ + λI (matrice n×n)
        double jtj[6 * 6] = {0};
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) {
                double sum = 0.0;
                for (int k = 0; k < 3; ++k) {
                    sum += jacobian[k * n + r] * jacobian[k * n + c];
                }
                jtj[r * n + c] = sum;
            }
            jtj[r * n + r] += lambda;  // +λI
        }

        // Résoudre (JᵀJ + λI) Δθ = Jᵀe par élimination gaussienne
        // (matrice n×n, n ≤ 6 donc c'est rapide)
        double a[6 * 6];
        double b[6];
        std::memcpy(a, jtj, sizeof(double) * n * n);
        std::memcpy(b, jte, sizeof(double) * n);

        // Élimination gaussienne avec pivotage partiel
        for (int col = 0; col < n; ++col) {
            // Pivot
            int maxRow = col;
            double maxVal = std::abs(a[col * n + col]);
            for (int row = col + 1; row < n; ++row) {
                double val = std::abs(a[row * n + col]);
                if (val > maxVal) {
                    maxVal = val;
                    maxRow = row;
                }
            }

            if (maxVal < 1e-15) {
                // Matrice singulière, augmenter lambda
                lambda *= 10.0;
                goto nextIteration;
            }

            // Échanger les lignes
            if (maxRow != col) {
                for (int c = 0; c < n; ++c) {
                    double tmp = a[col * n + c];
                    a[col * n + c] = a[maxRow * n + c];
                    a[maxRow * n + c] = tmp;
                }
                double tmpB = b[col];
                b[col] = b[maxRow];
                b[maxRow] = tmpB;
            }

            // Éliminer
            double pivot = a[col * n + col];
            for (int row = col + 1; row < n; ++row) {
                double factor = a[row * n + col] / pivot;
                for (int c = col; c < n; ++c) {
                    a[row * n + c] -= factor * a[col * n + c];
                }
                b[row] -= factor * b[col];
            }
        }

        // Substitution arrière
        for (int row = n - 1; row >= 0; --row) {
            double sum = b[row];
            for (int c = row + 1; c < n; ++c) {
                sum -= a[row * n + c] * b[c];
            }
            b[row] = sum / a[row * n + row];
        }

        // Mettre à jour les joints
        for (int j = 0; j < n; ++j) {
            jointValuesOut[j] += b[j];  // b = Δθ
        }

        // Réduire lambda (convergence)
        lambda *= 0.5;
        if (lambda < 1e-6) {
            lambda = 1e-6;
        }

        nextIteration:;
    }

    mLastError = "DHKinematics: IK did not converge after "
                 + std::to_string(mMaxIterations) + " iterations";
    return false;
}
