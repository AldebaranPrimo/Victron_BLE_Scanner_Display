# Project Status

**Last updated:** 2026-03-15

## Current State: Funzionante con 1 device

Il firmware e' stato flashato sull'M5StickC e testato con successo:
- Configurazione via web portal: OK
- Connessione WiFi: OK
- Pubblicazione MQTT: OK, dati verificati e arrivano correttamente
- BLE scan e decrypt: OK (testato con 1 device)

### Device configurati

| Device | Tipo | Stato |
|---|---|---|
| SmartSolar MPPT | Solar Charger (0x01) | Configurato e funzionante |
| SmartBatterySense | Battery Sense (0x02) | Da configurare (non ancora montato) |
| SmartShunt | Battery Monitor (0x02) | Da configurare (non ancora montato) |

## Modifiche in attesa di upload

Le seguenti modifiche sono state fatte nel codice ma **non ancora flashate** sul device:

### 1. Fix rientro in Setup Mode (PRIORITARIO)
**File:** `src/main.cpp`
**Problema:** la procedura di long press richiedeva di tenere premuto Button B *prima* che apparisse il messaggio sul display e non tollerava nessun rilascio, rendendo praticamente impossibile attivarla.
**Fix:** riscritto `checkLongPress()` con:
- Finestra di osservazione di 5 secondi (prima erano solo 3)
- Basta premere e tenere per 3 secondi *in qualsiasi momento* della finestra
- Log seriale con countdown e stato pulsante per debug

### 2. Rimossa password dall'AP di setup
**File:** `src/config_portal.cpp`, `src/display_ui.cpp/.h`, `src/main.cpp`
**Modifica:** AP aperto senza password (era `victron123`, inutile per un setup temporaneo)
**Stato:** gia' flashato e verificato

## Prossimi step

1. **Flashare** il firmware aggiornato (fix long press)
2. **Testare** la nuova procedura di rientro in setup mode (Button B per 3s durante i 5s di boot)
3. **Aggiungere SmartBatterySense**: rientrare in setup, abilitare device 2 con MAC e chiave AES
4. **Aggiungere SmartShunt**: quando sara' montato, rientrare in setup e abilitare device 3
5. **Verificare** il display a 3 righe con tutti i device attivi
6. **Monitorare** stabilita' a lungo termine (heap, riconnessioni WiFi/MQTT)

## Build info

- **Environment:** m5stick-c
- **Partition:** partitions_noota.csv (3MB app, no OTA)
- **Flash usage:** 52.8% (1.66 MB / 3.15 MB)
- **RAM usage:** 18.6% (60.8 KB / 327.7 KB)
- **Porta seriale:** COM8 (USB Serial Port, FTDI)
