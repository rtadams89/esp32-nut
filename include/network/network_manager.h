#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>

// Transport selected at build time: Wi-Fi (ESP32-S3, default) or wired Ethernet
// (ESP32-P4 boards, -DNUT_NET_ETHERNET=1 in platformio.ini)
#ifndef NUT_NET_ETHERNET
#define NUT_NET_ETHERNET 0
#endif

#if NUT_NET_ETHERNET
#include <ETH.h>
#else
#include <WiFi.h>
#endif

class AppNetworkManager {
public:
    AppNetworkManager();

    // true when the firmware has a Wi-Fi radio (Access Point setup, Wi-Fi tab)
    static constexpr bool hasWifi() { return !NUT_NET_ETHERNET; }

    // Avvia la connessione Wi-Fi. Ethernet: ssid e password ignorati, avvia il PHY (DHCP)
    void begin(const String& ssid, const String& password);
    // Wi-Fi only: Access Point for the first setup and the recovery
    void beginAP(const String& ap_ssid, const String& ap_password);

    // Gestisce il monitoraggio e la riconnessione
    void loop();

    // Ritorna true se la connessione è attiva (Ethernet: link up con indirizzo IP)
    bool isConnected() const;

    // Testo per la Web UI: l'SSID, "Connecting", "Disconnected" o, in Ethernet,
    // "192.168.1.10 (100 Mbps)"
    String statusText() const;
    // MAC address of the station / Ethernet interface, for the device id
    void macAddress(uint8_t mac[6]) const;

private:
    uint32_t m_lastConnectionAttempt;
    uint32_t m_lastDisconnectTime;
    bool m_isStarted;
    bool m_isApMode;
#if NUT_NET_ETHERNET
    bool m_lastConnected;
#else
    String m_ssid;
    String m_password;
    wl_status_t m_lastStatus;
#endif
};

#endif // NETWORK_MANAGER_H
