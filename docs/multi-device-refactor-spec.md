# Multi-Device Refactor — Specification & Implementation Brief

> Authoritative brief for the `feature/multi-device-mppt` work. This is a **public** repo:
> all deliverables (code, docs, README) must be clear enough for a stranger to replicate
> and reuse. English throughout. No `git push`, no device flashing as part of this work —
> those remain manual owner decisions performed later.

## Goal

Turn the gateway firmware from a **one-device-per-type** model into a **generic N-device**
model, so a single M5StickC can monitor several Victron BLE devices of the *same* type —
specifically **multiple SmartSolar MPPTs** — each published to its own MQTT topic.

Driving use case: a solar array is being split across multiple MPPT charge controllers
(one main controller plus dedicated controllers for differently-oriented sub-arrays). Each
controller must be monitored independently.

## Current architecture (as-is, `master`)

- `config_manager.h`: `#define MAX_DEVICES 3`; `DeviceConfig{enabled,name[16],mac[13],aesKey[33],type}`
  where `type` is `0=Solar / 1=Shunt / 2=BatterySense`.
- `victron_ble.{h,cpp}`: parses BLE advertisements into **three global singletons**
  `solarData`, `shuntData`, `batterySenseData` (one per type).
- `mqtt_publisher.cpp`: publishes **by type** to fixed sub-topics
  `mppt/state`, `smartshunt/state`, `battery_sense/state` under base topic `victron`.
- `display_ui.{h,cpp}`: 3-row power view (PV / Battery / Load), targeted at plain M5StickC.
- `config_html.h` + `config_portal.cpp`: captive-portal web form, up to 3 devices, each
  with name/type/MAC/key. Topic is **not** derived from the name today.
- `main.cpp`: wires config → BLE init → scan loop → publish loop → display.

**Hard limitation:** two devices configured as the same type collide — both land in the
same singleton struct and publish to the same topic. So a second MPPT is impossible today.

## Target design (to-be)

### Data model
- Raise `MAX_DEVICES` to **6** (covers e.g. `mppt`, `mppt2`, `mppt3`, `smartshunt`,
  `battery_sense`, plus one spare).
- Replace the per-type singletons with an **array of per-device runtime state** indexed by
  configured device slot. Route each decrypted advertisement to its slot **by MAC match**,
  not by type.
- Keep parsing logic per Victron record type (solar charger vs battery monitor), but store
  the parsed result into the matched device slot.

### MQTT topics — `victron/<name>/state`
- The per-device **name** becomes the topic segment: `"<baseTopic>/<name>/state"`.
- **Back-compat is mandatory:** the default first solar device keeps the name `mppt`, so the
  existing consumer keeps receiving `victron/mppt/state` byte-for-byte (same JSON schema).
  Subsequent solar devices use `mppt2`, `mppt3`, … The shunt stays `smartshunt`, the
  battery sense stays `battery_sense` — unless renamed by the user.
- Name constraints: lowercase, `[a-z0-9_]`, used verbatim as a topic segment (validate in
  the web form and/or sanitize). Keep the existing per-type JSON payload schemas unchanged
  (see `docs/mqtt-topics.md`) so downstream Home Assistant / Node-RED mappings are stable.
- `gateway/status` topic stays as-is.

### Web config portal
- Extend to **N device rows** (up to `MAX_DEVICES`), each with: enabled, **name** (new role:
  topic segment), type, MAC, encryption key.
- Pre-fill on re-entry as today. Persist via the existing NVS/Preferences mechanism — bump
  the stored config version/struct carefully so old saved configs are migrated or cleared
  cleanly (no garbage reads).

### Display (M5StickC **Plus** — 135×240, ST7789v2)
- **Board changes to M5StickC Plus** (the unit physically connected). Add/adjust the
  PlatformIO env and display library accordingly; keep the original M5StickC buildable if
  feasible, but the Plus is the primary target.
- Redesign the UI for the larger 135×240 panel and for **multiple devices**:
  - The view is informational only, but must be **well designed and usable**: clear
    hierarchy, legible at a glance, no clipping.
  - Multi-device aware: e.g. a **paged** layout (one device per page, Button A cycles) or a
    compact summary that lists each MPPT's PV power + the shunt SoC. Designer's call, but it
    must scale gracefully from 1 to 6 devices and clearly label which device is shown.
  - Preserve the connection-status indicator and orientation toggle.

## Constraints & non-goals

- **Public repo:** code comments, README, and a replication/usage guide must let a newcomer
  build, flash, and configure from scratch. Document the topic scheme, the back-compat
  guarantee, the web-portal fields, and the M5StickC Plus board target.
- **No `git push`** (no remote is configured here anyway) and **no flashing** as part of
  this task. Produce code on `feature/multi-device-mppt` + docs + a review report only.
- Keep memory footprint sane on the M5StickC Plus; mind MQTT buffer size
  (`mqtt.setBufferSize`) and per-device publish cadence.
- Don't break the existing JSON payload schemas.

## Files in scope

`config_manager.{h,cpp}`, `victron_ble.{h,cpp}`, `mqtt_publisher.{h,cpp}`,
`config_html.h`, `config_portal.cpp`, `display_ui.{h,cpp}`, `main.cpp`, `platformio.ini`,
`README.md`, `docs/configuration-guide.md`, `docs/mqtt-topics.md`, `docs/architecture.md`.

## Definition of done

1. Firmware compiles for the M5StickC Plus env (PlatformIO).
2. N MPPTs can be configured, each publishing to `victron/<name>/state`; `mppt` default
   preserves back-compat.
3. Display renders cleanly on 135×240 with 1..6 devices.
4. README + docs updated for public replication.
5. Adversarial review passed (routing correctness, config migration safety, buffer/memory
   safety, topic back-compat, display API correctness, docs clarity).
