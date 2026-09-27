# Debito tecnico — Victron BLE Gateway (M5StickC)

Registro unico delle voci di debito (`TD-NNN`). Le voci chiuse restano in fondo, sotto «Voci chiuse». Nato il
2026-09-27 con la propagazione del contratto `firmware-esp32-platformio` rev 4: sono adeguamenti in corso
(aderenza graduale), da fare in slice chieste da Marco, mai durante un sync. Le chiavi Victron nella storia
pubblica non sono debito: sono un override deciso da Marco (vedi `CLAUDE.md`, §4).

## Voci aperte

### TD-001 — Versioni delle librerie da fissare (P-01)
- Aperta: 2026-09-27 · Origine: sync del contratto.
- Cosa: `M5StickCPlus`, `M5StickC`, `PubSubClient` con range `^` e senza la versione provata.
- Chiusura: versioni esatte con la data della prova, in una slice di toolchain dedicata.

### TD-002 — `CORE_DEBUG_LEVEL` non impostato (P-05)
- Aperta: 2026-09-27 · Origine: sync del contratto.
- Cosa: nessun livello di log dichiarato in `platformio.ini`.
- Chiusura: impostarlo con il motivo, alla prossima slice che tocca la configurazione di build.

### TD-003 — JSON MQTT con ArduinoJson (P-08)
- Aperta: 2026-09-27 · Origine: sync del contratto.
- Cosa: i messaggi MQTT sono composti a mano.
- Chiusura: ArduinoJson, in una slice a sé perché aggiunge una dipendenza.

### TD-004 — Cartella `backups/` (P-10)
- Aperta: 2026-09-27 · Origine: sync del contratto.
- Cosa: nessun binario noto buono conservato.
- Chiusura: crearla al primo flash `risk:high`.

## Voci chiuse

(nessuna)
