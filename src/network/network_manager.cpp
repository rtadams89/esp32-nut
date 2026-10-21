#include "network/network_manager.h"
#include "core/app_logger.h"

#if NUT_NET_ETHERNET
#include <esp_mac.h>

// Board pins: the Arduino variant defines ETH_PHY_* for the ESP32-P4 reference design
// (IP101 PHY, addr 1, MDC 31, MDIO 52, reset 51, RMII clock in on 50), which is also
// the Waveshare ESP32-P4-ETH wiring. Other boards override them with -DNUT_ETH_PHY_*
#ifndef NUT_ETH_PHY_TYPE
#define NUT_ETH_PHY_TYPE ETH_PHY_TYPE
#endif
#ifndef NUT_ETH_PHY_ADDR
#define NUT_ETH_PHY_ADDR ETH_PHY_ADDR
#endif
#ifndef NUT_ETH_PHY_MDC
#define NUT_ETH_PHY_MDC ETH_PHY_MDC
#endif
#ifndef NUT_ETH_PHY_MDIO
#define NUT_ETH_PHY_MDIO ETH_PHY_MDIO
#endif
#ifndef NUT_ETH_PHY_POWER
#define NUT_ETH_PHY_POWER ETH_PHY_POWER
#endif
#ifndef NUT_ETH_CLK_MODE
#define NUT_ETH_CLK_MODE ETH_CLK_MODE
#endif

AppNetworkManager::AppNetworkManager()
    : m_lastConnectionAttempt(0),
      m_lastDisconnectTime(0),
      m_isStarted(false),
      m_isApMode(false),
      m_lastConnected(false) {}

void AppNetworkManager::beginAP(const String&, const String&) {
    // No radio on this build: main.cpp never takes the AP path
    AppLogger::log("ERROR", "[NETWORK] Access Point mode is not available on the Ethernet build");
}

void AppNetworkManager::begin(const String&, const String&) {
    m_isStarted = true;
    m_isApMode = false;
    m_lastConnected = false;

    AppLogger::log("INFO", "[NETWORK] Initializing Ethernet...");

    // Hostname before begin(): DHCP sends it with the first request
    uint8_t mac[6];
    macAddress(mac);
    char hostname[24];
    snprintf(hostname, sizeof(hostname), "esp32-nut-%02x%02x%02x", mac[3], mac[4], mac[5]);
    ETH.setHostname(hostname);

    if (!ETH.begin(NUT_ETH_PHY_TYPE, NUT_ETH_PHY_ADDR, NUT_ETH_PHY_MDC, NUT_ETH_PHY_MDIO,
                   NUT_ETH_PHY_POWER, NUT_ETH_CLK_MODE)) {
        AppLogger::log("ERROR", "[NETWORK] Ethernet initialization failed: check the PHY pins");
        return;
    }
    AppLogger::log("INFO", "[NETWORK] Ethernet started, hostname %s, waiting for link and DHCP", hostname);
    m_lastConnectionAttempt = millis();
    m_lastDisconnectTime = m_lastConnectionAttempt;
}

void AppNetworkManager::loop() {
    if (!m_isStarted) {
        return;
    }
    // The ESP-IDF Ethernet driver and DHCP client reconnect by themselves: only log changes
    bool connected = isConnected();
    if (connected != m_lastConnected) {
        if (connected) {
            AppLogger::log("INFO", "[NETWORK] Connection established! IP Address: %s (%u Mbps, %s duplex)\n",
                           ETH.localIP().toString().c_str(), (unsigned)ETH.linkSpeed(),
                           ETH.fullDuplex() ? "full" : "half");
        } else {
            m_lastDisconnectTime = millis();
            AppLogger::log("WARN", ETH.linkUp() ? "[NETWORK] Ethernet lost its IP address"
                                                : "[NETWORK] Ethernet link down (cable unplugged?)");
        }
        m_lastConnected = connected;
    }
}

bool AppNetworkManager::isConnected() const {
    return ETH.linkUp() && ETH.hasIP();
}

