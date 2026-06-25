#include "Agent/Drivers/GalilDriver.h"
#include "Agent/Drivers/GalilProtocol.h"
#include "Core/EventBus.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <cstring>

// =============================================================================
// Helpers
// =============================================================================

void GalilDriver::closeSocket(int& socket)
{
    if (socket >= 0) {
        ::close(socket);
        socket = -1;
    }
}

bool GalilDriver::setSocketTimeout(int socket, int timeoutMs)
{
    struct timeval tv;
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    return true;
}

// =============================================================================
// GalilDriver
// =============================================================================

GalilDriver::GalilDriver()
    : mCommandSocket(-1)
    , mMessageSocket(-1)
    , mConnected(false)
    , mListenerRunning(false)
    , mEventBus(nullptr)
{
}

GalilDriver::~GalilDriver()
{
    stopListener();
    disconnect();
}

bool GalilDriver::connect(const std::string& address, int commandPort, int messagePort)
{
    if (mConnected) {
        mLastError = "Already connected";
        return false;
    }

    // Créer les sockets
    mCommandSocket = socket(AF_INET, SOCK_STREAM, 0);
    mMessageSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (mCommandSocket < 0 || mMessageSocket < 0) {
        mLastError = "socket() failed: " + std::string(strerror(errno));
        closeSocket(mCommandSocket);
        closeSocket(mMessageSocket);
        return false;
    }

    // Résoudre l'adresse (supporte hostnames et IP)
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

    // Essayer d'abord en tant qu'IP dotted-decimal
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(address.c_str());

    if (addr.sin_addr.s_addr == INADDR_NONE)
    {
        // Résolution DNS pour hostname
        struct hostent* host = gethostbyname(address.c_str());
        if (host == nullptr || host->h_addrtype != AF_INET)
        {
            mLastError = "Cannot resolve address: " + address;
            disconnect();
            return false;
        }
        std::memcpy(&addr.sin_addr, host->h_addr, host->h_length);
    }

    // Connexion port commande
    addr.sin_port = htons(static_cast<uint16_t>(commandPort));
    if (::connect(mCommandSocket, reinterpret_cast<struct sockaddr*>(&addr),
                  sizeof(addr)) < 0) {
        mLastError = "connect() to port " + std::to_string(commandPort)
                     + " failed: " + strerror(errno);
        disconnect();
        return false;
    }
    setSocketTimeout(mCommandSocket, mDefaultTimeoutMs);

    // Connexion port message (optionnel : si messagePort == commandPort, on skip)
    if (messagePort != commandPort) {
        addr.sin_port = htons(static_cast<uint16_t>(messagePort));
        if (::connect(mMessageSocket, reinterpret_cast<struct sockaddr*>(&addr),
                      sizeof(addr)) < 0) {
            // Port message non disponible : ce n'est pas bloquant.
            // On ferme ce socket et on continue sans listener.
            closeSocket(mMessageSocket);
        } else {
            setSocketTimeout(mMessageSocket, mDefaultTimeoutMs);
        }
    } else {
        // Pas de port message séparé, on ferme le socket inutile
        closeSocket(mMessageSocket);
    }

    mConnected = true;

    // =========================================================================
    // Séquence d'initialisation post-connexion (inspirée de ThorV2)
    //
    // Ordre Galil standard :
    //   1. MO  — Safety: désactive tous les moteurs
    //   2. ST  — Stop tout mouvement résiduel
    //   3. SH  — Servo Here : active les moteurs
    //   4. SP  — Définit la vitesse par défaut (tous les axes)
    //   5. AC  — Définit l'accélération par défaut
    //   6. DC  — Définit la décélération par défaut
    // =========================================================================

    // 1. MO — Motors Off (sécurité au démarrage)
    sendAndCheck(GalilProtocol::encodeMotorOff(), mDefaultTimeoutMs);

    // 2. ST — Stop (annule tout mouvement résiduel)
    sendAndCheck(GalilProtocol::encodeStopAll(), mDefaultTimeoutMs);

    // 3. SH — Servo Here (active les moteurs, position courante = référence)
    if (!sendAndCheck(GalilProtocol::encodeServoHere(), mDefaultTimeoutMs))
    {
        // Non-bloquant : les primitives enverront SH si nécessaire
    }

    // 4. SP — Vitesse par défaut (25000 counts/s sur 8 axes, comme V2)
    {
        std::vector<double> defaultSpeeds(8, 25000.0);
        sendAndCheck(GalilProtocol::encodeSpeed(defaultSpeeds), mDefaultTimeoutMs);
    }

    // 5. AC — Accélération par défaut (10000 counts/s²)
    {
        std::vector<double> defaultAccels(8, 10000.0);
        sendAndCheck(GalilProtocol::encodeAcceleration(defaultAccels), mDefaultTimeoutMs);
    }

    // 6. DC — Décélération par défaut (10000 counts/s²)
    {
        std::vector<double> defaultDecels(8, 10000.0);
        sendAndCheck(GalilProtocol::encodeDeceleration(defaultDecels), mDefaultTimeoutMs);
    }

    return true;
}

