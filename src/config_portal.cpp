#include "config_portal.h"
#include "config_html.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer server(80);
static DNSServer dnsServer;
static bool saved = false;
static GatewayConfig portalCfg;

static String buildPage(const GatewayConfig& cfg) {
    String html = FPSTR(CONFIG_PAGE);

    html.replace("%WIFI_SSID%", cfg.wifiSsid);
    html.replace("%WIFI_PASS%", cfg.wifiPass);
    html.replace("%MQTT_BROKER%", cfg.mqttBroker);
    html.replace("%MQTT_PORT%", String(cfg.mqttPort ? cfg.mqttPort : 1883));
    html.replace("%MQTT_INTERVAL%", String(cfg.mqttPublishInterval ? cfg.mqttPublishInterval : 5));
    html.replace("%MQTT_TOPIC%", strlen(cfg.mqttBaseTopic) ? cfg.mqttBaseTopic : "victron");
    html.replace("%MQTT_USER%", cfg.mqttUser);
    html.replace("%MQTT_PASS%", cfg.mqttPass);

    for (int i = 0; i < MAX_DEVICES; i++) {
        String idx = String(i);
        html.replace("%DEV" + idx + "_EN%", cfg.devices[i].enabled ? "checked" : "");
        html.replace("%DEV" + idx + "_NAME%", cfg.devices[i].name);
        html.replace("%DEV" + idx + "_MAC%", cfg.devices[i].mac);
        html.replace("%DEV" + idx + "_KEY%", cfg.devices[i].aesKey);

        for (int t = 0; t < 3; t++) {
            String marker = "%DEV" + idx + "_T" + String(t) + "%";
            html.replace(marker, cfg.devices[i].type == t ? "selected" : "");
        }
    }

    return html;
}

static void handleRoot() {
    String page = buildPage(portalCfg);
    server.send(200, "text/html", page);
}

static void handleSave() {
    // WiFi
    strncpy(portalCfg.wifiSsid, server.arg("wifi_ssid").c_str(), sizeof(portalCfg.wifiSsid) - 1);
    strncpy(portalCfg.wifiPass, server.arg("wifi_pass").c_str(), sizeof(portalCfg.wifiPass) - 1);

    // MQTT
    strncpy(portalCfg.mqttBroker, server.arg("mqtt_broker").c_str(), sizeof(portalCfg.mqttBroker) - 1);
    portalCfg.mqttPort = server.arg("mqtt_port").toInt();
    if (portalCfg.mqttPort == 0) portalCfg.mqttPort = 1883;
    strncpy(portalCfg.mqttUser, server.arg("mqtt_user").c_str(), sizeof(portalCfg.mqttUser) - 1);
    strncpy(portalCfg.mqttPass, server.arg("mqtt_pass").c_str(), sizeof(portalCfg.mqttPass) - 1);
    strncpy(portalCfg.mqttBaseTopic, server.arg("mqtt_topic").c_str(), sizeof(portalCfg.mqttBaseTopic) - 1);
    portalCfg.mqttPublishInterval = server.arg("mqtt_interval").toInt();
    if (portalCfg.mqttPublishInterval == 0) portalCfg.mqttPublishInterval = 5;

    // Devices
    for (int i = 0; i < MAX_DEVICES; i++) {
        String idx = String(i);
        portalCfg.devices[i].enabled = server.hasArg("dev" + idx + "_en");

        String name = server.arg("dev" + idx + "_name");
        strncpy(portalCfg.devices[i].name, name.c_str(), sizeof(portalCfg.devices[i].name) - 1);

        String mac = server.arg("dev" + idx + "_mac");
        mac.toLowerCase();
        strncpy(portalCfg.devices[i].mac, mac.c_str(), sizeof(portalCfg.devices[i].mac) - 1);

        String key = server.arg("dev" + idx + "_key");
        key.toLowerCase();
        strncpy(portalCfg.devices[i].aesKey, key.c_str(), sizeof(portalCfg.devices[i].aesKey) - 1);

        portalCfg.devices[i].type = server.arg("dev" + idx + "_type").toInt();
    }

    configSave(portalCfg);
    server.send(200, "text/plain", "OK");
    saved = true;
}

static void handleNotFound() {
    // Captive portal: redirect everything to root
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

void portalStart(const GatewayConfig& currentCfg) {
    saved = false;
    memcpy(&portalCfg, &currentCfg, sizeof(GatewayConfig));

    WiFi.mode(WIFI_AP);
    WiFi.softAP("VictronBLE-Setup");
    delay(100);

    // DNS server for captive portal
    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.println("[PORTAL] AP started: VictronBLE-Setup (open)");
    Serial.print("[PORTAL] IP: ");
    Serial.println(WiFi.softAPIP());
}

void portalLoop() {
    dnsServer.processNextRequest();
    server.handleClient();
}

bool portalSaved() {
    return saved;
}

void portalStop() {
    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}
