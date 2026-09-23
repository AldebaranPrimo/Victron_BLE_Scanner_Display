# AI Execution Contract — ESP32 firmware with PlatformIO

> **Data ultimo aggiornamento**: 2026-09-23 (rev 1 — family created (workflow §9.2) from the five `Tomita\` firmware repos: `TomitaHome_eInk_Display` (pilot), `Victron_BLE_Scanner_eInk_Display`, `Tomita_Camper_LCD_Touch_bringup`, `TomitaHome_LCD_Display`, `Victron_BLE_Gateway`. Every invariant below comes from a bug or a lesson recorded in those repos' `CLAUDE.md`, `platformio.ini` or runbooks. History: `_master-contracts/STATO-CONTRATTI.md` §7, `CHANGELOG.md`.)
> **Data ultima sincronizzazione**: 2026-09-23
>
> Consumers: the five repos above. Same skeleton as the other families; conduct rules live in the user-global file and are not repeated here.

Runtime execution policy for **ESP32 / ESP32-S3 firmware built with PlatformIO** (Arduino framework, sometimes the `pioarduino` fork for Arduino-ESP32 core 3.x), FreeRTOS tasks, displays (e-Paper via GxEPD2, RGB/QSPI TFT via Arduino_GFX + LVGL 8), BLE (NimBLE), WiFi + MQTT (PubSubClient, ArduinoJson), PMIC power management (AXP2101), NVS persistence. The devices are **in production in Marco's house and camper**: a bad flash is a dark display on the wall or a gateway that stops feeding Home Assistant. The AI writes and builds; **Marco flashes and observes** the hardware.

This file is loaded after the user-global rules and before the per-repo `CLAUDE.md` (board, exact pins, task table, MQTT topics, serial port, remote, git policy, exceptions). It holds only what a current-generation model would get wrong on this stack without being told.

---

## Applicability by project scale

Scale is always `solo`, and most repos are also `maintenance`: a device that works is not touched for style. Relaxable: nothing about builds or secrets. Strict at every scale: all invariants P/T; `pio run` green for **every** environment of `platformio.ini`; no secret in git; no dependency or platform version change outside a declared toolchain slice; the file scope rule.

---

## Slices, file scope, frozen code

A slice is one testable change on the device: a new field on the display, a new MQTT field parsed, a new FreeRTOS task, a fix in a parser, a power-management adjustment. Out of scope unless the task declares it: bumping platform, core or library versions (a **toolchain slice**, on its own); changing memory or partition configuration; touching power-rail code; refactoring a working driver. Code copied verbatim from a sibling repo (the Victron BLE parser lives in three repos) stays verbatim: a fix goes to every copy in the same slice, or the divergence is recorded in `docs/tech-debt.md`.

Debt outside scope is recorded, not fixed: `// TODO(<tag>): <reason>` with tags `refactor`, `perf`, `hw` (needs hardware on the desk), `td-NNN`; cross-file debt has an entry in `docs/tech-debt.md`.

**Cost of a change** on a device is regression risk plus **recovery cost**: a firmware that does not boot needs the BOOT+RST recovery and a re-flash of the last good binary. Anything that can prevent boot (memory config, PSRAM flags, partitions, early init of PMIC or display) is high cost regardless of line count.

---

## Execution workflow

**Intake.** Context, Goal, Files, Constraints, Acceptance; ask for what is missing. Risk class, never downgraded:

- `risk:low` — layout, labels, fonts, log text, a new value rendered from data already parsed.
- `risk:medium` — new MQTT field or topic parsed or published, new FreeRTOS task with its own stack, new library within an already pinned major, NVS key added.
- `risk:high` — platform / core / library version change, `build_flags` affecting USB, logging or PSRAM, display driver or bus timing change, BLE stack change, partition table, OTA.
- `risk:critical` — power rails and battery-protection paths (PMIC configuration, thresholds that switch loads), anything that can leave the device unable to boot or to be reflashed.

**Branch.** Global default and per-repo git policy: most of these repos are single-branch (`main`/`master`) solo projects; when the per-repo says so, work happens on the main branch with one commit per slice. A documentation-only change never opens a branch.

**Implementation.** Pin what you add (P-01). Read the board's pin header and `platformio.ini` before touching any GPIO, bus or memory flag; the per-repo and the runbooks are the source, not memory of "the usual ESP32-S3 pins". A driver behaviour discovered on the desk (BUSY polarity, touch rotation, bounce buffer) is written down in the code comment **and** in the per-repo the same day.

**Completion gate**, before commit. `pio run` green for every environment (the AI runs it; build-only is safe). Report the RAM/Flash usage lines printed by the build and the delta against the previous build when they move more than a few percent. Then the **hardware step is Marco's**: `pio run -t upload` on the declared port and the serial monitor for at least one full refresh/publish cycle; the AI says so and waits for the outcome, never declares a firmware working from a green build. Before a `risk:high` or `risk:critical` flash, the current known-good binary is saved (P-10). Commit and push follow the global default and the per-repo policy.

---

## Forbidden without explicit authorization

- Committing secrets: WiFi credentials, MQTT credentials, Home Assistant tokens, Victron device MAC addresses and AES keys, Node-RED flows exported with tokens. They live in `src/secrets.h` (gitignored) with a committed `secrets.h.example` (P-03).
- Changing `platform`, `framework`, a `lib_deps` version or a `board_build.*` memory flag outside a declared toolchain slice.
- Hand-editing files under `.pio/libdeps/`: a library patch is applied by an `extra_scripts = pre:` script committed in the repo (P-04).
- Disabling the watchdog, raising or silencing `CORE_DEBUG_LEVEL` to make a crash disappear instead of finding the cause.
- Touching PMIC rail configuration (on AXP2101 boards `DC1` is the 3.3 V system rail: never disabled) or battery-protection thresholds outside a `risk:critical` task.
- Deleting or gitignoring `backups/*.bin` (recovery firmware) or the recovery runbook.
- Flashing a device: the AI never runs `upload`, `erase_flash` or `esptool` write commands; it prepares and waits.

---

## Testing policy

Firmware here has no unit tests by default; the test is the device on the desk. Pure logic (protocol parsers, bit-packing, AES record decoding, JSON mapping) is the exception: a `[env:native]` with Unity tests is **recommended** when such code is touched, and mandatory before a parser change ships to a device that is hard to reach. Every repo keeps a **hardware smoke checklist** in its README (boot, WiFi, MQTT connect, first render, first publish, one full cycle) that Marco runs after a flash; a `risk:high` change adds the specific check it needs.

---

## Stack invariants

Toolchain and build

- **P-01** Every `lib_deps` entry is pinned exactly (`@ =x.y.z`) or, when a caret range is deliberate, the tested version is written in a comment; the `platform` is pinned (`@ ^6.x` or the exact `pioarduino` release URL). A bump is its own toolchain slice, with the reason and the test outcome recorded as a comment in `platformio.ini`. Real breakages behind this rule: NimBLE 2.1.1 vs ESP-IDF 5.5.4 (ABI crash in `btdm_controller_deinit`), Arduino_GFX 1.6+ requiring Arduino-ESP32 core 3.x, LVGL 9 PSRAM problems on ESP32-S3 RGB panels, board variant renamed in core 3.x (`m5stick_c` → `m5stack_stickc_plus`).
- **P-02** `platformio.ini` is the single source of truth for memory configuration (`flash_mode`, `flash_size`, `psram_type`, `arduino.memory_type`, `upload.maximum_size`), verified with `esptool flash_id` on the real device and dated in a comment. `qio_opi` for ESP32-S3 with OPI PSRAM; getting this wrong is a device that boots into a reset loop.
- **P-03** Secrets in `src/secrets.h`, gitignored, with `src/secrets.h.example` committed and complete; `config.h` holds pins and constants only. A repo whose history or working tree still carries keys in a config header declares it in the per-repo as debt and is **not pushed to any shared remote** until migrated.
- **P-04** Library patches (e.g. GxEPD2 BUSY polarity `HIGH` → `LOW` for the GDEM0397T81 panel) are applied by a committed `scripts/*.py` registered as `extra_scripts = pre:` in `platformio.ini`, so they survive every library reinstall. Never patched by hand in `.pio/`.
- **P-05** USB and logging flags match the physical USB path: native USB (`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`) versus external UART bridge (CH340: `CDC_ON_BOOT=0`). `CORE_DEBUG_LEVEL` is chosen per board and written down: log spam from a driver can trip the task watchdog (seen with GT911 init logging `Invalid IO 255` at level ERROR).

Runtime architecture

- **P-06** The FreeRTOS task table (task, core, stack size, role) lives in the per-repo and matches the code. BLE scanning runs on core 0, display and WiFi/MQTT on core 1; shared state is behind a mutex; a shared I2C bus (PMIC, sensors, RTC, touch) has its own mutex. Stack sizes are stated in the table and changed only with the reason.
- **P-07** RGB DPI and QSPI panels with the framebuffer in PSRAM need a **bounce buffer** (`bounce_buffer_size_px` > 0 in Arduino_GFX's `Arduino_ESP32RGBPanel`): without it the DMA reads PSRAM while LVGL writes it and the image vibrates or shifts. The value used and why is in the pin header and the per-repo.
- **P-08** MQTT is a documented contract: every topic the device publishes or subscribes to, its JSON schema, retained or not, and the device's own status topic (separate per device, never shared with a sibling) are in `docs/DATA_CONTRACT.md` or in the README. JSON goes through ArduinoJson with the document size stated. A breaking change to a topic is an ADR and a coordinated change in the consumer (Node-RED flow, Home Assistant) in the same slice.
- **P-09** Power management is explicit: which PMIC rail feeds what (e.g. `EPD_VCC` on `ALDO3`), which rails are never disabled, display power-off (`display.powerOff()`, controller deep sleep) versus rail-off (needs a full re-init). Battery-protection paths are `risk:critical` and changed only with Marco watching the device.
- **P-10** Recovery is part of the deliverable: the per-repo (or `docs/runbook/`) has the recovery procedure for the board (BOOT + RST timing, `esptool erase_flash`, port), and `backups/` keeps the last known-good `.bin` with the commit hash it was built from, refreshed before every `risk:high` or `risk:critical` flash. `backups/*.bin` is excluded from the binary gitignore rules.
- **P-11** NVS persistence through `Preferences`: namespace and keys listed in the per-repo; a change of record layout ships with a version key and a migration or an explicit reset, never a silent reinterpretation of old bytes.
- **P-12** Time: NTP sync is optional on mobile devices (camper); every time-dependent behaviour has a declared fallback when the clock is not synced (e.g. brightness falls back to "day").

Transversal

- **T-01** Every session starts with the deterministic briefing of the SessionStart hook from the kit when installed; otherwise the per-repo lists the manual checks and `HANDOFF.md` is read first.
- **T-02** Session state between sessions lives in `HANDOFF.md` at repo root (global skills `recupera-memoria` / `salva-memoria`); Claude memory under `~/.claude/projects/` is a mirror, not the source.
- **T-03** Every repo declares its remote in the per-repo. A repo without a remote is a single-disk risk and says so in its first lines, together with the reason (typically P-03 not yet satisfied).

---

## Documentation layout

The layout is the cross-family one: three drawers `docs/decisions|requests|incidents/` (one event = one file `YYYY-MM-DD-<slug>.md`, Italian slug, YAML frontmatter `id / stato / data-apertura / schema-version`, sealed when closed; canonical templates are the ones emitted by the skills `decision-new`, `request-new`, `incident-new`), `docs/tech-debt.md` as the single living debt registry (entries `TD-NNN`, deferred decisions `TD-D-NNN` with a mandatory reopening trigger, closed entries kept under "Voci chiuse", inline `TODO(td-nnn)` markers matching entries), living docs at the root of `docs/` (`DATA_CONTRACT.md`, `pin_mapping.md`, `runbook/*.md`, mockups, edited in place), `README.md` + `CHANGELOG.md` (Keep a Changelog) at repo root.

When a file is born: a decision in the turn it is taken (a display layout chosen among mockups is a decision); a request when it does not close in the same turn; an incident for every anomaly with impact on a device in production, even if hot-fixed; a debt entry for every conscious deferral. A substantial plan (≥ 2 of: more than a week of work, ≥ 3 formal exchanges, ≥ 2 extra technical artifacts, ≥ 2 sub-projects) promotes the ADR or request to a folder with `README.md` and `<slug>-piano.md`, approved by the user before implementation.

Issue tracker, when a platform CLI is authenticated (`az repos` on Azure DevOps, `gh` on GitHub): one drawer file = one work item or issue, mandatory for requests and incidents, optional for tech debt and decisions; cross-links both ways. Without a CLI, file-only.

Legacy documentation present at the adoption date declared in the per-repo stays where it is (`ROADMAP.md`, `PROJECT_STATUS.md`, `docs/mockups/`); conversion is optional; the per-repo lists legacy files. Anti-patterns: empty skeleton folders, hand-maintained index READMEs (the one exception is `docs/reviews/README.md`, below), more than two levels under `docs/`, nested drawers, deleting closed debt entries.

Fourth drawer `docs/reviews/`: review threads by an external AI reviewer (Codex) on a plan or a diff, one file per review with the whole thread inside (Rilievi → Risposta → Replica → Esito), scaffolded by `/review-new`; each section is filled in place over its placeholder, never duplicated, nothing below Esito; an extra round adds `Rilievi (giro N)` + `Risposta (giro N)` before Esito. The reviewer writes only its Rilievi and Replica there, never code, never other drawers, never `stato`, never commits; Claude answers, moves `stato` to `risposta` and applies accepted fixes within the slice's file scope; only the user closes. The SessionStart briefing lists reviews still open: read and answer them before other work. The drawer's index is `docs/reviews/README.md` (one row per review: date, id, reviewer, object, state, one-line outcome), kept by Claude by hand at every state change, never by a hook; the reviewer does not touch it.

---

## Code documentation

Global rule *Codice — commentare sempre* applies. Stack form: every `.h`/`.cpp` opens with a header stating the module's role, the task it runs on and the hardware it touches; every pin, timing, bus speed or magic number carries the reason and, when it came from the desk, the date of the measurement; `platformio.ini` comments record why each pin and flag is what it is. Comments in Italian, code identifiers in English (convention of these repos).

---

## Per-repo `CLAUDE.md` — required sections

Intro (what the device does and where it is installed) · board and hardware (MCU, display, PMIC, sensors, pin header file) · repo and remote (or "none" with the reason, T-03) · toolchain (platform, core, exact library versions, serial port) · FreeRTOS task table · MQTT topics and data contract pointer · secrets handling state (P-03 satisfied or debt) · recovery procedure pointer and `backups/` state · sibling repos and shared code · **current git policy** · exceptions to P/T invariants with rationale (or "none") · do-not list of board footguns · documentation drawers with population state · issue tracker · documentation layout adoption date and legacy list · skills available.

---

## Glossary

Environment: an `[env:<name>]` section of `platformio.ini`, one buildable target. Toolchain slice: a change of platform, core or library versions, on its own. Bounce buffer: DRAM staging buffer between the PSRAM framebuffer and the display DMA. Known-good binary: the last `.bin` seen working on the device, kept in `backups/`. Data contract: the documented set of MQTT topics and JSON schemas a device exchanges with Node-RED / Home Assistant. Sibling repo: another repo of this family sharing hardware or verbatim code with this one.

---

## Required tooling

PlatformIO Core (`pio`) with the VS Code extension; the `pioarduino` platform where the repo pins it; `esptool` (ships with the platform) for `flash_id` and recovery; USB drivers for the board's bridge (CH340 where present); a serial terminal (`pio device monitor`). Node-RED and Home Assistant access is out of scope for the firmware repo: the consumer side is coordinated through the data contract. Base tooling and the platform CLI rule are in `CLAUDE-meta.md`.

---

## Revision history

Kept in `_master-contracts/STATO-CONTRATTI.md` §7 and `CHANGELOG.md`. Rev 1 (2026-09-23): family created from the five `Tomita\` firmware repos; pilot `TomitaHome_eInk_Display`.
