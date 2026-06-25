#include "Agent/Agent.h"
#include <iostream>
#include <csignal>

namespace {
    /// @brief Flag atomique pour l'arrêt sur SIGINT/SIGTERM.
    volatile sig_atomic_t gRunning = 1;

    /// @brief Handler de signal pour arrêt gracieux.
    void signalHandler(int /*signum*/)
    {
        gRunning = 0;
    }
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // 1. Charger la configuration
    Agent agent("Config/Agent.json");

    if (!agent.lastError().empty())
    {
        std::cerr << "Erreur de configuration: " << agent.lastError() << std::endl;
        return 1;
    }

    std::cout << "ThorV4 — Configuration chargée." << std::endl;

    // 2. Initialiser les sous-systèmes (LuaEngine, EventBus, Workcell wiring)
    if (!agent.initialize())
    {
        std::cerr << "Erreur d'initialisation: " << agent.lastError() << std::endl;
        return 1;
    }

    std::cout << "ThorV4 — Initialisation terminée." << std::endl;

    // 3. Afficher l'état de la WorkCell
    if (auto* wc = agent.getWorkcell())
    {
        wc->listComponents();
    }

    // 4. Enregistrer les handlers de signal pour arrêt gracieux
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "ThorV4 — Entrée dans la boucle principale. Ctrl+C pour arrêter." << std::endl;

    // 5. Boucle principale (consomme EventBus, ticks périodiques)
    while (gRunning)
    {
        agent.run();
    }

    // 6. Arrêt gracieux
    std::cout << "\nThorV4 — Arrêt en cours..." << std::endl;
    agent.shutdown();
    std::cout << "ThorV4 — Arrêt terminé." << std::endl;

    return 0;
}
