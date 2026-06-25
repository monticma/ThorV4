# AGENTS.md — Directives Impératives pour l'Implémentation

Ce document contient les règles **obligatoires** à suivre pour toute contribution
au code source du projet ThorV4. Aucune dérogation n'est admise sans discussion
préalable.

---

## 1. Principe KISS (Keep It Super Simple)

- **Interdiction absolue des modifications par lot (batch replace).** Même
  s'il y a plusieurs centaines de fichiers à modifier, chaque fichier doit
  être édité **individuellement** avec un outil d'édition dédié
  (`replace_string_in_file` ou `create_file`). Les scripts shell (`sed`,
  `cat >`, boucles `for`) qui modifient les fichiers source du projet sont
  **proscrits**. Cette règle évite les corruptions en cascade et garantit que
  chaque modification est vérifiable.
- Privilégier **toujours** la solution la plus simple et la plus directe.
- Une fonction = une responsabilité. Si une fonction dépasse ~40 lignes, la
  découper.
- Pas d'optimisation prématurée : faire fonctionner d'abord, optimiser ensuite
  **si nécessaire**.
- Pas d'abstraction inutile : ne pas créer de hiérarchie de classes ou d'interfaces
  « au cas où ».
- Le code doit pouvoir être compris par un développeur junior sans explication
  orale.
- Pas d'utilisation de fonctions lambda.
- Pas d'utilisation de templates C++.

---

## 2. Conventions de Nommage

### 2a. Variables : `camelCase` explicite

- Toute variable, membre de classe, paramètre de fonction doit être en
  **camelCase**.
- Le nom doit être **parlant** et décrire sans ambiguïté ce que contient la
  variable.
- Proscrire les abréviations obscures (`sz`, `ptr`, `idx` → préférer `size`,
  `pointer`, `index`).
- Proscrire les noms à une ou deux lettres, sauf pour les indices de boucle
  (`i`, `j`, `k`).

```cpp
// ✅ Correct
int waferCount = 0;
double approachSpeed = 30.0;
std::string controllerAddress = "192.168.1.10";

// ❌ Incorrect
int wc = 0;
double appSpd = 30.0;
std::string ctrlAddr = "192.168.1.10";
```

### 2b. PascalCase pour les noms structurels

| Élément | Convention | Exemple |
|---|---|---|
| Noms de **dossiers** | `PascalCase` | `Include/`, `Src/Agent/Components/` |
| Noms de **fichiers** | `PascalCase` | `Workcell.h`, `Robot.cpp`, `Agent.json` |
| Noms de **classes** | `PascalCase` | `class Workcell`, `class SignalTower` |
| Noms de **structs** | `PascalCase` | `struct RobotJoint`, `struct WorkcellUnits` |
| Noms de **méthodes** | `camelCase` | `loadFromFile()`, `findComponent()` |
| Noms de **fonctions libres** | `camelCase` | `createComponent()`, `validateSchema()` |

### 2c. Langage : C++

- Le langage de programmation du projet est le **C++** (standard C++17 minimum).
- Pas de C, pas de Python, pas de Lua pour le code cœur (les plugins et scripts
  de test peuvent utiliser d'autres langages si justifié).

### 2d. Séparation déclaration / implémentation

- **Déclarations** des classes et structs : dans des fichiers `.h` (ou `.hpp`).
- **Implémentations** des méthodes : dans des fichiers `.cpp` correspondants.
- Un seul fichier `.h` et un seul fichier `.cpp` par classe (sauf classes
  internes triviales).
- Les structs de données sans méthodes peuvent rester dans le `.h`.

```
Include/Agent/Components/Robot.h   ← déclaration de la classe Robot
Src/Agent/Components/Robot.cpp     ← implémentation de la classe Robot
```

### 2e. Principe « Header First »

1. **D'abord** concevoir et écrire le fichier `.h` (attributs, signatures des
   méthodes, documentation Doxygen).
