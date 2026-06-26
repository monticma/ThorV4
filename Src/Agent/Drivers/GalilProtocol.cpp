#include "Agent/Drivers/GalilProtocol.h"

#include <cmath>
#include <sstream>

// =============================================================================
// Helpers privés
// =============================================================================

char GalilProtocol::axisLabel(int index)
{
    // 0 → 'A', 1 → 'B', ... 7 → 'H'
    return 'A' + static_cast<char>(index);
}

std::string GalilProtocol::buildCsvCommand(const std::string& prefix,
                                           const std::vector<double>& values)
{
    std::string cmd = prefix;
    cmd += ' ';  // espace requis entre la commande et les paramètres
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            cmd += ',';
        }
        long count = static_cast<long>(std::llround(values[i]));
        cmd += std::to_string(count);
    }
    return cmd;
}

std::string GalilProtocol::buildExplicitCommand(const std::string& prefix,
                                                const std::vector<double>& values)
{
    std::string cmd = prefix;
    cmd += ' ';  // espace requis entre la commande et les paramètres
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            cmd += ',';
        }
        cmd += axisLabel(static_cast<int>(i));
        cmd += '=';
        long count = static_cast<long>(std::llround(values[i]));
        cmd += std::to_string(count);
    }
    return cmd;
}

// =============================================================================
// Encode : valeurs C++ → strings Galil
// =============================================================================

std::string GalilProtocol::encodeMoveAbsolute(const std::vector<double>& counts)
{
    // Ne pas envoyer les axes à zéro (placeholders).
    // On construit uniquement les paires axis=valeur pour valeurs ≠ 0.
    // Si toutes les valeurs sont à 0, on envoie quand même le 1er axe.
    std::string cmd = "PA ";
    bool first = true;
    for (size_t i = 0; i < counts.size(); ++i) {
        if (counts[i] == 0.0 && counts.size() > 1)
            continue; // ignorer les axes placeholder
        if (!first) cmd += ',';
        cmd += axisLabel(static_cast<int>(i));
        cmd += '=';
        long count = static_cast<long>(std::llround(counts[i]));
        cmd += std::to_string(count);
        first = false;
    }
    return cmd;
}

std::string GalilProtocol::encodeMoveRelative(const std::vector<double>& deltas)
{
    // Même logique que encodeMoveAbsolute : ignorer les axes à zéro
    std::string cmd = "PR ";
    bool first = true;
    for (size_t i = 0; i < deltas.size(); ++i) {
        if (deltas[i] == 0.0 && deltas.size() > 1)
            continue;
        if (!first) cmd += ',';
        cmd += axisLabel(static_cast<int>(i));
        cmd += '=';
        long count = static_cast<long>(std::llround(deltas[i]));
        cmd += std::to_string(count);
        first = false;
    }
    return cmd;
}

std::string GalilProtocol::encodeBeginMotion(const std::vector<int>& axisIndices)
{
    std::string cmd = "BG ";
    for (int index : axisIndices) {
        cmd += axisLabel(index);
    }
    return cmd;
}

std::string GalilProtocol::encodeStopAll()
{
    return "ST";
}

std::string GalilProtocol::encodeMotorOff()
{
    return "MO";
}

std::string GalilProtocol::encodeServoHere()
{
    // "SH" tout court agit sur tous les axes
    return "SH";
}

std::string GalilProtocol::encodeHome(int axisIndex)
{
    std::string cmd = "HM ";
    cmd += axisLabel(axisIndex);
    return cmd;
}

std::string GalilProtocol::encodeTellPosition()
{
    return "TP";
}

std::string GalilProtocol::encodeReportPosition()
{
    return "RP";
}

std::string GalilProtocol::encodeSpeed(const std::vector<double>& speeds)
{
    return buildCsvCommand("SP", speeds);
}

std::string GalilProtocol::encodeAcceleration(const std::vector<double>& accels)
{
    return buildCsvCommand("AC", accels);
}

std::string GalilProtocol::encodeDeceleration(const std::vector<double>& decels)
{
    return buildCsvCommand("DC", decels);
}

std::string GalilProtocol::encodeAfterMotion(const std::vector<int>& axisIndices)
{
    std::string cmd = "AM ";
    for (int index : axisIndices) {
        cmd += axisLabel(index);
    }
    return cmd;
}

// =============================================================================
// Decode : strings Galil → valeurs C++
// =============================================================================

bool GalilProtocol::decodeReply(const std::string& raw,
                                std::string& payloadOut,
                                bool& isErrorOut)
{
    if (raw.empty()) {
        return false;
    }

    // Trouver la fin du contenu utile (avant CR/LF)
    size_t end = raw.size();
    while (end > 0 && (raw[end - 1] == '\r' || raw[end - 1] == '\n')) {
        --end;
    }

    if (end == 0) {
        // Pas de contenu avant les CR/LF
        return false;
    }

    // Le dernier caractère significatif est le terminateur : ou ?
    char terminator = raw[end - 1];

    if (terminator == ':') {
        isErrorOut = false;
        payloadOut = raw.substr(0, end - 1);
        return true;
    }

    if (terminator == '?') {
        isErrorOut = true;
        payloadOut = raw.substr(0, end - 1);
        return true;
    }

    // Pas de terminateur reconnu
    return false;
}

bool GalilProtocol::parseAxisValues(const std::string& payload,
                                    std::vector<double>& valuesOut,
                                    int expectedCount)
{
    valuesOut.clear();

    if (payload.empty()) {
        return expectedCount == 0;
    }

    // Parser manuellement (pas de strtok qui modifie la chaîne)
    size_t pos = 0;
    while (pos < payload.size()) {
        // Sauter les espaces et virgules au début
        while (pos < payload.size() &&
               (payload[pos] == ' ' || payload[pos] == ',' || payload[pos] == '\t')) {
            ++pos;
        }

        if (pos >= payload.size()) {
            break;
        }

        // Trouver la fin du nombre
        size_t start = pos;
        while (pos < payload.size() &&
               payload[pos] != ',' && payload[pos] != ' ' && payload[pos] != '\t') {
            ++pos;
        }

        std::string token = payload.substr(start, pos - start);
        if (token.empty()) {
            continue;
        }

        // Convertir en double
        double value = 0.0;
        try {
            value = std::stod(token);
        } catch (const std::exception&) {
            return false;
        }

        valuesOut.push_back(value);
    }

    if (expectedCount > 0 && static_cast<int>(valuesOut.size()) != expectedCount) {
        return false;
    }

    return true;
}
