# CLAUDE.md — Victron BLE Gateway (M5StickC) (per-repo)

> Repo: `D:\_RedBones\Tomita\Victron_BLE_Gateway\`, remote GitHub **pubblico**
> `AldebaranPrimo/Victron_BLE_Scanner_Display`. Auto-caricato da Claude Code. Le regole globali di Marco restano valide.

Il contratto di famiglia (firmware ESP32 con PlatformIO) è importato qui sotto e vale per ogni sessione.

@CLAUDE-firmware-esp32-platformio.md

---

## 1. Intro

Gateway BLE → MQTT per dispositivi Victron su **M5StickC Plus** (135×240, primario) con ambiente legacy
M5StickC base ancora compilabile. È il progenitore dei firmware e-Ink e LCD del camper e della casa. Repo
**pubblico**: vale in modo rafforzato P-03, nessun dato personale (MAC, chiavi, SSID) nel codice.
Attenzione allo stato dei branch: `main` è il firmware stabile mono-dispositivo, `feature/multi-device-mppt`
è la **beta v2.0.0 non validata sul campo** (vedi avviso nel README).

## 2. Allineamento col contratto di famiglia

- **Famiglia**: `firmware-esp32-platformio`, copia locale [`CLAUDE-firmware-esp32-platformio.md`](CLAUDE-firmware-esp32-platformio.md).
- **Data ultima sincronizzazione**: 2026-09-23 (aggancio §9.1 alla nascita della famiglia).

## 3. Board e toolchain

- Due ambienti: `m5stick-c-plus` (default, ST7789v2) e `m5stick-c` (legacy, ST7735S); codice di rendering
  indipendente dalla risoluzione.
- Con il framework recente (arduino-esp32 3.x via pioarduino) la board `m5stick-c` punta a una variante rinominata:
  `board_build.variant = m5stack_stickc_plus` è la correzione documentata del 2026-05-29 (P-01, caso reale).
- Librerie `M5StickCPlus ^0.1.0` / `M5StickC ^0.2.5`, `PubSubClient ^2.8`: range con caret, versione testata da
  annotare in `platformio.ini` alla prossima build (P-01).

## 4. Runtime, dati, segreti

- Configurazione WiFi/MQTT tramite **portale di configurazione** (`config_portal`, `config_manager`) salvata in
  NVS: nessuna credenziale nel sorgente. Chiavi AES e MAC dei dispositivi Victron: verificare alla prima sessione
  che stiano solo in NVS/portale e non in header committati (P-03; repo pubblico).
- MQTT: topic e schema nel README; da promuovere a `docs/DATA_CONTRACT.md` al primo cambiamento (P-08).
- Recovery: `esptool erase_flash` + riflash; `backups/` da creare al primo flash `risk:high` (P-10).

## 5. Politica git corrente

Default globale; `main` = stabile, branch di feature per il lavoro (`feature/multi-device-mppt` aperto); commit e
push dell'AI a fine slice, PR verso `main` solo quando la beta è validata sul campo da Marco.

## 6. Eccezioni e do-not

- Eccezioni al contratto: nessuna.
- Do-not: non mergiare la beta in `main` senza validazione con dati reali; non pubblicare nulla che contenga
  MAC o chiavi (repo pubblico).

## 7. Documentazione, tracker, skill

- Cassetti e `docs/tech-debt.md`: da creare al primo evento. Legacy alla data di adozione (2026-09-23): `README.md`.
- Issue tracker: GitHub `AldebaranPrimo`, issue del repo; categoria `personal`.
- Stato fra sessioni: `HANDOFF.md` da creare con Marco alla prima chiusura di sessione. Kit `.claude/` non installato.