void GalilDriver::disconnect()
{
    // Séquence d'arrêt (V2) : ST → MO
    if (mConnected)
    {
        sendAndCheck(GalilProtocol::encodeStopAll(), mDefaultTimeoutMs);
        sendAndCheck(GalilProtocol::encodeMotorOff(), mDefaultTimeoutMs);
    }

    closeSocket(mCommandSocket);
    closeSocket(mMessageSocket);
    mConnected = false;
}

bool GalilDriver::isConnected() const
{
    return mConnected;
}

// =============================================================================
// sendCommand — cœur de la communication TCP
// =============================================================================

bool GalilDriver::sendCommand(const std::string& command,
                               std::string& responseOut,
                               int timeoutMs)
{
    if (!mConnected) {
        mLastError = "Not connected";
        return false;
    }

    // Ajouter le terminateur de commande Galil (\r)
    std::string framed = command + "\r";

    // Envoyer
    ssize_t sent = ::send(mCommandSocket, framed.c_str(), framed.size(), 0);
    if (sent != static_cast<ssize_t>(framed.size())) {
        mLastError = "send() failed: " + std::string(strerror(errno));
        return false;
    }

    // Lire la réponse jusqu'au terminateur : ou ?
    char buffer[1024];
    responseOut.clear();

    struct pollfd pfd;
    pfd.fd = mCommandSocket;
    pfd.events = POLLIN;

    while (true) {
        int ret = poll(&pfd, 1, timeoutMs);
        if (ret < 0) {
            mLastError = "poll() failed: " + std::string(strerror(errno));
            return false;
        }
        if (ret == 0) {
            mLastError = "Timeout waiting for response to: " + command;
            return false;
        }

        ssize_t n = ::recv(mCommandSocket, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            mLastError = "recv() failed or connection closed";
            return false;
        }
        buffer[n] = '\0';
        responseOut += buffer;

        // Chercher le terminateur : ou ?
        if (responseOut.find(':') != std::string::npos ||
            responseOut.find('?') != std::string::npos) {
            break;
        }
    }

    return true;
}

bool GalilDriver::sendAndCheck(const std::string& command, int timeoutMs)
{
    std::string response;
    if (!sendCommand(command, response, timeoutMs)) {
        return false;
    }

    std::string payload;
    bool isError;
    if (!GalilProtocol::decodeReply(response, payload, isError)) {
        mLastError = "Invalid response to: " + command;
        return false;
    }
    if (isError) {
        mLastError = "Controller rejected: " + command + " (" + payload + ")";
        return false;
    }
    return true;
}

// =============================================================================
// Commandes motion
// =============================================================================

