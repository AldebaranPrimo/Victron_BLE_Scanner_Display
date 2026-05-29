// ============================================================================
// config_portal.cpp — WiFi AP + captive-portal web server
//
// Serves the configuration form (config_html.h) and persists submissions to
// NVS. The device rows are generated in C++ (buildDeviceRow) so the row count
// always follows MAX_DEVICES with no HTML duplication. On save, device names are
// sanitized to a safe MQTT topic segment ([a-z0-9_]), back-compat defaults are
// filled (normalizeDefaultNames), and duplicate names are disambiguated so two
// devices never publish to the same topic.
// ============================================================================

#include "config_portal.h"
#include "config_html.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer server(80);
static DNSServer dnsServer;
static bool saved = false;
static GatewayConfig portalCfg;

// Builds one collapsible <details> device block for slot `i`, pre-filled from
// `dc`. The first slot is rendered `open`; the rest collapsed. The Name field
// is constrained to a valid topic segment via the HTML pattern attribute.
static String buildDeviceRow(int i, const DeviceConfig& dc) {
    String idx = String(i);
    String openAttr = (i == 0) ? " open" : "";
    String row;
    row.reserve(900);

    row += "<details" + openAttr + ">\n";
    row += "<summary>Dispositivo " + String(i + 1) + "</summary>\n";
    row += "<div class=\"chk\"><input type=\"checkbox\" name=\"dev" + idx +
           "_en\" value=\"1\" " + (dc.enabled ? "checked" : "") +
           "><label>Abilitato</label></div>\n";

    row += "<label>Nome</label>\n";
    row += "<input name=\"dev" + idx + "_name\" value=\"" + String(dc.name) +
           "\" maxlength=\"15\" pattern=\"[a-z0-9_]{1,15}\" "
           "title=\"lowercase letters, digits, underscore\"";
    if (i == 0) row += " placeholder=\"mppt (keep for back-compat)\"";
    row += ">\n";
    row += "<div class=\"info\">Topic MQTT: victron/&lt;nome&gt;/state</div>\n";

    row += "<label>Tipo</label>\n";
    row += "<select name=\"dev" + idx + "_type\">\n";
    row += "<option value=\"0\"" + String(dc.type == 0 ? " selected" : "") + ">SmartSolar MPPT</option>\n";
    row += "<option value=\"1\"" + String(dc.type == 1 ? " selected" : "") + ">SmartShunt</option>\n";
    row += "<option value=\"2\"" + String(dc.type == 2 ? " selected" : "") + ">SmartBatterySense</option>\n";
    row += "</select>\n";

    row += "<label>MAC Address (12 hex, senza \":\")</label>\n";
    row += "<input name=\"dev" + idx + "_mac\" value=\"" + String(dc.mac) +
           "\" maxlength=\"12\" pattern=\"[0-9a-fA-F]{12}\" placeholder=\"es. c15639b47db5\">\n";
    if (i == 0) {
        row += "<div class=\"info\">Trovi MAC e chiave in VictronConnect &gt; "
               "Impostazioni &gt; Info prodotto &gt; Instant Readout</div>\n";
    }

    row += "<label>Encryption Key (32 hex)</label>\n";
    row += "<input name=\"dev" + idx + "_key\" value=\"" + String(dc.aesKey) +
           "\" maxlength=\"32\" pattern=\"[0-9a-fA-F]{32}\">\n";
    row += "</details>\n";

    return row;
}

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

    // Generate all device rows from C++ (single source of truth = MAX_DEVICES).
    String rows;
    rows.reserve(MAX_DEVICES * 900);
    for (int i = 0; i < MAX_DEVICES; i++) rows += buildDeviceRow(i, cfg.devices[i]);
    html.replace("%DEVICE_ROWS%", rows);

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

        // Name -> sanitize to a safe MQTT topic segment: lowercase, keep only
        // [a-z0-9_]. Anything else (spaces, '/', accents, ...) is dropped.
        String name = server.arg("dev" + idx + "_name");
        name.toLowerCase();
        String clean;
        for (size_t k = 0; k < name.length(); k++) {
            char c = name[k];
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') clean += c;
        }
        strncpy(portalCfg.devices[i].name, clean.c_str(), sizeof(portalCfg.devices[i].name) - 1);
        portalCfg.devices[i].name[sizeof(portalCfg.devices[i].name) - 1] = '\0';

        String mac = server.arg("dev" + idx + "_mac");
        mac.toLowerCase();
        strncpy(portalCfg.devices[i].mac, mac.c_str(), sizeof(portalCfg.devices[i].mac) - 1);

        String key = server.arg("dev" + idx + "_key");
        key.toLowerCase();
        strncpy(portalCfg.devices[i].aesKey, key.c_str(), sizeof(portalCfg.devices[i].aesKey) - 1);

        portalCfg.devices[i].type = server.arg("dev" + idx + "_type").toInt();
    }

    // Fill any enabled-but-nameless slot with its canonical back-compat default
    // (same logic the loader uses), so clearing the name field never breaks the
    // topic. mppt / mppt2 / smartshunt / battery_sense.
    normalizeDefaultNames(portalCfg);

    // Duplicate-name guard: if two enabled devices ended up with the same name,
    // append the slot index to the later one so they publish to distinct topics.
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!portalCfg.devices[i].enabled) continue;
        for (int j = i + 1; j < MAX_DEVICES; j++) {
            if (!portalCfg.devices[j].enabled) continue;
            if (strcmp(portalCfg.devices[i].name, portalCfg.devices[j].name) == 0) {
                char fixed[16];
                snprintf(fixed, sizeof(fixed), "%.13s%d", portalCfg.devices[j].name, j);
                strncpy(portalCfg.devices[j].name, fixed, sizeof(portalCfg.devices[j].name) - 1);
                portalCfg.devices[j].name[sizeof(portalCfg.devices[j].name) - 1] = '\0';
                Serial.printf("[PORTAL] dup name fixed: slot %d -> %s\n", j, portalCfg.devices[j].name);
            }
        }
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
