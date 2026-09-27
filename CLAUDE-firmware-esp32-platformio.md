# AI Execution Contract — ESP32 firmware with PlatformIO

> **Data ultimo aggiornamento**: 2026-09-27 (rev 4 — trimmed: drawer templates and rules left to the skills that emit them, per-repo required sections moved to the `master-sync` skill, revision history left to the registry; drawer files through the skill `nuovo-file-cassetto`, completion gate and two-axis self-review through `chiusura-slice`. Gradual adherence for existing repos; documentation map allowed in `docs/README.md`. History: `_master-contracts/STATO-CONTRATTI.md` §7, `CHANGELOG.md`.)
> **Data ultima sincronizzazione**: 2026-09-27 (sync2 e mappa della documentazione; snapshot `storico/2026-09-27-sync2/`, `storico/2026-09-27-direct-mappa-documentazione/`).
>
> Family created 2026-09-23 (workflow §9.2) from the five `Tomita\` firmware repos, which are its consumers: `TomitaHome_eInk_Display` (pilot), `Victron_BLE_Scanner_eInk_Display`, `Tomita_Camper_LCD_Touch_bringup`, `TomitaHome_LCD_Display`, `Victron_BLE_Gateway`. Every invariant below comes from a bug or a lesson recorded in those repos' `CLAUDE.md`, `platformio.ini` or runbooks. Conduct rules live in the user-global file and are not repeated here.

Runtime execution policy for **ESP32 / ESP32-S3 firmware built with PlatformIO** (Arduino framework, sometimes the `pioarduino` fork for Arduino-ESP32 core 3.x), FreeRTOS tasks, displays (e-Paper via GxEPD2, RGB/QSPI TFT via Arduino_GFX + LVGL 8), BLE (NimBLE), WiFi + MQTT (PubSubClient, ArduinoJson), PMIC power management (AXP2101), NVS persistence. The devices are **in production in Marco's house and camper**: a bad flash is a dark display on the wall or a gateway that stops feeding Home Assistant. The AI writes and builds; **Marco flashes and observes** the hardware.

This file is loaded after the user-global rules and before the per-repo `CLAUDE.md` (board, exact pins, task table, MQTT topics, serial port, remote, git policy, exceptions). It holds only what a current-generation model would get wrong on this stack without being told.

An existing repo adapts to this contract on its own schedule: deviations recorded in the per-repo `CLAUDE.md` (an override, or an *adeguamento in corso* with a `docs/tech-debt.md` entry) are known and decided — follow the per-repo, never raise them again, report only a new deviation, once. Firmware code is never changed to follow the contract outside a slice Marco asks for.

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

**Completion gate**, before commit, through the skill `chiusura-slice` (gate, then self-review on the Standard and Specifica axes). `pio run` green for every environment (the AI runs it; build-only is safe). Report the RAM/Flash usage lines printed by the build and the delta against the previous build when they move more than a few percent. Then the **hardware step is Marco's**: `pio run -t upload` on the declared port and the serial monitor for at least one full refresh/publish cycle; the AI says so and waits for the outcome, never declares a firmware working from a green build. Before a `risk:high` or `risk:critical` flash, the current known-good binary is saved (P-10). Commit and push follow the global default and the per-repo policy.

---

## Forbidden without explicit authorization

- Committing a secret the per-repo classifies as **protected** (P-03): credentials that open Marco's network or a client's system, tokens that let someone act rather than read. A secret the per-repo classifies as **plain** (the AES key of Marco's own device, a read-only key to a public feed) may sit in the repo like any other string.
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
- **P-03** Secrets are **weighed by the project, not by the contract** (Marco, 2026-09-26). Not every secret is capital: the AES key of Marco's own Victron device or the WiFi password of the camper, if leaked, expose at most the readings of his camper; a credential that opens his home network, or anything belonging to a client, is a different matter. The contract's only duty is to **flag**: when the AI finds a secret in the tree or in the history it says so once, with what the secret opens. The per-repo `CLAUDE.md` then classifies each secret as **plain** (stays in the repo like any other string, no `secrets.h`, no gitignore, no debt) or **protected** (kept out of the tree: `src/secrets.h` gitignored with a committed `secrets.h.example`; if it is already in the history, rotate first, then rewrite the history, then force-push, every step done by Marco). **Marco's classification is final**: once he has said a secret is plain, it is plain, and the AI does not raise it again. A repo with no classification yet lists its secrets in the per-repo as "to classify" and nothing more.
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
- **T-03** Every repo declares its remote in the per-repo. A repo without a remote is a single-disk risk and says so in its first lines, together with the reason. These repos are private and seen by Marco and Claudio only: a plain secret (P-03) is never a reason to withhold a remote.

---

## Documentation layout

Drawers `docs/decisions|requests|incidents|reviews/`, one event = one file, created only through the skill `nuovo-file-cassetto` (it holds templates, frontmatter and drawer rules); `docs/tech-debt.md` as the single debt registry (`TD-NNN`, deferred decisions `TD-D-NNN` with a reopening trigger, closed entries kept, inline `TODO(td-nnn)` matching an entry); living docs at the root of `docs/` (`DATA_CONTRACT.md`, `pin_mapping.md`, `runbook/*.md`, mockups) edited in place; `README.md` + `CHANGELOG.md` at repo root; `HANDOFF.md` for session state (global skills `recupera-memoria` / `salva-memoria`).

When a file is born: a decision in the turn it is taken (a display layout chosen among mockups is a decision); a request when it does not close in the same turn; an incident for every anomaly with impact on a device in production, even if hot-fixed; a debt entry for every conscious deferral. A substantial plan (≥ 2 of: estimate above 8 hours of active Claude time, per the global skill `stima-tempi-sviluppo` (Marco sets the final figure), ≥ 3 formal exchanges, ≥ 2 extra technical artifacts, ≥ 2 sub-projects) promotes the ADR or request to a folder with `README.md` and `<slug>-piano.md`, approved by the user before implementation.

Issue tracker, when a platform CLI is authenticated (`az repos` on Azure DevOps, `gh` on GitHub): one drawer file = one work item or issue, mandatory for requests and incidents, optional for tech debt and decisions; cross-links both ways. Without a CLI, file-only.

Legacy documentation present at the adoption date stays where it is (`ROADMAP.md`, `PROJECT_STATUS.md`, `docs/mockups/`), listed in the per-repo. No empty skeleton folders, no hand-kept index README except `docs/reviews/README.md` and the documentation map `docs/README.md` (projects with user-facing documentation: table *Mappa della documentazione* with document, audience, update trigger, notes and sealed files, read by `chiusura-slice`), no nested drawers, no more than two levels under `docs/`.

Reviews by an external AI (Codex, thread rules in the skill `nuovo-file-cassetto`): the SessionStart briefing lists the open ones, read and answer them before other work; Claude fills Risposta in place, moves `stato` to `risposta`, fixes only inside the slice's file scope and updates `docs/reviews/README.md`; only the user closes.

---

## Code documentation

Global rule *Codice — commentare sempre* applies. Stack form: every `.h`/`.cpp` opens with a header stating the module's role, the task it runs on and the hardware it touches; every pin, timing, bus speed or magic number carries the reason and, when it came from the desk, the date of the measurement; `platformio.ini` comments record why each pin and flag is what it is. Comments in Italian, code identifiers in English (convention of these repos).

---

## Glossary

Environment: an `[env:<name>]` section of `platformio.ini`, one buildable target. Toolchain slice: a change of platform, core or library versions, on its own. Bounce buffer: DRAM staging buffer between the PSRAM framebuffer and the display DMA. Known-good binary: the last `.bin` seen working on the device, kept in `backups/`. Data contract: the documented set of MQTT topics and JSON schemas a device exchanges with Node-RED / Home Assistant. Sibling repo: another repo of this family sharing hardware or verbatim code with this one.

---

## Required tooling

PlatformIO Core (`pio`) with the VS Code extension; the `pioarduino` platform where the repo pins it; `esptool` (ships with the platform) for `flash_id` and recovery; USB drivers for the board's bridge (CH340 where present); a serial terminal (`pio device monitor`). Node-RED and Home Assistant access is out of scope for the firmware repo: the consumer side is coordinated through the data contract. Base tooling and the platform CLI rule are in `CLAUDE-meta.md`.