bool GalilDriver::moveAbsolute(const std::vector<double>& counts)
{
    if (counts.empty()) {
        mLastError = "moveAbsolute: empty counts";
        return false;
    }

    // PA
    std::string cmd = GalilProtocol::encodeMoveAbsolute(counts);
    if (!sendAndCheck(cmd, mDefaultTimeoutMs)) {
        return false;
    }

    // BG (tous les axes)
    std::vector<int> allAxes;
    for (size_t i = 0; i < counts.size(); ++i) {
        allAxes.push_back(static_cast<int>(i));
    }
    cmd = GalilProtocol::encodeBeginMotion(allAxes);
    if (!sendAndCheck(cmd, mDefaultTimeoutMs)) {
        return false;
    }

    return true;
}

bool GalilDriver::moveRelative(const std::vector<double>& deltas)
{
    if (deltas.empty()) {
        mLastError = "moveRelative: empty deltas";
        return false;
    }

    std::string cmd = GalilProtocol::encodeMoveRelative(deltas);
    if (!sendAndCheck(cmd, mDefaultTimeoutMs)) {
        return false;
    }

    std::vector<int> allAxes;
    for (size_t i = 0; i < deltas.size(); ++i) {
        allAxes.push_back(static_cast<int>(i));
    }
    cmd = GalilProtocol::encodeBeginMotion(allAxes);
    if (!sendAndCheck(cmd, mDefaultTimeoutMs)) {
        return false;
    }

    return true;
}

bool GalilDriver::beginMotion(const std::vector<int>& axisIndices)
{
    std::string cmd = GalilProtocol::encodeBeginMotion(axisIndices);
    return sendAndCheck(cmd, mDefaultTimeoutMs);
}

bool GalilDriver::afterMotion(const std::vector<int>& axisIndices)
{
    std::string cmd = GalilProtocol::encodeAfterMotion(axisIndices);
    return sendAndCheck(cmd, mDefaultTimeoutMs);
}

bool GalilDriver::stopAll()
{
    return sendAndCheck(GalilProtocol::encodeStopAll(), mDefaultTimeoutMs);
}

bool GalilDriver::motorOff()
{
    return sendAndCheck(GalilProtocol::encodeMotorOff(), mDefaultTimeoutMs);
}

bool GalilDriver::servoHere()
{
    return sendAndCheck(GalilProtocol::encodeServoHere(), mDefaultTimeoutMs);
}

bool GalilDriver::home(int axisIndex)
{
    // 1. Servo Here — activer le moteur avant le homing
    //    SH sans argument = tous les axes, mais on ne veut que celui-ci.
    //    On envoie d'abord SH global (simple), puis HM sur l'axe cible.
    std::string cmd = GalilProtocol::encodeServoHere();
    if (!sendAndCheck(cmd, mDefaultTimeoutMs)) {
        return false;
    }

    // 2. Home — lancer la recherche du switch de limite
    cmd = GalilProtocol::encodeHome(axisIndex);
    if (!sendAndCheck(cmd, mDefaultTimeoutMs)) {
        return false;
    }

    // 3. Begin Motion — démarrer le mouvement de homing
    cmd = GalilProtocol::encodeBeginMotion({axisIndex});
    return sendAndCheck(cmd, mDefaultTimeoutMs);
}

bool GalilDriver::setSpeeds(const std::vector<double>& speeds)
{
    return sendAndCheck(GalilProtocol::encodeSpeed(speeds), mDefaultTimeoutMs);
}

bool GalilDriver::setAccelerations(const std::vector<double>& accels)
{
    return sendAndCheck(GalilProtocol::encodeAcceleration(accels),
                        mDefaultTimeoutMs);
}

bool GalilDriver::setDecelerations(const std::vector<double>& decels)
{
    return sendAndCheck(GalilProtocol::encodeDeceleration(decels),
                        mDefaultTimeoutMs);
}