String AppNetworkManager::statusText() const {
    if (!m_isStarted) return "Disconnected";
    if (!ETH.linkUp()) return "Disconnected";
    if (!ETH.hasIP()) return "Connecting";
    return ETH.localIP().toString() + " (" + String(ETH.linkSpeed()) + " Mbps)";
}

void AppNetworkManager::macAddress(uint8_t mac[6]) const {
    // Read from eFuse: valid before ETH.begin() too
    if (esp_read_mac(mac, ESP_MAC_ETH) != ESP_OK) {
        memset(mac, 0, 6);
    }
}

#else // Wi-Fi

AppNetworkManager::AppNetworkManager()
    : m_lastConnectionAttempt(0),
      m_lastDisconnectTime(0),
      m_isStarted(false),
      m_isApMode(false),
      m_lastStatus(WL_IDLE_STATUS) {}

void AppNetworkManager::beginAP(const String& ap_ssid, const String& ap_password) {
    m_isStarted = true;
    m_isApMode = true;
    AppLogger::log("INFO", "[NETWORK] Starting Access Point mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
    AppLogger::log("INFO", "[NETWORK] AP Started. IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void AppNetworkManager::begin(const String& ssid, const String& password) {
    m_ssid = ssid;
    m_password = password;
    m_isStarted = true;
    m_isApMode = false;
    m_lastStatus = WL_IDLE_STATUS;

    AppLogger::log("INFO", "[NETWORK] Initializing Wi-Fi...");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true); // Consente all'ESP32 di gestire le riconnessioni a basso livello

    AppLogger::log("INFO", "[NETWORK] Connecting to SSID: %s...\n", m_ssid.c_str());
    WiFi.begin(m_ssid.c_str(), m_password.c_str());
    m_lastConnectionAttempt = millis();
    m_lastDisconnectTime = m_lastConnectionAttempt;
}

void AppNetworkManager::loop() {
    if (!m_isStarted || m_isApMode) {
        return;
    }

    wl_status_t currentStatus = WiFi.status();
    uint32_t now = millis();

    // Rileva cambiamenti di stato
    if (currentStatus != m_lastStatus) {
        if (m_lastStatus == WL_CONNECTED && currentStatus != WL_CONNECTED) {
            m_lastDisconnectTime = now;
        }

        if (currentStatus == WL_CONNECTED) {
            AppLogger::log("INFO", "[NETWORK] Connection established! IP Address: %s\n", WiFi.localIP().toString().c_str());
        } else if (currentStatus == WL_DISCONNECTED && m_lastStatus == WL_CONNECTED) {
            AppLogger::log("WARN", "[NETWORK] Wi-Fi connection lost!");
        } else if (currentStatus == WL_NO_SSID_AVAIL) {
            AppLogger::log("WARN", "[NETWORK] SSID network not found.");
        } else if (currentStatus == WL_CONNECT_FAILED) {
            AppLogger::log("ERROR", "[NETWORK] Connection failed.");
        } else if (currentStatus == WL_CONNECTION_LOST) {
            AppLogger::log("WARN", "[NETWORK] Connection lost (WL_CONNECTION_LOST).");
        }
        m_lastStatus = currentStatus;
    }

    if (currentStatus == WL_CONNECTED) {
        m_lastConnectionAttempt = now; // Reset attempt timer while connected
    }

    // Gestione riconnessione e fallback
    if (currentStatus != WL_CONNECTED) {
        if (now - m_lastConnectionAttempt >= 15000) {
            m_lastConnectionAttempt = now;
            AppLogger::log("INFO", "[NETWORK] Reconnecting... Attempt on SSID: %s\n", m_ssid.c_str());
            // Forza una nuova connessione
            WiFi.disconnect();
            WiFi.begin(m_ssid.c_str(), m_password.c_str());
        }
    }
}

bool AppNetworkManager::isConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

String AppNetworkManager::statusText() const {
    if (m_isApMode) return "AP Mode Active";
    if (WiFi.status() == WL_CONNECTED) return WiFi.SSID();
    return "Connecting";
}

void AppNetworkManager::macAddress(uint8_t mac[6]) const {
    WiFi.macAddress(mac);
}

#endif