2. **Ensuite seulement** implémenter le fichier `.cpp`.
3. Ne jamais modifier le `.cpp` sans avoir mis à jour le `.h` correspondant si
   nécessaire.
4. Toute méthode publique doit apparaître dans le `.h` avant d'être codée dans
   le `.cpp`.

### 2f. Commentaires Doxygen obligatoires

- **Toute** classe, struct, méthode, fonction, énumération doit être documentée
  avec des commentaires au format **Doxygen** (`///` ou `/** */`).
- Une documentation minimale comprend :
  - **Description** : ce que fait la fonction/classe.
  - **`@param`** : chaque paramètre (nom + description).
  - **`@return`** : valeur de retour (si non-void).
  - **`@throw`** : exceptions éventuelles (si applicable).

```cpp
/**
 * @brief Charge la configuration d'un composant depuis un fichier JSON.
 *
 * Parcourt le fichier JSON spécifié et remplit les attributs de la classe.
 * En cas d'erreur de parsing, `lastError()` retournera le détail.
 *
 * @param file_path Chemin absolu ou relatif vers le fichier .json.
 * @return true si le chargement a réussi, false sinon.
 */
bool loadFromFile(const std::string& file_path);
```

### 2g. Pas de fonctions lambda

- L'utilisation de **fonctions lambda** (`[](){}`) est **interdite**.
- Toute logique nécessitant un callback ou un prédicat doit être écrite sous
  forme de **fonction nommée** (fonction libre ou méthode privée).
- Motif accepté pour remplacer une lambda : une fonction privée statique ou une
  classe foncteur simple.

```cpp
// ❌ Interdit
std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
    return a.name < b.name;
});

// ✅ Correct
static bool compareByName(const Item& a, const Item& b)
{
    return a.name < b.name;
}
// ...
std::sort(items.begin(), items.end(), compareByName);
```

### 2h. Pas de templates

- L'utilisation de **templates C++** (`template<typename T>`) est **interdite**.
- Toute généricité doit être obtenue par polymorphisme classique (héritage,
  fonctions virtuelles) ou par spécialisation explicite.
- Les templates déjà présents dans les bibliothèques externes (nlohmann/json,
  sol2, etc.) sont tolérés car hors du périmètre du projet.

```cpp
// ❌ Interdit
template<typename T>
T getMax(const T& a, const T& b) { return (a > b) ? a : b; }

// ✅ Correct — spécialiser pour chaque type nécessaire
double getMaxDouble(double a, double b) { return (a > b) ? a : b; }
int    getMaxInt(int a, int b)          { return (a > b) ? a : b; }
```

---

## 3. Règles Supplémentaires

- **Pas de `using namespace`** dans les headers (`.h` / `.hpp`). Autorisé
  uniquement dans les `.cpp`, portée locale.
- Tout fichier `.h` doit avoir un `#pragma once` en première ligne (après le
  commentaire Doxygen de fichier).
- Les champs membres des classes sont privés ou protégés ; l'accès se fait par
  des getters/setters **uniquement si nécessaire** (KISS : si une struct
  publique suffit, ne pas créer de classe avec accesseurs).
- Les includes doivent être ordonnés : standard library d'abord, puis
  dépendances externes, puis headers du projet.

---

## 4. Structure du Projet (rappel)

```
ThorV4/
├── AGENTS.md              ← ce fichier
├── CMakeLists.txt
├── Config/                ← fichiers JSON de configuration
│   ├── Agent.json
│   ├── Components/        ← templates par type de composant
│   ├── Plugins/
│   ├── Profiles/
│   ├── Schemas/
│   └── WorkCells/         ← définitions de cellules de travail
├── Include/
│   └── Agent/
│       ├── Agent.h
│       ├── Workcell.h
│       └── Components/    ← headers des classes composant
├── Src/
│   └── Agent/
│       ├── Agent.cpp
│       ├── ThorAgent.cpp
│       ├── Workcell.cpp
│       └── Components/    ← implémentations des classes composant
└── Vendor/                ← bibliothèques externes
```

---

*Dernière mise à jour : 2026-06-20*
