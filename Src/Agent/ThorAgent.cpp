#include "Agent/Agent.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    Agent agent("Config/Agent.json");

    if (agent.lastError().empty()) {
        std::cout << "Configuration chargée avec succès!\n" << std::endl;

        // Lister tous les composants
        if (auto* wc = agent.getWorkcell()) {
            wc->dump();
        }

        return 0;
    } else {
        std::cerr << "Erreur lors du chargement de la configuration: "
                  << agent.lastError() << std::endl;
        return 1;
    }
}