// =============================================================================
// Requêtes
// =============================================================================

bool GalilDriver::getPositions(std::vector<double>& positionsOut)
{
    std::string response;
    if (!sendCommand(GalilProtocol::encodeTellPosition(), response,
                     mDefaultTimeoutMs)) {
        return false;
    }

    std::string payload;
    bool isError;
    if (!GalilProtocol::decodeReply(response, payload, isError)) {
        return false;
    }
    if (isError) {
        mLastError = "getPositions: controller error";
        return false;
    }

    return GalilProtocol::parseAxisValues(payload, positionsOut, 0);
}

bool GalilDriver::getPositionsReport(std::vector<double>& positionsOut)
{
    std::string response;
    if (!sendCommand(GalilProtocol::encodeReportPosition(), response,
                     mDefaultTimeoutMs)) {
        return false;
    }

    std::string payload;
    bool isError;
    if (!GalilProtocol::decodeReply(response, payload, isError)) {
        return false;
    }
    if (isError) {
        mLastError = "getPositionsReport: controller error";
        return false;
    }

    return GalilProtocol::parseAxisValues(payload, positionsOut, 0);
}

bool GalilDriver::getDigitalInput(int pin, bool& valueOut)
{
    // La commande Galil pour lire une entrée est @IN[pin]
    std::string cmd = "@IN[" + std::to_string(pin) + "]";

    std::string response;
    if (!sendCommand(cmd, response, mDefaultTimeoutMs)) {
        return false;
    }

    std::string payload;
    bool isError;
    if (!GalilProtocol::decodeReply(response, payload, isError)) {
        return false;
    }

    // La réponse est "0" ou "1"
    valueOut = (!payload.empty() && payload != "0");
    return true;
}

bool GalilDriver::setDigitalOutput(int pin, bool value)
{
    // SB = set bit, CB = clear bit
    std::string cmd;
    if (value) {
        cmd = "SB " + std::to_string(pin);
    } else {
        cmd = "CB " + std::to_string(pin);
    }
    return sendAndCheck(cmd, mDefaultTimeoutMs);
}

// =============================================================================
// Listener asynchrone (port 2324)
// =============================================================================

void GalilDriver::startListener(EventBus* bus)
{
    if (mListenerRunning.load()) {
        return;  // déjà démarré
    }

    mEventBus = bus;
    mListenerRunning.store(true);

    // Utiliser un pointeur de membre (pas de lambda)
    mListenerThread = std::thread(&GalilDriver::listenLoop, this);
}

void GalilDriver::stopListener()
{
    mListenerRunning.store(false);

    if (mListenerThread.joinable()) {
        mListenerThread.join();
    }
}

void GalilDriver::listenLoop()
{
    char buffer[1024];

    while (mListenerRunning.load()) {
        struct pollfd pfd;
        pfd.fd = mMessageSocket;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100);  // 100 ms timeout pour pouvoir s'arrêter
        if (ret < 0) {
            break;
        }
        if (ret == 0) {
            continue;  // timeout, on vérifie mListenerRunning
        }

        ssize_t n = ::recv(mMessageSocket, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            break;  // connexion fermée ou erreur
        }
        buffer[n] = '\0';

        // Publier un événement générique pour les messages async
        // (le parsing fin des messages Galil sera ajouté plus tard)
        if (mEventBus) {
            Event ev;
            ev.type = EventType::IOChange;  // placeholder
            ev.priority = EventPriority::High;
            snprintf(ev.data.generic.stringValue, 64, "%.63s", buffer);
            mEventBus->publish(ev);
        }
    }
}

// =============================================================================
// Diagnostic
// =============================================================================

std::string GalilDriver::lastError() const
{
    return mLastError;
}

bool GalilDriver::sendRawCommand(const std::string& command,
                                  std::string& responseOut, int timeoutMs)
{
    return sendCommand(command, responseOut, timeoutMs);
}
