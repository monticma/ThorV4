# Rapport d'Analyse Comparative des Fichiers JSON — ThorV4

**Date :** 2026-06-20  
**Périmètre :** Tous les JSON de `Config/` (templates composants, WorkCell, schémas, plugins, profils)

---

## Table des matières

1. [Joints vs Axes : deux modèles incompatibles](#1-joints-vs-axes--deux-modèles-incompatibles)
2. [Unités : définitions hétérogènes et risque matériel](#2-unités--définitions-hétérogènes-et-risque-matériel)
3. [I/O Pins : chaos des connexions analogiques/digitales](#3-io-pins--chaos-des-connexions-analogiquesdigitales)
4. [Format des axes : divergence entre composants mobiles](#4-format-des-axes--divergence-entre-composants-mobiles)
5. [Problèmes additionnels](#5-problèmes-additionnels)
6. [Propositions d'harmonisation](#6-propositions-dharmonisation)

---

## 1. Joints vs Axes : deux modèles incompatibles

### 1.1 État des lieux

| Composant | Fichier JSON | Terme utilisé | Structure | Type d'axe |
|---|---|---|---|---|
| **Robot** | `ATM100V2.json` | `joints` (tableau) | `joints[].{name, type, minLimit, maxLimit, maxVelocity, maxAcceleration, homePosition}` | `revolute`, `prismatic` |
| **Aligner** | `PRE-200.json` | `axes` (objet nommé) | `axes.{rotation, radial, pinLift}.{role, travel, maxVelocity, maxAcceleration}` | `rotation`, `centering`, `vertical` |
| **Track** | `Track_600mm.json` | `axes` (objet nommé) | `axes.{linear}.{role, travel, maxVelocity, maxAcceleration}` | `translation` |
| **Flipper** | `Flipper_Standard.json` | `axes` (objet nommé) | `axes.{rotation}.{role, travel, maxVelocity, maxAcceleration}` | `rotation` |
| **Controller** | `axisMapping[]` (WorkCell) | `controllerAxis` + `joint` + `role` | `{controllerAxis, component, joint, role, countsPerUnit}` | `rotation`, `extension`, `vertical`, `translation`, `centering` |

### 1.2 Problèmes identifiés

#### A. Structure de données différente
- **Robot** utilise un **tableau** `joints[]` avec un champ `type` ∈ {`revolute`, `prismatic`}
- **Aligner, Track, Flipper** utilisent un **objet nommé** `axes.{nom}` avec un champ `role` ∈ {`rotation`, `centering`, `vertical`, `translation`}
- Impossible d'itérer uniformément sur les axes d'un composant sans connaître son type.

#### B. Sémantique différente pour le même concept
- `joints[].type` = `"revolute"` (Robot) vs `axes.rotation.role` = `"rotation"` (Aligner/Flipper) — même réalité physique, nommage différent.
- `joints[].minLimit`/`maxLimit` (Robot) vs `axes.{}.travel.min`/`max` (Aligner/Track/Flipper) — même information, structure différente.

#### C. Champs exclusifs au Robot
- `homePosition` existe dans `joints[]` du Robot mais pas dans les `axes` des autres composants.
- `continuous` (rotation infinie) existe dans `axes.rotation.travel` de l'Aligner mais pas dans `joints` du Robot.
- `upPosition`/`downPosition` existe dans `axes.pinLift` de l'Aligner, idem absent des joints.

#### D. Le Controller axisMapping fait le pont... mais avec un 3ème vocabulaire
- `axisMapping[].role` ∈ {`rotation`, `extension`, `vertical`, `translation`, `centering`} — ni le vocabulaire de Robot (`revolute`/`prismatic`), ni celui d'Aligner (`rotation`/`centering`/`vertical`).

### 1.3 Impact
- Pas de fonction générique `getAxes()` qui marche pour tous les composants mobiles.
- Le mapping contrôleur → composant doit connaître la structure interne de chaque type.

---

## 2. Unités : définitions hétérogènes et risque matériel

### 2.1 État des lieux

| Composant | Champs `units` présents | Champs absents (vs WorkCell) |
|---|---|---|
| **WorkCell** | `length`, `angle`, `linearVelocity`, `angularVelocity`, `linearAcceleration`, `angularAcceleration`, `time`, `pressure`, `speed` | — (le plus complet) |
| **Controller** | `length`, `angle`, `linearVelocity`, `angularVelocity`, `linearAcceleration`, `angularAcceleration`, `time` | `pressure`, `speed` |
| **Robot** | `length`, `angle`, `linearVelocity`, `angularVelocity`, `linearAcceleration`, `angularAcceleration` | `time`, `pressure`, `speed` |
| **Aligner** | `length`, `angle`, `linearVelocity`, `angularVelocity`, `linearAcceleration`, `angularAcceleration`, `time` | `pressure`, `speed` |
| **Track** | `length`, `linearVelocity`, `linearAcceleration` | `angle`, `angularVelocity`, `angularAcceleration`, `time`, `pressure`, `speed` |
| **Flipper** | `angle`, `angularVelocity`, `angularAcceleration` | `length`, `linearVelocity`, `linearAcceleration`, `time`, `pressure`, `speed` |
| **Cassette** | `length` uniquement | Tous les autres |
| **EndEffector** | `length`, `angle`, `pressure`, `force`, `mass`, `time` | `linearVelocity`, `angularVelocity`, `linearAcceleration`, `angularAcceleration`, `speed` |
| **ProcessZone** | *(aucun)* | *(tous)* |
| **SignalTower** | *(aucun)* | *(tous)* |
| **CycleCounter** | *(aucun)* | *(tous)* |

### 2.2 Problèmes identifiés

#### A. Aucune unité standard obligatoire
- Rien n'empêche un composant de déclarer `"length": "in"` pendant que la WorkCell déclare `"length": "mm"`.
- La conversion n'est implémentée nulle part → **risque de collision matériel**.

#### B. Le Controller a `limits.maxVelocity` et `maxAcceleration` en `counts/s` et `counts/s²`
- Ce sont des unités brutes encodeur, inutilisables sans connaître le `countsPerUnit` du `axisMapping`.
- Le `countsPerUnit` est dans le WorkCell.json mais n'est **pas lu** par le code C++ (cf. audit précédent).

#### C. `speed` en pourcentage (`%`)
- Les vitesses workflow et composant sont en `%` du max. Mais quel max ? Celui du template, celui de l'override WorkCell, ou celui du profil Galil ?
- Pas de hiérarchie de résolution documentée.

#### D. Unités manquantes critiques
- **Cassette** ne déclare que `length` — pourtant `pitch`, `offset`, `stroke` sont en mm.
- **EndEffector** a `force`, `mass`, `pressure` — mais le Robot n'a pas ces unités. Que faire si l'EE spécifie `force` en `N` mais le Robot attend des `kgf` ?
- **ProcessZone** n'a pas de `time` dans ses unités, mais a un `timeout` en ms. L'unité est implicite.

#### E. Plugin kinematics_atm100.json utilise `degrees` et `millimeters`
- Alors que tous les autres JSON utilisent `deg` et `mm`. Incohérence de nomenclature.

### 2.3 Impact
- **Risque matériel réel** : si un composant est en inches et un autre en mm, le robot peut percuter un obstacle.
- Pas de validation croisée des unités entre template composant et WorkCell.

---

## 3. I/O Pins : chaos des connexions analogiques/digitales

### 3.1 État des lieux

Chaque composant dans le WorkCell.json définit ses pins I/O de manière **totalement ad-hoc** :

| Composant | Structure `io` | Exemple de pins |
|---|---|---|
| **galil1** (controller) | Pas de `io` — les pins sont implicites dans `axisMapping` | — |
| **aligner** | `io.type`, `io.startAlignPin`, `io.alignDonePin`, `io.alignErrorPin`, `io.chuckVacuumOutputPin`, `io.chuckVacuumSensorPin`, `io.pinLiftOutputPin` | 7 champs nommés individuellement |
| **cassette1** | `io.cassettePresencePin`, `io.doorInterlockPin` | 2 champs |
| **cassette2** | `io.cassettePresencePin`, `io.doorInterlockPin` | 2 champs |
| **processZone1** | `io.type`, `io.readyPin`, `io.startProcessPin`, `io.processingPin`, `io.donePin`, `io.errorPin` | 6 champs |
| **towerLight** | `io.greenPin`, `io.yellowPin`, `io.redPin`, `io.buzzerPin` | 4 champs |
| **endEffector1** | `io.vacuumOutputPin`, `io.vacuumSensorPin` | 2 champs |
| **endEffector1.scanner** | `scanner.laserOutputPin`, `scanner.beamBreakInputPin`, `scanner.offsetFromTCP.{x,y,z}`, `scanner.beamWidth` | 6 champs (dont 2 pins) |

### 3.2 Problèmes identifiés

#### A. Aucune structure commune
- Chaque composant invente ses propres noms de pins.
- Les pins sont des champs plats, sans métadonnées (direction, type électrique, durée de debounce, pull-up/pull-down).

#### B. Pas de distinction Input/Output explicite
- Certains noms suggèrent la direction (`vacuumOutputPin` vs `vacuumSensorPin`) mais ce n'est pas structuré.
- Impossible de valider automatiquement qu'un pin configuré en output n'est pas câblé sur une entrée contrôleur.

#### C. Pas de vérification de conflit de pins
- Rien n'empêche deux composants d'utiliser le même numéro de pin.
- Exemple : `aligner.startAlignPin = 3` et `processZone1.donePin = 9` — si on ajoute un composant avec `pin = 3`, conflit silencieux.

#### D. Le type de signal n'est pas documenté
- Est-ce du 24V DC ? Du 5V TTL ? Du contact sec ?
- Le contrôleur a des entrées/sorties digitales ET analogiques, mais rien dans le JSON ne distingue les deux.

#### E. Les pins sont dans le WorkCell.json, pas dans le template composant
- Le template EndEffector (`EE_Horseshoe_200mm.json`) définit le nombre de canaux vacuum (`channels: 1`) mais pas les pins.
- Les pins sont dans le WorkCell → OK pour l'instance, mais le template devrait déclarer le **nombre et type de pins requis** (ex: "ce composant a besoin de 2 digital outputs et 1 digital input").

### 3.3 Impact
- Impossible d'écrire un validateur automatique de câblage.
- Risque de conflit de pins entre composants.
- Pas de distinction digital/analogique → risque de brancher un capteur analogique sur une entrée digitale.

---

## 4. Format des axes : divergence entre composants mobiles

### 4.1 État des lieux

Tous les composants mobiles (Robot, Aligner, Track, Flipper) définissent des axes, mais avec des formats incompatibles.

| Propriété | Robot (`joints[]`) | Aligner (`axes.*`) | Track (`axes.*`) | Flipper (`axes.*`) |
|---|---|---|---|---|
| **Structure racine** | Tableau `[]` | Objet nommé `{}` | Objet nommé `{}` | Objet nommé `{}` |
| **Nom de l'axe** | `joints[].name` | Clé de l'objet (`rotation`) | Clé de l'objet (`linear`) | Clé de l'objet (`rotation`) |
| **Type cinématique** | `joints[].type` ∈ {revolute, prismatic} | *(absent, déduit du rôle)* | *(absent)* | *(absent)* |
| **Rôle** | *(absent)* | `axes.*.role` | `axes.*.role` | `axes.*.role` |
| **Limites** | `minLimit`, `maxLimit` (flat) | `travel.min`, `travel.max` (imbriqué) | `travel.min`, `travel.max` (imbriqué) | `travel.min`, `travel.max` (imbriqué) |
| **Vitesse max** | `maxVelocity` | `maxVelocity` | `maxVelocity` | `maxVelocity` |
| **Accélération max** | `maxAcceleration` | `maxAcceleration` | `maxAcceleration` | `maxAcceleration` |
| **Home** | `homePosition` (obligatoire) | *(absent)* | `homing` (section séparée) | `homing` (section séparée) |
| **Continu** | *(absent)* | `travel.continuous` (rotation) | *(absent)* | *(absent)* |
| **Positions spéciales** | *(aucune)* | `upPosition`, `downPosition` (pinLift) | *(aucune)* | *(aucune)* |

### 4.2 Problèmes identifiés

#### A. Tableau vs Objet nommé
- Le Robot est le **seul** à utiliser un tableau. Tous les autres utilisent un objet nommé.
- Un tableau est plus générique (on peut avoir N joints), un objet nommé est plus lisible mais moins extensible.

#### B. Absence de `role` dans le Robot
- Le Robot n'a pas de champ `role` pour ses joints.
- Le `axisMapping` du contrôleur associe un `role` à chaque joint du robot (`rotation`, `extension`, `vertical`), mais cette information devrait être dans le template Robot.
- Actuellement, le rôle est défini **deux fois** : implicitement dans `axisMapping` du WorkCell, et devrait être dans `joints[]` du Robot.

#### C. Homing éclaté
- Robot : `homePosition` dans chaque joint.
- Track/Flipper : section `homing` séparée avec `method`, `direction`, `velocity`.
- Aligner : ni `homePosition`, ni `homing`.
- Aucune standardisation.

#### D. Le Controller axisMapping référence `joint` par nom
- `axisMapping[].joint` = `"Theta"` (nom dans `joints[]` du Robot)
- Pour Aligner/Track/Flipper, le `joint` dans axisMapping est... le nom de la clé d'axe (`"Linear"`, `"Rotation"`).
- Incohérence : pour le Robot, `joint` référence `joints[].name`. Pour les autres, `joint` référence la clé de `axes`.

### 4.3 Impact
- Pas de code générique pour itérer sur les axes d'un composant.
- Le driver doit avoir du code spécifique par type de composant.

---

## 5. Problèmes additionnels

### 5.1 Controller dans `components[]` vs `controller` racine
- Le schéma `workcell.schema.json` définit **deux** emplacements pour le contrôleur :
  - `controller` (objet racine) — avec `config`, `connection`, `axisMapping`
  - `components[]` (tableau) — où `galil1` de type `controller` apparaît
- Dans `WorkCell.json`, le contrôleur est **uniquement** dans `components[]`. La section `controller` racine n'existe pas.
- Le schéma et la réalité divergent.

### 5.2 `endEffector` : racine dédiée vs dans `components[]`
- Le schéma définit une section `endEffector` (objet racine) en plus de `components[]`.
- Dans `WorkCell.json`, `endEffector1` est dans `components[]`, pas de section racine `endEffector`.
- Même problème que pour le controller.

### 5.3 Plugin kinematics : unités non standard
- `kinematics_atm100.json` utilise `"unit": "degrees"` et `"unit": "millimeters"` au lieu de `deg`/`mm`.
- Les noms de joints sont `J1`, `J2`, `J3` au lieu de `Theta`, `Elbow`, `Z`.

### 5.4 Profil Galil : unités implicites
- Le profil `galil8240_default.json` a des valeurs brutes sans unités :
  - `SP: [25000, ...]` — sont-ce des counts/s ? des deg/s ?
  - `AC: [10000, ...]` — counts/s² ?
  - `TL: [1.0, ...]` — Volts ?
  - Aucune métadonnée d'unité.

### 5.5 `weightUnit` vs `units.mass`
- Le Robot utilise `physical.weightUnit = "kg"` dans `physical`.
- L'EndEffector utilise `units.mass = "kg"` dans `units`.
- Incohérence : le poids est-il une propriété `physical` ou une `unit` ?

---

## 6. Propositions d'harmonisation

### 6.1 Axes : format universel `axes[]`

**Proposition :** Tous les composants mobiles utilisent un **tableau `axes[]`** avec une structure commune.

```json
"axes": [
  {
    "name": "Theta",
    "type": "revolute",
    "role": "rotation",
    "travel": { "min": -180.0, "max": 180.0, "continuous": false },
    "maxVelocity": 360.0,
    "maxAcceleration": 1800.0,
    "homePosition": 0.0,
    "description": "Base rotation"
  }
]
```

| Champ | Type | Obligatoire | Description |
|---|---|---|---|
| `name` | string | OUI | Nom unique de l'axe dans le composant |
| `type` | enum | OUI | `revolute`, `prismatic` |
| `role` | enum | OUI | `rotation`, `extension`, `vertical`, `translation`, `centering` |
| `travel.min` | float | OUI | Limite basse (en unité du composant) |
| `travel.max` | float | OUI | Limite haute |
| `travel.continuous` | bool | NON | Axe rotatif continu (défaut: false) |
| `maxVelocity` | float | OUI | Vitesse max |
| `maxAcceleration` | float | OUI | Accélération max |
| `homePosition` | float | NON | Position de référence home |
| `upPosition` | float | NON | Position haute (pinLift, Z) |
| `downPosition` | float | NON | Position basse (pinLift, Z) |
| `description` | string | NON | Description |

**Avantages :**
- Code générique `for (auto& axis : component.axes)` possible pour TOUS les composants.
- Le `axisMapping` du contrôleur référence `axes[].name` de manière uniforme.
- Les champs optionnels (`continuous`, `upPosition`, `homePosition`) sont ignorés silencieusement par les composants qui ne les utilisent pas.

**Migration Robot :** `joints[]` → `axes[]` (renommer `minLimit`/`maxLimit` → `travel.min`/`travel.max`, ajouter `role`).

### 6.2 Unités : `units` obligatoire avec validation croisée

**Proposition :** Un bloc `units` minimal obligatoire pour TOUS les composants.

```json
"units": {
  "length": "mm",
  "angle": "deg",
  "time": "ms"
}
```

Trois unités de base **toujours requises** :
- `length` ∈ {`mm`, `m`, `in`}
- `angle` ∈ {`deg`, `rad`}
- `time` ∈ {`ms`, `s`}

Unités optionnelles selon le type de composant :
- `linearVelocity`, `angularVelocity`, `linearAcceleration`, `angularAcceleration` — pour composants mobiles
- `pressure` — pour vacuum/pneumatique
- `force` — pour préhenseurs
- `mass` — pour propriétés physiques
- `speed` — pour pourcentages (toujours `%`)

**Règle de validation :** Au chargement du WorkCell, vérifier que toutes les unités des composants sont cohérentes avec les unités de la WorkCell. Si divergence, **refuser le chargement** avec un message d'erreur explicite.

### 6.3 I/O Pins : structure standard `ioPins[]`

**Proposition :** Remplacer les champs I/O ad-hoc par un tableau structuré.

```json
"ioPins": [
  {
    "id": "vacuumOutput",
    "pin": 0,
    "direction": "output",
    "signalType": "digital",
    "voltage": "24VDC",
    "description": "Vacuum ejector solenoid"
  },
  {
    "id": "vacuumSensor",
    "pin": 1,
    "direction": "input",
    "signalType": "digital",
    "voltage": "24VDC",
    "debounceMs": 5,
    "pullUp": true,
    "description": "Vacuum pressure switch (NC)"
  }
]
```

| Champ | Type | Obligatoire | Description |
|---|---|---|---|
| `id` | string | OUI | Identifiant unique de la pin dans le composant |
| `pin` | int | OUI | Numéro de pin physique sur le contrôleur |
| `direction` | enum | OUI | `input`, `output` |
| `signalType` | enum | OUI | `digital`, `analog`, `pwm`, `quadrature` |
| `voltage` | string | NON | `5V`, `12V`, `24VDC`, `dry_contact` |
| `debounceMs` | int | NON | Temps de debounce en ms (entrées) |
| `pullUp` | bool | NON | Résistance pull-up activée |
| `description` | string | NON | Rôle fonctionnel |

**Template vs Instance :**
- Le **template** composant déclare les `ioPins` **requis** avec `pin: null` (pin non assigné).
- La **WorkCell** fournit les numéros de pins réels.

**Validation automatique :**
- Détection de conflits (deux composants sur le même pin).
- Vérification que le contrôleur supporte le type de signal demandé.
- Vérification que la direction correspond aux capacités du contrôleur (ex: pin 0-7 = digital out, 8-15 = digital in).

### 6.4 Controller : unification `components[]`

**Proposition :** Supprimer les sections racines `controller` et `endEffector` du schéma. Tout est dans `components[]`.

- Le contrôleur est un composant comme les autres, avec `type: "controller"`.
- L'`axisMapping` et la `connection` sont des propriétés de l'instance contrôleur dans la WorkCell.
- Supprimer la section `endEffector` racine → l'end-effector est un composant standard avec `attachedTo`.

### 6.5 Homing : section `homing` unifiée

**Proposition :** Ajouter une section `homing` optionnelle dans le template de chaque composant mobile.

```json
"homing": {
  "method": "limit_switch",
  "direction": "negative",
  "velocity": 10.0,
  "sequence": ["Theta", "Elbow", "Z"],
  "description": "Home all joints sequentially, Z last"
}
```

- `sequence` : ordre de homing des axes (optionnel, défaut = ordre de `axes[]`).

---

## Résumé des Actions Prioritaires

| Priorité | Action | Impact |
|---|---|---|
| 🔴 **P0** | Standardiser `axes[]` pour tous les composants mobiles | Débloque le code générique de pilotage |
| 🔴 **P0** | Standardiser `ioPins[]` avec validation de conflits | Évite les courts-circuits et conflits de pins |
| 🟠 **P1** | Rendre `units` obligatoire avec 3 champs minimum | Évite les collisions matériel (inches vs mm) |
| 🟠 **P1** | Supprimer les sections racines `controller`/`endEffector` du schéma | Simplifie le modèle (tout dans `components[]`) |
| 🟡 **P2** | Unifier `homing` pour tous les composants mobiles | Homogénéise le comportement de référence |
| 🟡 **P2** | Aligner les unités du plugin kinematics (`deg`/`mm` au lieu de `degrees`/`millimeters`) | Cohérence lexicale |
| 🟢 **P3** | Ajouter les métadonnées d'unités dans le profil Galil | Traçabilité et conversion automatique |

---

*Fin du rapport.*
