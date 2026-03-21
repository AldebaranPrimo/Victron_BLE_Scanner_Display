# Victron BLE Gateway - M5StickC

## Specifica Progetto per Sviluppo Firmware

**Versione:** 1.1  
**Data:** 15 Marzo 2026  
**Target Hardware:** M5StickC (prima generazione)  
**Obiettivo:** Lettura BLE passiva di 3 dispositivi Victron Energy e pubblicazione dati via MQTT

> **NOTA PER CLAUDE CODE**: il repository **AldebaranPrimo/Victron_BLE_Scanner_Display** 
> (https://github.com/AldebaranPrimo/Victron_BLE_Scanner_Display) e' la **base di partenza 
> consigliata** per lo sviluppo. E' gia' un progetto PlatformIO funzionante su M5StickC Plus 
> con supporto per tutti e 3 i dispositivi Victron (MPPT, SmartShunt, BatterySense), parsing 
> bit-level implementato e testato. Manca solo la parte WiFi+MQTT che va aggiunta.
> Clonare quel repo e lavorare da li' e' il percorso piu' sicuro.

---

## 1. Hardware Target

### M5StickC (prima generazione) - Specifiche Verificate da Etichetta

| Parametro | Valore |
|---|---|
| Model Name | M5StickC |
| FCC-ID | 2AN3WM5STICKC |
| SoC | ESP32-PICO-D4 (ESP32 dual-core Xtensa LX6) |
| CPU | 240 MHz dual-core |
| SRAM | 520 KB |
| Flash | 4 MB (in-package) |
| PSRAM | **ASSENTE** |
| WiFi | 802.11 b/g/n 2.4 GHz |
| Bluetooth | v4.2 BR/EDR + BLE |
| Display | ST7735S 80x160 @ 0.96" (SPI) |
| LCD Pins | G15/G13/G23/G18/G5 |
| PMIC | AXP192 (I2C: SDA G21, SCL G22) |
| IMU | MPU6886 6-Axis (I2C condiviso) |
| RTC | BM8563 (I2C condiviso) |
| Microfono | SPM1423 (D:G34, C:G0) |
| IR Emitter | G9 |
| LED Interna | G10 |
| Pulsante A (frontale) | G37 |
| Pulsante B (laterale) | G39 |
| Batteria | ~80 mAh LiPo |
| USB | USB-C (CP2104 USB-to-UART) |
| Baud rate | 1200-115200/250K/500K/750K/1.5M |
| Connettore espansione (top) | GND, 5V, G26, G36, G0, BAT, 3V3, 5V |
| Connettore Grove (bottom) | HY2.0-4P: G, Vout, G32, G33 |
| I2C interno | SDA: G21, SCL: G22, INT: G35 |

### Vincoli Hardware Critici

1. **No PSRAM**: tutta la RAM disponibile e' 520 KB SRAM. Lo stack BLE di ESP-IDF consuma ~70-100 KB, WiFi ~40-60 KB. Rimangono ~300-350 KB per l'applicazione. Sufficiente per il nostro caso d'uso (no display pesante, no framebuffer), ma NON usare componenti memory-intensive.

2. **Flash 4 MB**: con partition table standard (1 MB app, 1.5 MB OTA, 1 MB spiffs), lo spazio per il firmware e' limitato. Se serve OTA, usare una partition table ottimizzata.

3. **BLE 4.2**: sufficiente per ricevere BLE advertisements Victron (passive scan). NON serve BLE 5.0.

4. **Coesistenza WiFi + BLE**: l'ESP32 ha un singolo radio 2.4 GHz condiviso tra WiFi e BLE. Il coexistence controller integrato gestisce il time-slicing automaticamente. Per ottimizzare: usare scan BLE passivo (non attivo), intervalli di scan ragionevoli (non continuo).

5. **AXP192 PMIC**: deve essere inizializzato per alimentare il display e altri periferici. Senza inizializzazione AXP192, il dispositivo potrebbe non funzionare correttamente. La libreria M5StickC si occupa di questo nel suo `M5.begin()`.

---

## 2. Dispositivi Victron Target

### 2.1 SmartSolar MPPT (Record Type 0x01)

Regolatore di carica solare. Trasmette BLE advertisements con i seguenti dati (dopo decryption):

| Campo | Bit | Tipo | Scala | Unita' |
|---|---|---|---|---|
| Device state | 0-7 | uint8 | 1 | enum |
| Charger error | 8-15 | uint8 | 1 | enum |
| Battery voltage | 16-31 | int16 | 0.01 | V |
| Battery current | 32-41 | int10 (signed) | 0.1 | A |
| Yield today | 42-51 | uint10 | 0.01 | kWh |
| PV power | 52-67 | uint16 | 1 | W |
| Load current | 68-76 | uint9 | 0.1 | A |

**Device state enum:** 0=OFF, 1=Low Power, 2=Fault, 3=Bulk, 4=Absorption, 5=Float, 6=Storage, 7=Equalize, 11=Other(Hub-1), 245=Starting Up, 252=External Control

### 2.2 SmartBatterySense (Record Type 0x02)

Sensore batteria wireless. Condivide il Record Type 0x02 con SmartShunt/BMV ma ha PID diverso.

**Product IDs:** 0xA3A4, 0xA3A5

Struttura record type 0x02 (Battery Monitor):

| Campo | Bit | Tipo | Scala | Unita' |
|---|---|---|---|---|
| TTG (Time to Go) | 0-15 | uint16 | 1 | min |
| Battery voltage | 16-31 | int16 | 0.01 | V |
| Alarm reason | 32-47 | uint16 | - | bitfield |
| Aux value | 48-69 | - | - | vedi sotto |
| Aux mode | 70-71 | uint2 | - | enum |
| Battery current | 72-93 | int22 (signed) | 0.001 | A |
| Consumed Ah | 94-113 | uint20 | 0.1 | Ah |
| SOC | 114-123 | uint10 | 0.1 | % |

**Aux mode enum:** 0 = starter voltage, 1 = midpoint voltage, 2 = temperature, 3 = disabled

**Per SmartBatterySense:** solo voltage e temperature sono validi. aux_mode = 2, aux_value contiene temperatura in Kelvin * 100. Formula: `temp_C = (aux_value / 100.0) - 273.15`. Tutti gli altri campi (SOC, current, TTG, consumed Ah) contengono valori non significativi e vanno ignorati.

### 2.3 SmartShunt (Record Type 0x02)

Battery monitor completo. Stesso Record Type del BatterySense.

**Product IDs:** 0xA389, 0xA38A, 0xA38B (vari modelli SmartShunt)

Tutti i campi della tabella sopra sono validi per SmartShunt.

### Come distinguere SmartBatterySense da SmartShunt

Entrambi usano Record Type 0x02. L'approccio **corretto e affidabile** e' l'identificazione tramite **MAC address** (come fa il progetto AldebaranPrimo). Ogni dispositivo ha un MAC unico, e le chiavi AES sono per-dispositivo.

L'approccio alternativo (usato da esphome-victron_ble) e' tramite Product ID nel BLE advertisement, ma il MAC-based e' piu' robusto.

---

## 3. Protocollo Victron BLE Instant Readout

### 3.1 Struttura Advertisement

I dispositivi Victron trasmettono BLE advertisements con manufacturer data. La struttura del manufacturer data e':

```
Byte [0-1]: Vendor ID = 0x02E1 (Victron Energy) - little-endian: 0xE1, 0x02
Byte [2-3]: Product ID (es. 0x0257 per MPPT 100/50)
Byte [4]:   Record Type (0x01 = Solar Charger, 0x02 = Battery Monitor, 0x08 = AC Charger, ...)
Byte [5]:   Nonce/Data Counter low byte
Byte [6]:   Nonce/Data Counter high byte  
Byte [7]:   First byte of encryption key (encryptKeyMatch)
Byte [8..]: Encrypted data (lunghezza dipende dal record type)
```

**ATTENZIONE - Punto Critico**: il byte [4] "Record Type" nel protocollo Victron distingue la struttura dei dati crittografati. NON confondere con il tipo di advertisement BLE. Il valore `0x10` nel primo byte dei manufacturer data dopo il Vendor ID e' il tipo di "Product Advertisement" (vs 0x01/0x02/0x03 che sono pacchetti VE.Smart networking). Tuttavia, nel formato attuale come parsato dal codice di AldebaranPrimo e hoberman, la struttura e' come sopra.

### 3.2 Decryption AES-CTR

1. Estrarre il nonce a 16 bit da byte [5] e [6]
2. Costruire il counter block (16 byte): `{ nonce_lo, nonce_hi, 0x00, 0x00, ... 0x00 }` (primi 2 byte = nonce, resto = 0)
3. Usare AES-128-CTR con la chiave del dispositivo (16 byte) e il counter block
4. Decrittografare i byte [8..] per ottenere i dati in chiaro
5. Parsare secondo la struttura del record type

### 3.3 Frequenza Advertisement

- Intervallo normale: ~1 secondo (in realta' variabile, tipicamente 100-1000ms)
- Quando VictronConnect e' connesso al dispositivo: l'intervallo cambia a ~350ms ma il tipo di advertisement cambia da ADV_IND a ADV_SCAN_IND
- **CRITICO**: quando VictronConnect e' connesso via BLE a un dispositivo, quel dispositivo potrebbe smettere di trasmettere Instant Readout data o trasmettere dati diversi. Disconnettersi prima di testare.

### 3.4 Prerequisiti sui Dispositivi Victron

Per OGNI dispositivo Victron:
1. Firmware v3.61+ (verificare con VictronConnect)
2. In VictronConnect: Settings > Product Info > abilitare "Instant readout via Bluetooth"
3. Premere SHOW su "Instant Readout Details" per ottenere la Encryption Key (32 caratteri hex)
4. Annotare il MAC address

---

## 4. Architettura Firmware

### 4.1 Approccio Consigliato: PlatformIO + Arduino Framework

Motivazioni:
- Libreria M5StickC ufficiale per gestione hardware (AXP192, display, pulsanti)
- Libreria NimBLE-Arduino per BLE (piu' efficiente in termini di RAM rispetto alla BLE library default)
- PubSubClient per MQTT
- Codice di riferimento testato su M5StickC (hoberman/Victron_BLE_Scanner_Display)
- Pieno controllo su memoria e comportamento

### 4.2 Stack Software

```
+---------------------------------------------+
|              Applicazione                    |
|  - BLE scan callback + parsing              |
|  - AES-CTR decryption                       |
|  - MQTT publish                             |
|  - Display status (opzionale, minimale)     |
+---------------------------------------------+
|     NimBLE-Arduino   |  PubSubClient (MQTT) |
+---------------------------------------------+
|     WiFi (ESP-IDF)   |  mbedtls (AES)       |
+---------------------------------------------+
|           Arduino-ESP32 Core                 |
+---------------------------------------------+
|              ESP-IDF / FreeRTOS              |
+---------------------------------------------+
|           ESP32-PICO-D4 Hardware             |
+---------------------------------------------+
```

### 4.3 Flusso di Esecuzione

```
setup():
  1. M5.begin() -> inizializza AXP192, display, seriale
  2. Display: mostra "Victron BLE Gateway - Connecting..."
  3. WiFi.begin(ssid, password) -> connessione WiFi
  4. mqttClient.setServer(broker, port) -> configura MQTT
  5. NimBLEDevice::init("") -> inizializza stack BLE
  6. Crea BLE scanner con callback
  7. Avvia scan passivo continuo

loop():
  1. Mantieni connessione WiFi (reconnect se necessario)
  2. Mantieni connessione MQTT (reconnect se necessario)
  3. Se nuovi dati disponibili da callback BLE:
     a. Pubblica su MQTT come JSON
     b. Aggiorna display (opzionale)
  4. M5.update() -> gestione pulsanti
  5. delay minimo per yield

BLE Scan Callback (eseguito in contesto BLE task):
  1. Verifica manufacturer data presente
  2. Verifica Vendor ID == 0x02E1
  3. Match MAC address contro dispositivi noti
  4. Seleziona chiave AES corrispondente
  5. Estrai nonce e dati crittografati
  6. Decrypta con AES-CTR
  7. Parsa secondo record type
  8. Salva in struttura dati globale (con mutex)
  9. Setta flag "nuovi dati disponibili"
```

### 4.4 Struttura File Progetto

```
victron-ble-gateway/
├── platformio.ini              # Configurazione PlatformIO
├── src/
│   ├── main.cpp                # Entry point, setup/loop
│   ├── config.h                # Configurazione WiFi, MQTT, chiavi AES, MAC
│   ├── victron_ble.h           # Header: strutture dati Victron, prototipi
│   ├── victron_ble.cpp         # BLE scan, decrypt, parse
│   ├── mqtt_publisher.h        # Header MQTT
│   ├── mqtt_publisher.cpp      # Pubblicazione MQTT
│   ├── display_ui.h            # Header display (opzionale)
│   └── display_ui.cpp          # UI minimale su display (opzionale)
└── README.md
```

---

## 5. Configurazione e Dipendenze

### 5.1 platformio.ini

**Opzione A - Partendo dal repo AldebaranPrimo (BLE Arduino standard):**

```ini
[env:m5stick-c]
platform = espressif32
board = m5stick-c
framework = arduino
monitor_speed = 115200
lib_deps =
    m5stack/M5StickC@^0.2.9          ; M5StickC (NON Plus!)
    ESP32 BLE Arduino                 ; libreria BLE standard (come nel progetto base)
    knolleary/PubSubClient@^2.8       ; MQTT client (DA AGGIUNGERE)
```

**Opzione B - Con NimBLE (se la RAM non basta con Opzione A):**

```ini
[env:m5stick-c]
platform = espressif32
board = m5stick-c
framework = arduino
monitor_speed = 115200
lib_deps =
    m5stack/M5StickC@^0.2.9
    h2zero/NimBLE-Arduino@^1.4.3      ; sostituisce ESP32 BLE Arduino
    knolleary/PubSubClient@^2.8
build_flags =
    -D CONFIG_BT_NIMBLE_ROLE_CENTRAL=1
    -D CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=0
    -D CONFIG_BT_NIMBLE_ROLE_OBSERVER=1
    -D CONFIG_BT_NIMBLE_ROLE_BROADCASTER=0
    -D CONFIG_BT_NIMBLE_MAX_CONNECTIONS=0
    -D CONFIG_BT_NIMBLE_PINNED_TO_CORE=0
```

**Note:**
- **Partire SEMPRE dall'Opzione A** (BLE Arduino standard), perche' il codice base del repo AldebaranPrimo lo usa e funziona
- Se dopo l'aggiunta di WiFi+MQTT il free heap scende sotto 40-50 KB, passare all'Opzione B (NimBLE)
- Con NimBLE: disabilitare i ruoli non necessari (Peripheral, Broadcaster) risparmia ~30-50 KB di RAM
- `MAX_CONNECTIONS=0` perche' facciamo solo scan passivo, non ci connettiamo ai dispositivi
- NimBLE-Arduino e' significativamente piu' leggero in RAM rispetto alla libreria BLE default (~60 KB vs ~170 KB)

### 5.2 config.h - Template

```cpp
#ifndef CONFIG_H
#define CONFIG_H

// ===== WiFi =====
#define WIFI_SSID     "TuaReteWiFi"
#define WIFI_PASSWORD "TuaPasswordWiFi"

// ===== MQTT =====
#define MQTT_BROKER   "192.168.1.xxx"   // IP del broker (Mosquitto)
#define MQTT_PORT     1883
#define MQTT_USER     ""                 // vuoto se senza autenticazione
#define MQTT_PASS     ""
#define MQTT_CLIENT_ID "victron-ble-gw"
#define MQTT_BASE_TOPIC "victron"        // topic base, es: victron/mppt/pv_power

// ===== Intervalli =====
#define MQTT_PUBLISH_INTERVAL_MS 5000    // Pubblica ogni 5 secondi
#define BLE_SCAN_WINDOW_MS       400     // Scan window (>350ms per catturare adv Victron)
#define BLE_SCAN_INTERVAL_MS     500     // Scan interval
#define WIFI_RECONNECT_INTERVAL_MS 5000
#define MQTT_RECONNECT_INTERVAL_MS 5000

// ===== Dispositivo 1: SmartSolar MPPT (impianto solare CASA) =====
#define MPPT_ENABLED true
#define MPPT_MAC     "XX:XX:XX:XX:XX:XX"    // DA OTTENERE da VictronConnect
#define MPPT_AES_KEY "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"  // 32 hex chars da VictronConnect

// ===== Dispositivo 2: SmartBatterySense (impianto solare CASA) =====
#define BATTERY_SENSE_ENABLED true
#define BATTERY_SENSE_MAC     "XX:XX:XX:XX:XX:XX"
#define BATTERY_SENSE_AES_KEY "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

// ===== Dispositivo 3: SmartShunt (impianto solare CASA) =====
#define SMARTSHUNT_ENABLED true
#define SMARTSHUNT_MAC     "XX:XX:XX:XX:XX:XX"
#define SMARTSHUNT_AES_KEY "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

#endif
```

**ATTENZIONE**: questi sono dispositivi dell'**impianto solare di casa**, NON quelli del camper 
(che hanno MAC e chiavi diverse, usate nel progetto VictronSolarDisplayEsp). Prima di compilare, 
ricavare MAC e chiavi AES da VictronConnect per ciascuno dei 3 dispositivi di casa.

---

## 6. Strutture Dati Victron

### 6.1 Struttura Manufacturer Data BLE (pre-decryption)

```cpp
// Struttura del manufacturer data come ricevuto dal BLE advertisement
// NOTA: i campi sono packed e little-endian
typedef struct __attribute__((packed)) {
    uint16_t vendorID;          // 0x02E1 (Victron Energy)
    uint16_t productID;         // Product ID (es. 0xA389 per SmartShunt)
    uint8_t  recordType;        // 0x01=Solar, 0x02=BattMon, 0x08=Charger
    uint16_t nonceDataCounter;  // Nonce per AES-CTR (little-endian)
    uint8_t  encryptKeyMatch;   // Primo byte della chiave AES (per verifica rapida)
    uint8_t  encryptedData[];   // Dati crittografati (lunghezza variabile)
} VictronManufacturerData;
```

### 6.2 Struttura Dati Parsati

```cpp
// Dati SmartSolar MPPT (Record Type 0x01)
typedef struct {
    uint8_t  deviceState;    // 0=OFF,3=Bulk,4=Absorption,5=Float,...
    uint8_t  chargerError;   // 0=no error
    float    batteryVoltage; // V
    float    batteryCurrent; // A  
    float    yieldToday;     // kWh
    uint16_t pvPower;        // W
    float    loadCurrent;    // A
    bool     valid;          // dati validi (almeno un adv ricevuto)
    uint32_t lastUpdate;     // millis() dell'ultimo aggiornamento
} MpptData;

// Dati SmartBatterySense (Record Type 0x02, PID 0xA3A4/0xA3A5)
typedef struct {
    float    batteryVoltage; // V
    float    temperature;    // gradi C (da Kelvin*100 - 273.15)
    bool     valid;
    uint32_t lastUpdate;
} BatterySenseData;

// Dati SmartShunt (Record Type 0x02, PID 0xA389-0xA38B)
typedef struct {
    float    soc;            // % (0-100)
    float    batteryVoltage; // V
    float    batteryCurrent; // A (positivo = carica, negativo = scarica)
    uint16_t timeToGo;       // minuti (0xFFFF = infinito/non disponibile)
    float    consumedAh;     // Ah
    uint16_t alarmReason;    // bitfield
    bool     valid;
    uint32_t lastUpdate;
} SmartShuntData;
```

### 6.3 Parsing dei Bit-Fields (Record Type 0x02 - Battery Monitor)

Il parsing dei dati decriptati per il record type 0x02 richiede estrazione a livello di bit. I campi NON sono allineati a byte.

```cpp
// Esempio di parsing per Record Type 0x02
// decrypted[] = array di byte decriptati
// NOTA: i campi sono in ordine little-endian a livello di bit

// TTG: bit 0-15 (2 byte, uint16, little-endian)
uint16_t ttg = decrypted[0] | (decrypted[1] << 8);

// Battery voltage: bit 16-31 (int16, * 0.01 per volt)
int16_t rawVoltage = decrypted[2] | (decrypted[3] << 8);
float voltage = rawVoltage * 0.01f;

// Alarm reason: bit 32-47
uint16_t alarm = decrypted[4] | (decrypted[5] << 8);

// I campi successivi richiedono bit-level extraction:
// Aux value: bit 48-69 (22 bit)
// Aux mode: bit 70-71 (2 bit)  
// Current: bit 72-93 (22 bit, signed)
// Consumed Ah: bit 94-113 (20 bit)
// SOC: bit 114-123 (10 bit)

// Aux + Aux mode (bit 48-71 = 3 byte: decrypted[6], [7], [8])
uint32_t auxRaw = decrypted[6] | (decrypted[7] << 8) | (decrypted[8] << 16);
uint32_t auxValue = auxRaw & 0x3FFFFF;        // bit 0-21 del gruppo (= bit 48-69 globali)
uint8_t  auxMode  = (auxRaw >> 22) & 0x03;    // bit 22-23 del gruppo (= bit 70-71 globali)

// Current: bit 72-93 (decrypted[9], [10], [11] parziale)
uint32_t currentRaw = decrypted[9] | (decrypted[10] << 8) | (decrypted[11] << 16);
int32_t currentSigned = currentRaw & 0x3FFFFF; // 22 bit
if (currentSigned & 0x200000) currentSigned |= 0xFFC00000; // sign extend
float current = currentSigned * 0.001f; // mA -> A

// NOTA: il parsing esatto dei bit per consumed_ah e SOC dipende dall'allineamento.
// Consumed Ah: bit 94-113 
// SOC: bit 114-123
// Questi attraversano i confini dei byte e richiedono shift accurati.
// Fare riferimento al codice di AldebaranPrimo/victron_ble.c o 
// Fabian-Schmidt/esphome-victron_ble per l'implementazione corretta.
```

**ATTENZIONE CRITICA**: il parsing bit-level e' la parte piu' soggetta a errori. I campi attraversano confini di byte e richiedono attenzione all'endianness e al sign-extension. **Fare sempre riferimento a implementazioni funzionanti note**:
- `AldebaranPrimo/VictronSolarDisplayEsp/components/victron_ble/victron_ble.c`
- `Fabian-Schmidt/esphome-victron_ble` (C++)
- `keshavdv/victron-ble` (Python, usa la libreria `construct`)
- Documento ufficiale: `extra-manufacturer-data-2022-12-14.pdf`

### 6.4 Parsing Record Type 0x01 (Solar Charger)

Piu' semplice del 0x02 perche' ha meno campi bit-packed:

```cpp
// decrypted[] = array di byte decriptati (tipicamente 12 byte)
uint8_t deviceState = decrypted[0];
uint8_t chargerError = decrypted[1];
int16_t rawVoltage = decrypted[2] | (decrypted[3] << 8);
float batteryVoltage = rawVoltage * 0.01f;

// Battery current: 10 bit signed (bit 32-41)
uint16_t rawCurrent = decrypted[4] | (decrypted[5] << 8);
int16_t currentBits = rawCurrent & 0x03FF;
if (currentBits & 0x0200) currentBits |= 0xFC00; // sign extend 10 bit
float batteryCurrent = currentBits * 0.1f;

// Yield today: 10 bit (bit 42-51)
uint16_t yieldBits = (rawCurrent >> 10) | ((decrypted[6] & 0x0F) << 6);
float yieldToday = (yieldBits & 0x03FF) * 0.01f;

// PV Power: 16 bit (bit 52-67)
uint16_t pvPower = ((decrypted[6] >> 4) & 0x0F) | (decrypted[7] << 4) | ((decrypted[8] & 0x0F) << 12);
// NOTA: il parsing esatto dei bit-field potrebbe variare.
// Verificare con implementazione di riferimento.

// Load current: 9 bit (bit 68-76)
// ...
```

**NOTA IMPORTANTE**: il codice di parsing sopra e' illustrativo. L'implementazione DEVE essere verificata contro il documento ufficiale Victron e contro il codice funzionante di AldebaranPrimo. Gli shift e le maschere esatte dipendono dall'allineamento bit specifico del protocollo.

---

## 7. MQTT - Struttura Topic e Payload

### 7.1 Topic Structure

```
victron/mppt/state              -> JSON con tutti i dati MPPT
victron/battery_sense/state     -> JSON con tutti i dati BatterySense
victron/smartshunt/state        -> JSON con tutti i dati SmartShunt
victron/gateway/status          -> JSON con stato del gateway
```

### 7.2 Payload JSON

**MPPT:**
```json
{
  "device_state": 3,
  "device_state_text": "Bulk",
  "charger_error": 0,
  "battery_voltage": 13.45,
  "battery_current": 2.1,
  "yield_today": 1.23,
  "pv_power": 284,
  "load_current": 0.5,
  "rssi": -67,
  "timestamp": 1710504000
}
```

**SmartBatterySense:**
```json
{
  "battery_voltage": 13.44,
  "temperature": 22.5,
  "rssi": -72,
  "timestamp": 1710504000
}
```

**SmartShunt:**
```json
{
  "soc": 87.0,
  "battery_voltage": 13.45,
  "battery_current": 2.5,
  "time_to_go_min": 765,
  "consumed_ah": 5.4,
  "alarm_reason": 0,
  "rssi": -65,
  "timestamp": 1710504000
}
```

**Gateway Status:**
```json
{
  "wifi_rssi": -45,
  "uptime_sec": 3600,
  "free_heap": 125000,
  "mppt_last_seen_sec": 2,
  "battery_sense_last_seen_sec": 3,
  "smartshunt_last_seen_sec": 1
}
```

### 7.3 Integrazione Home Assistant

Con MQTT auto-discovery (opzionale), il gateway pubblica anche su `homeassistant/sensor/victron_ble_gw/...` per auto-configurazione. In alternativa, i sensori si configurano manualmente nel `configuration.yaml` di HA tramite MQTT sensor platform.

Esempio manuale in HA:
```yaml
mqtt:
  sensor:
    - name: "MPPT PV Power"
      state_topic: "victron/mppt/state"
      value_template: "{{ value_json.pv_power }}"
      unit_of_measurement: "W"
      device_class: power
    - name: "SmartShunt SOC"
      state_topic: "victron/smartshunt/state"
      value_template: "{{ value_json.soc }}"
      unit_of_measurement: "%"
      device_class: battery
    - name: "Battery Temperature"
      state_topic: "victron/battery_sense/state"
      value_template: "{{ value_json.temperature }}"
      unit_of_measurement: "°C"
      device_class: temperature
```

---

## 8. Riferimenti Codice Verificati e Funzionanti

### 8.0 Repository Base Consigliata (USARE QUESTA)

**AldebaranPrimo/Victron_BLE_Scanner_Display** (PlatformIO, Arduino, C++)
- URL: https://github.com/AldebaranPrimo/Victron_BLE_Scanner_Display
- Fork di hoberman, esteso dallo stesso autore del progetto ESP-IDF analizzato
- **GIA' funzionante su M5StickC Plus** (stesso SoC ESP32-PICO-D4 del M5StickC)
- **GIA' supporta tutti e 3 i dispositivi**: SmartSolar MPPT, SmartBatterySense, SmartShunt
- **GIA' in formato PlatformIO** con `platformio.ini` e sorgenti in `src/`
- Parsing bit-packed per Record Type 0x02 gia' implementato e testato
- Conversione temperatura Kelvin->Celsius per BatterySense gia' implementata
- Struttura device config pulita con enum per tipo dispositivo:
  ```cpp
  enum deviceType { DEVICE_SOLAR_CHARGER, DEVICE_SMART_SHUNT, DEVICE_BATTERY_SENSE };
  struct victronDevice {
    char* mac;
    char* key;
    char* name;
    deviceType type;
    // ...
  };
  ```
- Usa `ESP32 BLE Arduino` (libreria BLE standard Bluedroid), NON NimBLE
- Display a 2 pagine: SOLAR (MPPT data) e INFO (BatterySense + SmartShunt)
- Pulsante A = cambio pagina, Pulsante B = rotazione display

**STRATEGIA DI SVILUPPO CONSIGLIATA**: partire da questo repo e aggiungere:
1. WiFi station mode
2. Client MQTT (PubSubClient)
3. Pubblicazione JSON periodica dei dati gia' parsati
4. (opzionale) Adattamento display per M5StickC (80x160 vs 135x240 del Plus)

**ATTENZIONE CRITICA SULLA LIBRERIA BLE**: questo progetto usa `ESP32 BLE Arduino` (Bluedroid), che consuma ~170 KB di RAM. Per aggiungere WiFi+MQTT senza PSRAM, potrebbe essere necessario migrare a NimBLE-Arduino (~60 KB di RAM). Se durante i test il free heap scende sotto 30 KB dopo l'aggiunta di WiFi+MQTT, la migrazione a NimBLE diventa obbligatoria. Le API sono simili ma non identiche.

**Differenze M5StickC vs M5StickC Plus (rilevanti per il codice)**:
- Display: ST7735S 80x160 (M5StickC) vs ST7789V2 135x240 (Plus) - richiede cambio libreria display
- Libreria: `M5StickC` vs `M5StickCPlus` - cambio include e init
- Batteria: 80mAh vs 120mAh - solo autonomia, non impatta il codice
- SoC identico: ESP32-PICO-D4 su entrambi

### 8.1 Altri Repository di Riferimento

1. **AldebaranPrimo/VictronSolarDisplayEsp** (ESP-IDF, C)
   - URL: https://github.com/AldebaranPrimo/VictronSolarDisplayEsp
   - Supporta: MPPT (0x01), SmartShunt (0x02), BatterySense (0x02), AC Charger (0x08)
   - Identificazione: MAC-based
   - Target: ESP32-WROOM-32E (stesso SoC family del PICO-D4)
   - **Il file chiave per il parsing e' `components/victron_ble/victron_ble.c`**

2. **hoberman/Victron_BLE_Scanner_Display** (Arduino, C++)
   - URL: https://github.com/hoberman/Victron_BLE_Scanner_Display
   - Supporta: SmartSolar MPPT (0x01)
   - **Scritto specificamente per M5StickC e M5StickCPlus**
   - Usa Arduino BLE library (NON NimBLE)
   - Chiavi come stringhe hex convertite in byte array a runtime

3. **hoberman/Victron_BLE_Advertising_example** (Arduino, C++)
   - URL: https://github.com/hoberman/Victron_BLE_Advertising_example
   - Esempio minimale di decrypt e parse per SmartSolar
   - Buon punto di partenza per capire il protocollo

4. **Fabian-Schmidt/esphome-victron_ble** (ESPHome/C++)
   - URL: https://github.com/Fabian-Schmidt/esphome-victron_ble
   - Implementazione completa e matura di tutti i record types
   - Codice C++ di alta qualita' per il parsing
   - Riferimento autorevole per le strutture dati

### 8.2 Documentazione Protocollo

- **Documento ufficiale Victron:** `extra-manufacturer-data-2022-12-14.pdf` (disponibile nel repo wytr/VictronSolarDisplayEsp sotto `docs/`)
- **Victron Community thread:** https://community.victronenergy.com/questions/187303/victron-bluetooth-advertising-protocol.html

---

## 9. Gotcha e Problemi Noti

### 9.1 Errori Comuni (da evitare assolutamente)

1. **String vs std::string**: le versioni recenti di ESP32 Arduino core hanno cambiato `BLEAdvertisedDevice.getManufacturerData()` da `std::string` a `String`. Usare `.c_str()` e `.length()` in modo compatibile. Hoberman ha un `#define USE_String` per gestire questa differenza.

2. **Byte zero nel manufacturer data**: il manufacturer data puo' contenere byte 0x00. `String.toCharArray()` e `String.getBytes()` si interrompono prematuramente su byte 0x00. Usare `.c_str()` per ottenere il puntatore raw ai dati e `.length()` per la lunghezza.

3. **MAC address byte order**: i MAC nel BLE sono trasmessi in ordine inverso. Il MAC `C1:56:39:B4:7D:B5` nei raw advertisement data appare come `B5:7D:B4:39:56:C1`. NimBLE gestisce questo internamente e restituisce il MAC in formato human-readable, ma se si confrontano bytes raw, attenzione all'ordine.

4. **AES-CTR counter block**: il nonce e' solo 16 bit (2 byte) ma il counter block AES e' 16 byte. I primi 2 byte sono il nonce (little-endian), i restanti 14 byte sono TUTTI ZERO. NON usare un nonce piu' lungo.

5. **Sign extension dei campi signed**: i campi come battery_current (10 bit signed per MPPT, 22 bit signed per Battery Monitor) richiedono sign extension manuale quando vengono estratti da campi piu' larghi. Un errore comune e' dimenticare il sign extension, ottenendo valori sempre positivi.

6. **SmartBatterySense vs SmartShunt**: entrambi hanno Record Type 0x02 ma i campi validi sono diversi. Non tentare di leggere SOC/current/TTG da un BatterySense - i valori non sono significativi.

7. **Coesistenza WiFi+BLE su ESP32 senza PSRAM**: lo stack BLE default di ESP32 (Bluedroid) consuma ~170 KB di RAM. NimBLE consuma ~60 KB. Su un dispositivo senza PSRAM con 520 KB SRAM, usare **sempre NimBLE** per lasciare RAM sufficiente a WiFi+MQTT+applicazione.

8. **VictronConnect interferenza**: mentre VictronConnect e' connesso a un dispositivo, l'Instant Readout potrebbe non essere trasmesso. Disconnettere l'app prima di testare.

9. **Scan window**: Victron raccomanda una scan window >= 400ms perche' l'intervallo di advertisement puo' essere ~350ms. Se la window e' troppo corta, si perdono pacchetti.

10. **AXP192 su M5StickC**: senza inizializzazione dell'AXP192 (che `M5.begin()` fa automaticamente), il display e altri periferici potrebbero non ricevere alimentazione. Se si usa ESP-IDF puro senza la libreria M5, bisogna inizializzare l'AXP192 manualmente via I2C.

11. **Migrazione da M5StickC Plus a M5StickC**: il progetto AldebaranPrimo/Victron_BLE_Scanner_Display usa la libreria `M5StickCPlus` (display ST7789V2 135x240). Per il M5StickC (prima generazione, display ST7735S 80x160) bisogna:
    - Cambiare `#include <M5StickCPlus.h>` in `#include <M5StickC.h>`
    - Cambiare la lib_deps in platformio.ini da `m5stack/M5StickCPlus` a `m5stack/M5StickC`
    - Adattare le coordinate e le dimensioni del testo per il display piu' piccolo (80x160 vs 135x240)
    - Le API di M5.Lcd sono compatibili tra le due librerie (stesse funzioni, solo dimensioni diverse)
    - Il board in platformio.ini rimane `m5stick-c` per entrambi

12. **Libreria BLE e RAM**: il progetto base usa `ESP32 BLE Arduino` (Bluedroid, ~170KB RAM). Con solo BLE scan + display funziona. Aggiungendo WiFi+MQTT si rischia out-of-memory. Monitorare `ESP.getFreeHeap()` subito dopo l'inizializzazione di WiFi+BLE. Se sotto 50KB, migrare a NimBLE. La migrazione richiede:
    - Cambiare include da `<BLEDevice.h>/<BLEScan.h>/<BLEAdvertisedDevice.h>` a `<NimBLEDevice.h>`
    - Le API di NimBLE sono simili ma i tipi di callback cambiano leggermente
    - `getManufacturerData()` ritorna `std::string` in NimBLE (non `String`)
    - La struct `BLEAdvertisedDevice` diventa `NimBLEAdvertisedDevice`

### 9.2 Limiti di Memoria - Dati Verificati

Budget RAM basato su **misurazioni reali** dalla community ESP32 (no PSRAM, 520 KB SRAM):

| Scenario | Free Heap Misurato | Fonte |
|---|---|---|
| Solo boot (nulla inizializzato) | ~360 KB | Blog scottyob.com |
| + WiFi connesso | ~263 KB (~97 KB consumati) | Blog scottyob.com |
| + WiFi + BLE Bluedroid | ~148 KB (~212 KB consumati) | Blog scottyob.com |
| + WiFi + BLE NimBLE | ~250 KB (~110 KB consumati) | Forum ESP32 |
| + WiFi + BLE Bluedroid + HTTPS/TLS | ~45 KB (CRITICO) | GitHub issue #2175 |

**Verdetto per M5StickC (prima generazione, no PSRAM):**

Con **Bluedroid + WiFi + MQTT plain (no TLS)**: ~148 KB liberi. PubSubClient consuma ~2-3 KB. Margine sufficiente (~140+ KB liberi). **La prima generazione M5StickC funziona.**

Con **NimBLE + WiFi + MQTT plain**: ~250 KB liberi. Margine abbondante.

**ATTENZIONE**: se in futuro si volesse MQTT con TLS (porta 8883), servirebbero ~40 KB extra per i buffer SSL. Con Bluedroid si scenderebbe a ~45 KB liberi (rischioso). In quel caso NimBLE diventa obbligatorio.

**Raccomandazione**: partire con Bluedroid (come nel progetto base di Marco). Se servisse TLS o si aggiungessero funzionalita', migrare a NimBLE.

---

## 10. Specifica Display

### 10.1 Layout

Display ST7735S 80x160 pixel, orientamento landscape (160x80). Sfondo nero.

Tre righe di dati, ciascuna con valore in Watt e font il piu' grande possibile per la leggibilita':

```
┌────────────────────────────────────┐
│  ☀  284 W                    BLU  │  <- Potenza solare (PV Power da MPPT)
│  🔋 +142 W                  VERDE │  <- Potenza batteria (V * I da SmartShunt)
│  ⚡  142 W                  GIALLO│  <- Potenza consumata (solare - batteria)
└────────────────────────────────────┘
```

### 10.2 Dettaglio Valori

**Riga 1 - Potenza Solare (BLU, colore 0x001F o simile visibile)**
- Fonte: campo `pv_power` dal record MPPT (Record Type 0x01)
- Unita': W (intero, senza decimali)
- Sempre >= 0
- Se dato non disponibile: "-- W"

**Riga 2 - Potenza Batteria (VERDE se carica / ROSSO se scarica)**
- Fonte: `battery_voltage * battery_current` dallo SmartShunt (Record Type 0x02)
- `battery_current` dallo SmartShunt e' positivo in carica, negativo in scarica
- Calcolo: `watt_batteria = voltage * current` (il segno viene dal current)
- Colore: **VERDE** (0x07E0) se `watt_batteria >= 0` (carica), **ROSSO** (0xF800) se `watt_batteria < 0` (scarica)
- Mostrare il segno: "+142 W" in carica, "-85 W" in scarica
- Se dato non disponibile: "-- W"

**Riga 3 - Potenza Consumata (GIALLO, 0xFFE0)**
- Calcolo: `watt_consumo = pv_power - watt_batteria`
  - Se solare = 284W e batteria = +142W (carica) -> consumo = 284 - 142 = 142W
  - Se solare = 0W e batteria = -85W (scarica) -> consumo = 0 - (-85) = 85W
  - Se solare = 200W e batteria = -50W (scarica nonostante sole) -> consumo = 200 - (-50) = 250W
- Sempre mostrato come valore assoluto positivo (il consumo e' sempre "consumo")
- Se uno dei due dati sorgente non e' disponibile: "-- W"

### 10.3 Note Implementative Display

- La libreria M5StickC fornisce `M5.Lcd` con API TFT_eSPI compatibile
- Per font grande usare `M5.Lcd.setTextSize(2)` o `M5.Lcd.setTextSize(3)` - testare quale sta nel display 160x80
- Per evitare flickering: sovrascrivere il testo precedente con sfondo nero (`M5.Lcd.setTextColor(color, TFT_BLACK)`) oppure ridisegnare solo quando il valore cambia
- Coordinate approssimative in landscape 160x80: riga 1 y=2, riga 2 y=28, riga 3 y=54 (da calibrare)
- Un piccolo indicatore in alto a destra per WiFi/MQTT status (es. puntino verde = connesso, rosso = disconnesso)
- Rotazione display: `M5.Lcd.setRotation(1)` per landscape

### 10.4 Colori RGB565

| Colore | Nome | Valore RGB565 | Uso |
|---|---|---|---|
| Blu | TFT_BLUE | 0x001F | Potenza solare |
| Blu chiaro | - | 0x07FF (cyan) | Alternativa solare se blu scuro poco leggibile |
| Verde | TFT_GREEN | 0x07E0 | Batteria in carica |
| Rosso | TFT_RED | 0xF800 | Batteria in scarica |
| Giallo | TFT_YELLOW | 0xFFE0 | Consumo |
| Bianco | TFT_WHITE | 0xFFFF | Testo secondario |
| Nero | TFT_BLACK | 0x0000 | Sfondo |

**NOTA**: il blu puro (0x001F) su display TFT piccoli puo' risultare poco leggibile su sfondo nero. Testare con cyan (0x07FF) come alternativa. Il progetto originale di Marco usa cyan per i voltage readings.

---

## 11. Step di Sviluppo Concreti (per Claude Code)

### 10.1 Step 1 - Clonare e adattare il progetto base

1. Clonare `https://github.com/AldebaranPrimo/Victron_BLE_Scanner_Display`
2. In `platformio.ini`: cambiare `M5StickCPlus` con `M5StickC`
3. In `src/main.cpp`: cambiare `#include <M5StickCPlus.h>` con `#include <M5StickC.h>`
4. Adattare le coordinate display da 135x240 a 80x160
5. Compilare e flashare: deve mostrare le pagine display (senza dati Victron)
6. Verificare con `Serial.printf("Free heap: %d\n", ESP.getFreeHeap())`

### 10.2 Step 2 - Aggiungere WiFi

1. Aggiungere `#include <WiFi.h>` e funzione `setupWiFi()` con `WiFi.begin()`
2. Nel loop: reconnect WiFi se disconnesso
3. Mostrare IP e stato WiFi sul display
4. Verificare free heap dopo WiFi init (dovrebbe essere >100 KB)

### 10.3 Step 3 - Aggiungere MQTT

1. Aggiungere `PubSubClient` alle lib_deps
2. Implementare `setupMQTT()` e `reconnectMQTT()`
3. Nel loop: `mqttClient.loop()` per mantenere la connessione
4. Test: pubblicare un messaggio statico su un topic di test
5. Verificare free heap (punto critico: se <40 KB con BLE+WiFi+MQTT, passare a NimBLE)

### 10.4 Step 4 - Collegare BLE scan a MQTT publish

1. Le strutture dati Victron sono gia' popolate dal BLE callback esistente
2. Aggiungere una funzione `publishMQTT()` che legge le strutture e costruisce JSON
3. Chiamare `publishMQTT()` ogni N secondi (es. 5000ms) nel loop principale
4. Usare `ArduinoJson` oppure `snprintf` per costruire i JSON payload
5. Pubblicare su topic separati per dispositivo

### 10.5 Step 5 - Stabilizzazione

1. Test di funzionamento continuo 24h+
2. Monitorare free heap minimo via MQTT (topic gateway status)
3. Gestire watchdog: se BLE o WiFi si bloccano, restart automatico
4. (opzionale) Aggiungere MQTT auto-discovery per Home Assistant

---

## 12. Procedura di Test

### 12.1 Test Incrementale

1. **Fase 1 - Hardware base**: flash di uno sketch M5StickC di esempio per verificare che display, pulsanti e seriale funzionino
2. **Fase 2 - WiFi**: connessione WiFi e stampa dell'IP su display e seriale
3. **Fase 3 - MQTT**: connessione al broker e pubblicazione di un messaggio di test
4. **Fase 4 - BLE scan**: scan BLE e log dei manufacturer data Victron su seriale (senza decrypt)
5. **Fase 5 - Decrypt**: aggiungere decryption AES-CTR e verificare che i valori corrispondano a VictronConnect
6. **Fase 6 - Integrazione**: pubblicazione MQTT dei dati decrittografati + verifica ricezione in Node-RED/HA
7. **Fase 7 - Stabilita'**: test di funzionamento continuo per 24+ ore, monitorando free heap e riconnessioni

### 12.2 Verifica Valori

Per ogni dispositivo, confrontare i valori pubblicati via MQTT con quelli mostrati nell'app VictronConnect (schermata Overview). I valori devono corrispondere con tolleranza di +-0.01V per voltage, +-0.1A per current, +-1% per SOC.

**NOTA**: durante la verifica, l'app VictronConnect deve essere DISCONNESSA dal dispositivo (tornare alla schermata di lista dispositivi) altrimenti l'Instant Readout potrebbe non essere trasmesso.

### 12.3 Monitoraggio Heap

Aggiungere nel loop periodico (ogni 30 secondi):
```cpp
Serial.printf("Free heap: %d bytes, Min free: %d bytes\n", 
              ESP.getFreeHeap(), ESP.getMinFreeHeap());
```
Se `ESP.getMinFreeHeap()` scende sotto 30 KB, c'e' rischio di crash. Ridurre i buffer o disabilitare funzionalita' non essenziali.

---

## 13. Approccio Alternativo: ESPHome (Solo YAML)

Se si preferisce evitare lo sviluppo custom, ESPHome con il componente `esphome-victron_ble` funziona su ESP32-PICO-D4 (M5StickC) ed e' una soluzione no-code.

**File YAML completo:**

```yaml
esphome:
  name: victron-ble-gateway
  friendly_name: "Victron BLE Gateway"

esp32:
  board: m5stick-c
  framework:
    type: arduino

logger:
  level: INFO

wifi:
  ssid: "TuaRete"
  password: "TuaPassword"
  ap:
    ssid: "Victron-BLE-Fallback"
    password: "fallbackpwd"

captive_portal:

mqtt:
  broker: 192.168.1.xxx
  port: 1883

ota:
  platform: esphome

external_components:
  - source: github://Fabian-Schmidt/esphome-victron_ble

esp32_ble_tracker:
  scan_parameters:
    interval: 500ms
    window: 400ms
    active: false

victron_ble:
  - id: mppt
    mac_address: "XX:XX:XX:XX:XX:XX"
    bindkey: "chiave32hex"
  - id: battery_sense
    mac_address: "YY:YY:YY:YY:YY:YY"
    bindkey: "chiave32hex"
  - id: smartshunt
    mac_address: "ZZ:ZZ:ZZ:ZZ:ZZ:ZZ"
    bindkey: "chiave32hex"

sensor:
  - platform: victron_ble
    victron_ble_id: mppt
    name: "MPPT PV Power"
    type: PV_POWER
  - platform: victron_ble
    victron_ble_id: mppt
    name: "MPPT Battery Voltage"
    type: BATTERY_VOLTAGE
  - platform: victron_ble
    victron_ble_id: mppt
    name: "MPPT Battery Current"
    type: BATTERY_CURRENT
  - platform: victron_ble
    victron_ble_id: mppt
    name: "MPPT Yield Today"
    type: YIELD_TODAY
  - platform: victron_ble
    victron_ble_id: battery_sense
    name: "Battery Temperature"
    type: TEMPERATURE
  - platform: victron_ble
    victron_ble_id: battery_sense
    name: "BatterySense Voltage"
    type: BATTERY_VOLTAGE
  - platform: victron_ble
    victron_ble_id: smartshunt
    name: "SmartShunt SOC"
    type: STATE_OF_CHARGE
  - platform: victron_ble
    victron_ble_id: smartshunt
    name: "SmartShunt Voltage"
    type: BATTERY_VOLTAGE
  - platform: victron_ble
    victron_ble_id: smartshunt
    name: "SmartShunt Current"
    type: BATTERY_CURRENT
  - platform: victron_ble
    victron_ble_id: smartshunt
    name: "SmartShunt Time To Go"
    type: TIME_TO_GO
  - platform: victron_ble
    victron_ble_id: smartshunt
    name: "SmartShunt Consumed Ah"
    type: CONSUMED_AH

text_sensor:
  - platform: victron_ble
    victron_ble_id: mppt
    name: "MPPT Charge State"
    type: CHARGE_STATE

binary_sensor:
  - platform: victron_ble
    victron_ble_id: smartshunt
    name: "SmartShunt Alarm"
    type: ALARM
```

**NOTA sulla RAM con ESPHome**: ESPHome su ESP32 senza PSRAM con BLE tracker e' al limite. NON aggiungere componenti pesanti (web_server, bluetooth_proxy, voice_assistant). Se si verificano crash, rimuovere il logger o ridurre il livello a ERROR.

---

## 14. Schema Connessioni e Deployment

```
                    BLE Advertisements (~1/sec)
┌──────────────┐         ┌──────────────┐         ┌──────────────┐
│SmartSolar    │ )))  ((( │SmartBattery  │ )))  ((( │ SmartShunt   │
│MPPT          │         │Sense         │         │              │
│(Record 0x01) │         │(Record 0x02) │         │(Record 0x02) │
└──────────────┘         └──────────────┘         └──────────────┘
       │                        │                        │
       └────────────────────────┼────────────────────────┘
                                │
                           BLE Passive Scan
                                │
                        ┌───────┴───────┐
                        │   M5StickC    │
                        │  ESP32-PICO   │
                        │               │
                        │ BLE scan      │
                        │ AES decrypt   │
                        │ Parse records │
                        │ WiFi + MQTT   │
                        └───────┬───────┘
                                │
                            WiFi/MQTT
                                │
                        ┌───────┴───────┐
                        │  Mosquitto    │
                        │  (MQTT Broker)│
                        └───────┬───────┘
                                │
                    ┌───────────┼───────────┐
                    │                       │
            ┌───────┴───────┐       ┌───────┴───────┐
            │   Node-RED    │       │Home Assistant  │
            │   (flows)     │       │  (MQTT sensor) │
            └───────────────┘       └───────────────┘
```

Il M5StickC puo' essere alimentato via USB-C (5V) o dalla batteria interna (80mAh, circa 30-60 minuti di autonomia con WiFi+BLE attivi). Per installazione permanente, alimentare via USB-C con un alimentatore 5V.

---

## 15. Checklist Pre-Sviluppo

- [ ] Hardware M5StickC funzionante (test con sketch di esempio)
- [ ] PlatformIO installato e configurato per M5StickC
- [ ] MAC address dei 3 dispositivi Victron dell'impianto casa annotati
- [ ] Chiavi AES dei 3 dispositivi Victron di casa ottenute da VictronConnect
- [ ] "Instant readout via Bluetooth" abilitato su tutti e 3 i dispositivi di casa
- [ ] Firmware Victron v3.61+ su tutti e 3 i dispositivi di casa
- [ ] Broker MQTT (Mosquitto) raggiungibile dalla rete WiFi
- [ ] IP del broker MQTT noto
- [ ] Credenziali WiFi pronte
