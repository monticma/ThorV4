#pragma once

#include <string>
#include <vector>

// =============================================================================
// GalilProtocol — Codec pur pour le langage ASCII Galil DMC
// =============================================================================
//
// Fonctions STATIC, sans état, sans I/O.
// Chaque fonction encode*() prend des valeurs C++ et retourne une string Galil.
// Chaque fonction decode*() prend une string Galil et retourne des valeurs C++.
//
// La forme explicite par axe est utilisée pour les commandes de position :
//   "PAA=1000,B=2000,C=5000"
// La forme CSV compacte est utilisée pour les commandes de vitesse/accélération :
//   "SP10000,20000,50000"
//
// Terminateur de commande : CR ('\r'), ajouté par GalilDriver (pas ici).
// Terminateur de réponse  : ':' (succès) ou '?' (erreur).
//
// Usage :
//   std::string cmd = GalilProtocol::encodeMoveAbsolute(counts);
//   driver.sendRaw(cmd, response, timeout);
//   bool isError;
//   std::string payload;
//   GalilProtocol::decodeReply(response, payload, isError);
// =============================================================================

/// @brief Codec pur pour le protocole texte Galil DMC.
///
/// Toutes les méthodes sont statiques. Aucune instance n'est nécessaire.
/// Partagé entre tous les modèles Galil (DMC-8240, DMC-2143, DMC-4040...).
class GalilProtocol
{
public:
    // =========================================================================
    // Encode : valeurs C++ → strings de commande Galil
    // =========================================================================

    /// @brief Construit une commande PA (Position Absolute) par axe.
    /// @param counts Positions en counts (index 0 = axe A, 1 = B, ...).
    /// @return "PAA=1000,B=2000,C=5000"
    static std::string encodeMoveAbsolute(const std::vector<double>& counts);

    /// @brief Construit une commande PR (Position Relative) par axe.
    /// @param deltas Déplacements en counts (index 0 = axe A, ...).
    /// @return "PRA=100,B=200,C=300"
    static std::string encodeMoveRelative(const std::vector<double>& deltas);

    /// @brief Construit une commande BG (Begin Motion).
    /// @param axisIndices Indices des axes à démarrer (0=A, 1=B, ...).
    /// @return "BGABC"
    static std::string encodeBeginMotion(const std::vector<int>& axisIndices);

    /// @brief "ST" — stop immédiat de tous les axes.
    static std::string encodeStopAll();

    /// @brief "MO" — motor off (désactive les servos).
    static std::string encodeMotorOff();

    /// @brief "SH" — servo here (active l'asservissement sur tous les axes).
    static std::string encodeServoHere();

    /// @brief Construit une commande HM (Home) pour un axe.
    /// @param axisIndex Index de l'axe (0=A, 1=B, ...).
    /// @return "HMA"
    static std::string encodeHome(int axisIndex);

    /// @brief "TP" — tell position (retourne la position de tous les axes).
    static std::string encodeTellPosition();

    /// @brief "RP" — report position (format compact).
    static std::string encodeReportPosition();

    /// @brief Construit une commande SP (Speed) en CSV.
    /// @param speeds Vitesses en counts/s par axe.
    /// @return "SP10000,20000,50000"
    static std::string encodeSpeed(const std::vector<double>& speeds);

    /// @brief Construit une commande AC (Acceleration) en CSV.
    /// @param accels Accélérations en counts/s² par axe.
    /// @return "AC10000,20000,50000"
    static std::string encodeAcceleration(const std::vector<double>& accels);

    /// @brief Construit une commande DC (Deceleration) en CSV.
    /// @param decels Décélérations en counts/s² par axe.
    /// @return "DC10000,20000,50000"
    static std::string encodeDeceleration(const std::vector<double>& decels);

    /// @brief Construit une commande AM (After Motion) pour des axes.
    /// @param axisIndices Indices des axes à attendre (0=A, 1=B, ...).
    /// @return "AMABC"
    static std::string encodeAfterMotion(const std::vector<int>& axisIndices);

    // =========================================================================
    // Decode : strings de réponse Galil → valeurs C++
    // =========================================================================

    /// @brief Décode une réponse brute du Galil.
    ///
    /// Nettoie les CR/LF, détecte le terminateur ':' ou '?',
    /// et extrait le payload.
    ///
    /// @param raw       Réponse brute reçue du socket (ex: "1000,2000:\r\n").
    /// @param payloadOut [out] Partie utile avant le terminateur.
    /// @param isErrorOut [out] true si terminateur '?', false si ':'.
    /// @return true si un terminateur valide a été trouvé.
    static bool decodeReply(const std::string& raw,
                            std::string& payloadOut,
                            bool& isErrorOut);

    /// @brief Parse un payload CSV de valeurs numériques.
    ///
    /// @param payload       Chaîne "1000,2000,5000".
    /// @param valuesOut     [out] Vecteur rempli avec {1000.0, 2000.0, 5000.0}.
    /// @param expectedCount Nombre de valeurs attendues (0 = pas de vérification).
    /// @return true si le parsing a réussi et que le compte correspond.
    static bool parseAxisValues(const std::string& payload,
                                std::vector<double>& valuesOut,
                                int expectedCount);

private:
    /// @brief Convertit un index d'axe en lettre Galil (0→A, 1→B, ...).
    static char axisLabel(int index);

    /// @brief Construit une commande de type "CMD" + valeurs CSV.
    /// @param prefix    Préfixe de la commande (ex: "SP", "AC", "DC").
    /// @param values    Valeurs à formater en CSV.
    /// @return "SP10000,20000,50000"
    static std::string buildCsvCommand(const std::string& prefix,
                                       const std::vector<double>& values);

    /// @brief Construit une commande de type "CMD" + "AXE=valeur,..." .
    /// @param prefix    Préfixe de la commande (ex: "PA", "PR").
    /// @param values    Valeurs à formater en paires axe=valeur.
    /// @return "PAA=1000,B=2000,C=5000"
    static std::string buildExplicitCommand(const std::string& prefix,
                                            const std::vector<double>& values);
};
