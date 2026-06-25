#include <iostream>
#include <string>

#include "Agent/Components/Component.h"

// -----------------------------------------------------------------------------
// Helpers statiques pour le dump des structs communs
// -----------------------------------------------------------------------------

/**
 * @brief Affiche une ligne indentée "clé : valeur" (string).
 */
void dumpField(const std::string& key, const std::string& value, int indent)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << value << "\n";
}

/**
 * @brief Affiche une ligne indentée "clé : valeur" (int).
 */
void dumpFieldInt(const std::string& key, int value, int indent)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << value << "\n";
}

/**
 * @brief Affiche une ligne indentée "clé : valeur" (double).
 */
void dumpFieldDouble(const std::string& key, double value, int indent)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << value << "\n";
}

/**
 * @brief Affiche une ligne indentée "clé : valeur" (bool).
 */
void dumpFieldBool(const std::string& key, bool value, int indent)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << (value ? "true" : "false") << "\n";
}

/**
 * @brief Affiche un vecteur de strings indenté sous une clé.
 */
void dumpStringVector(const std::string& key,
                     const std::vector<std::string>& vec,
                     int indent)
{
    if (vec.empty()) return;
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : [";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << vec[i];
    }
    std::cout << "]\n";
}

/**
 * @brief Affiche un vecteur d'entiers indenté sous une clé.
 */
void dumpIntVector(const std::string& key,
                  const std::vector<int>& vec,
                  int indent)
{
    if (vec.empty()) return;
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : [";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << vec[i];
    }
    std::cout << "]\n";
}

/**
 * @brief Affiche une IoPinRequired.
 */
void dumpIoPin(const IoPinRequired& io, int indent)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << "- id=" << io.id
              << " dir=" << io.direction
              << " signal=" << io.signalType
              << " count=" << io.count
              << " voltage=" << io.voltage
              << " debounce=" << io.debounceMs << "ms";
    if (!io.description.empty())
        std::cout << " (" << io.description << ")";
    std::cout << "\n";
}

/**
 * @brief Affiche un vecteur de IoPinRequired.
 */
void dumpIoPins(const std::vector<IoPinRequired>& pins,
               const std::string& label,
               int indent)
{
    if (pins.empty()) return;
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << label << " (" << pins.size() << ") :\n";
    for (const auto& p : pins)
        dumpIoPin(p, indent + 2);
}

/**
 * @brief Affiche un ComponentAxis.
 */
void dumpAxis(const ComponentAxis& ax, int indent)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << "- " << ax.name
              << " type=" << ax.type
              << " role=" << ax.role
              << " travel=[" << ax.travel.min << ".." << ax.travel.max << "]";
    if (ax.travel.continuous) std::cout << " continuous";
    std::cout << " maxV=" << ax.maxVelocity
              << " maxA=" << ax.maxAcceleration
              << " home=" << ax.homePosition;
    if (ax.upPosition != 0.0 || ax.downPosition != 0.0)
        std::cout << " up=" << ax.upPosition << " down=" << ax.downPosition;
    if (!ax.description.empty())
        std::cout << " (" << ax.description << ")";
    std::cout << "\n";
}

/**
 * @brief Affiche un vecteur de ComponentAxis.
 */
void dumpAxes(const std::vector<ComponentAxis>& axes,
             const std::string& label,
             int indent)
{
    if (axes.empty()) return;
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << label << " (" << axes.size() << ") :\n";
    for (const auto& a : axes)
        dumpAxis(a, indent + 2);
}

/**
 * @brief Affiche un ComponentHoming.
 */
void dumpHoming(const ComponentHoming& h, int indent)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << "homing: method=" << h.method
              << " dir=" << h.direction
              << " velocity=" << h.velocity;
    if (!h.sequence.empty()) {
        std::cout << " seq=[";
        for (size_t i = 0; i < h.sequence.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << h.sequence[i];
        }
        std::cout << "]";
    }
    if (!h.description.empty())
        std::cout << " (" << h.description << ")";
    std::cout << "\n";
}

// =============================================================================

void Component::dump() const
{
    std::cout << "  [Component base]\n";
    dumpField("version", version);
    dumpField("fileType", fileType);
}
