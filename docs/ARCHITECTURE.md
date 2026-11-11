# Ditonira — Architecture

Living document of technical decisions. All implementation must follow this text.

## Language convention

- **All project documentation is English:** `README.md`, everything under `docs/`, Cursor rules (`.cursor/rules/`), engineering notes, and any other docs agents create or update.
- Code comments and identifiers (types, functions, variables): **English**.
- **All end-user UI strings are English** (labels, buttons, status messages, placeholders, errors, window title). No Spanish (or other) UI copy in the MVP; i18n/`tr()` may remain for future localization, but the source strings must be English.
- Agents must not write or leave new documentation or UI strings in Spanish (or other languages); translate or rewrite into English.

## 1. Goal

Linux GUI application for amateur CW:

1. **Transmit** Morse text by keying the **RTS** or **DTR** line of a USB–serial converter (shipped MVP).
2. **Receive / decode** CW audio with the DeepCW neural pipeline (ONNX Conformer-CTC) — see [`ARCHITECTURE_RX_DEEPCW.md`](ARCHITECTURE_RX_DEEPCW.md) (DEC-005, phased).

### MVP scope (shipped — TX)

- Serial port selection.
- Keyer line selection: RTS or DTR.
- Invertible polarity (key-down = asserted or deasserted).
- Speed in WPM.
- Text box with the message to transmit.
- **Transmit** and **Stop (Esc)** actions; **Esc** aborts TX the same as Stop (§7.4) — implemented (`QShortcut` + idle no-op).
- English UI throughout.
- Live TX progress: already-transmitted text shown in a distinct color (§7.3).

### RX scope (approved — DeepCW port)

- Full algorithmic port of DeepCW Normal-mode decode (9600 Hz → STFT → 65-bin spectrogram → ONNX → CTC).
- Phased: offline WAV harness → live UI → streaming transcript → optional pileup.
- **Normative detail:** [`ARCHITECTURE_RX_DEEPCW.md`](ARCHITECTURE_RX_DEEPCW.md). Do not invent alternate DSP/ML paths.

### Out of scope (current)

- Local sidetone.
- Manual key / paddle / iambic.
- DeepCW Web Serial keyer, RTTY, JA model (absent), neural NR (not in DeepCW web code).
- Macros, memory banks.
- Windows / macOS (Linux x86_64 AppImage only).

## 2. Stack

| Piece | Decision | Rationale |
|---|---|---|
| Language | C++17 | Required; enough for `chrono`, `atomic`, etc. |
| GUI | Qt 5 Widgets | Better fit for older distros; mature AppImage tooling |
| Serial | QtSerialPort (`QSerialPort`) | Stable RTS/DTR API without data I/O |
| RX inference | ONNX Runtime C++ (CPU EP) | Same models as DeepCW; no WASM |
| RX audio | PortAudio or Qt5 audio + resample to 9600 Hz | Match DeepCW capture rate |
| Build | CMake ≥ 3.16 | Standard `find_package(Qt5 …)` + ORT |
| Distribution | **Self-contained** AppImage x86_64 | End user installs **no** app packages (no Qt/ORT/ICU apt); see **§8 (hard)** |

If `find_package(Qt5SerialPort)` fails but the runtime `.so` is present, CMake may use headers extracted under `.deps/qt5serialport-dev` (local dev only; the AppImage still bundles the library). Prefer installing `libqt5serialport5-dev` on the build host.

**Qt 6 is not used:** higher glibc/ABI risk on older hosts.

**Product non‑negotiable (from day one):** Ditonira is distributed so a user on **another Linux distro/version** can run the AppImage **without installing matching library versions**. Building on a bleeding-edge host and shipping that binary is a **defect**, not “good enough.”

### 2.1 Offline-only + lean surface (HARD — Architect-defined)

Ditonira is an **offline** amateur-radio tool (local serial TX + local audio RX + local ONNX model). As a **product**, it must **not** use the **internet** or talk to remote hosts (no cloud, no downloads, no telemetry).

**Careful distinction — local IPC is not “going online”:**

Linux desktops often need **local** sockets / buses for normal offline operation. Those are **allowed** when required by Qt, audio, or the display stack. Do **not** strip or ban them blindly.

| Allowed local mechanisms (examples) | Why they may appear |
|---|---|
| Unix domain sockets (`AF_UNIX`) | PulseAudio / PipeWire client, other local audio |
| D-Bus (session/system, local) | Qt platform / desktop integration on some hosts |
| Abstract or filesystem socket paths under `/run`, `/tmp`, `$XDG_RUNTIME_DIR` | Same — local only |
| Local loopback used *only* by a host service the OS already runs | Still not our app “calling the internet” |

**FORBIDDEN (remote / internet product behavior):**

| Forbidden | Examples |
|---|---|
| Remote network clients/servers | HTTP/HTTPS to the internet, WebSocket to remote hosts, FTP, intentional DNS for app features, “phone home” |
| Telemetry / analytics / crash-upload | Sentry, Google Analytics, custom beacons, usage ping |
| Online downloads at runtime | Fonts, models, updates, dictionaries, icons, “fetch assets on first run” |
| Account / cloud / license servers | Login, activation over the network |
| Bundling unused *remote* stacks “just in case” | Qt Network + TLS/curl/CUPS pulled in without need — strip when not `DT_NEEDED`; do **not** confuse this with Pulse/D-Bus local sockets |

**ALLOWED product I/O:** USB-serial, local sound device (incl. via local audio daemon sockets), local filesystem read of **shipped** AppImage resources, X11/Wayland display.

**Default for Implementer:** do not add remote/internet features or fat unused modules. Prefer the **smallest** AppImage that still runs. **Do not** remove local socket support that the host audio/GUI stack needs. When unsure → HANDOFF **For Architect**.

Ship CW model **inside** the AppImage (`resources/models/`). Never download it.

## 3. Morse timing (PARIS)

Speed standard: the reference word **PARIS** equals 50 time units.

\[
T_{\mathrm{ms}} = \frac{1200}{\mathrm{WPM}}
\]

| Element | Units |
|---|---|
| Dit (mark) | 1 |
| Dah (mark) | 3 |
| Intra-character element gap (space) | 1 |
| Inter-character gap | 3 |
| Word gap | 7 |

Recommended UI range: **5–50 WPM**.

## 4. Character encoding

- Normalize to uppercase before encoding.
- ITU map: A–Z, 0–9, and basic punctuation: `=`, `/`, `?`, `.`, `,`.
- Space → word gap (7 units).
- Unknown characters: **omitted** (no extra gap). Documented so future changes stay intentional.

Implementation: `MorseEncoder` (static table + mark/space sequence).

## 5. Hardware keying

- Open the port with `QSerialPort` at **9600 8N1** (baud rate irrelevant for RTS/DTR; conventional value).
- No application flow control that interferes; do not rely on XON/XOFF.
- Active line from user selection:
  - RTS → `setRequestToSend(bool)`
  - DTR → `setDataTerminalReady(bool)`
- The **unused** line stays idle (key-up).
- **Polarity** (default: asserted = key down):
  - Normal: `keyDown` → `true` in the Qt API; `keyUp` → `false`.
  - Inverted: negate the logical value before writing the line.

On finish or abort, **always** force key-up.

## 6. Concurrency

```
MainWindow (GUI thread)
    │ start/stop + parameters
    ▼
KeyerEngine (QThread)
    │ MorseEncoder → sequence
    ▼
SerialKeyLine → QSerialPort (RTS/DTR)
```

- Timing does **not** use `QTimer` on the GUI thread (high jitter).
- Keyer thread: `std::chrono::steady_clock`, absolute deadlines per element, sleep for the remainder.
- Cooperative stop: atomic flag / `requestStop()`; checked between elements; key-up on exit.
- Qt signals to the UI (queued to the GUI thread):
  - status (idle / transmitting / error / stopped / ready),
  - **required** progress: `progressChanged(int charsCompleted)` — count of **source** characters (UTF-16 `QString` indices / length semantics) from the start of `Job::text` that are fully handled,
  - finished / failed.

`SerialKeyLine` and `QSerialPort` are created and used on the engine thread; the UI only sends commands via agreed thread-safe paths.

## 7. UI (MainWindow)

### 7.1 Controls

1. Port combo + Refresh (`QSerialPortInfo::availablePorts()`).
2. Line combo: RTS | DTR.
3. WPM spin box (5–50).
4. Invert polarity checkbox.
5. Text edit for the message (`QPlainTextEdit` or `QTextEdit`).
6. Transmit / Stop buttons (Stop label includes Esc hint — §7.2 / §7.4).
7. Status label (idle, transmitting, open error, etc.).

While transmitting: disable port/line/polarity/WPM changes and Transmit; make the text edit **read-only**; enable Stop. **Esc** must abort TX the same way as Stop (§7.4).

### 7.2 UI language (English)

Canonical English copy (Implementer must use these or clear English equivalents; no Spanish):

| Role | String |
|---|---|
| Window title | `Ditonira CW Keyer` |
| Port label | `Port:` |
| Refresh | `Refresh` |
| Line label | `Line:` |
| Line items | `DTR`, `RTS` |
| WPM label | `WPM:` |
| Polarity | `Invert polarity` |
| Text label | `Text to transmit:` |
| Placeholder | e.g. `CQ CQ DE CALL K` |
| Transmit | `Transmit` |
| Stop | `Stop (Esc)` — compact self-explanatory label (button + Esc share the same action) |
| Ready | `Ready` |
| No ports | `No serial ports` / `(no ports)` |
| Select port | `Select a port` |
| Empty text | `Enter text to transmit` |
| Opening fail | `Could not open %1: %2` |
| Transmitting | `Transmitting…` |
| Stopping | `Stopping…` |
| Stopped | `Stopped` |
| Nothing to send | `Nothing to transmit` |

Engine-emitted status/error strings shown in the UI must also be English.

### 7.3 Transmission progress coloring

**Goal:** the operator can see how far TX has advanced inside the text box.

**Rules:**

1. **Progress unit:** source character index into the exact `Job::text` snapshot taken at Transmit (same string the engine encodes). `charsCompleted` is in `0 … text.size()` and means characters `[0, charsCompleted)` are done.
2. **When to advance:** after each source character is fully handled by the engine:
   - Mapped character: after its marks **and** its trailing inter-character gap (or end of message).
   - Space: after its word gap.
   - Unknown (omitted): advance immediately when skipped so the highlight stays aligned with visible text.
3. **Visual:** apply a distinct **sent** style to `[0, charsCompleted)` and leave `[charsCompleted, end)` in the normal **pending** style. Do **not** rewrite or mutate the text content to show progress (no inserting markers). Prefer `QPlainTextEdit::setExtraSelections` or equivalent char formatting.
4. **Colors:** derive from `QPalette` so light/dark themes stay readable. Recommended: **sent** foreground = `QPalette::Link`; **pending** = default text (`QPalette::Text` / document default). Sent must be clearly distinguishable from pending; do not rely on background-only highlighting that hides the caret range.
5. **Lifecycle:**
   - On Transmit start: reset to `charsCompleted = 0` (all pending).
   - During TX: update on each `progressChanged`.
   - On natural finish: `charsCompleted == text.size()` (all sent).
   - On Stop/abort: leave the partial sent range as-is (shows how far it got).
   - When the user edits the text after TX (or starts a new Transmit): clear or recompute highlight appropriately (edit → clear sent styling until the next TX).
6. **Threading:** only the GUI thread paints; the engine only emits indices.

`KeyerEngine` owns progress emission; `MainWindow` owns painting.

### 7.4 Stop via button and Esc

**Goal:** abort TX quickly without hunting for the mouse — same behavior as clicking Stop.

**Rules:**

1. **Same code path:** Esc and the Stop button both call the existing stop handler (`onStop` / `requestStop()`). No separate abort logic.
2. **When Esc is active:** only while a transmission is in progress (same window of time Stop is enabled). When idle, Esc must **not** close the window or clear the text.
3. **Focus:** Esc must work even if focus is in the text edit or another control (application/window shortcut), not only when Stop has focus.
4. **Implementation (Qt 5):** prefer `QShortcut` / `QAction` with `Qt::Key_Escape` on `MainWindow` (e.g. `Qt::WindowShortcut` or `Qt::ApplicationShortcut` scoped to this window), connected to the Stop slot. Do not rely on the user tabbing to the Stop button.
5. **Label:** Stop button text is **`Stop (Esc)`** so the shortcut is discoverable without a separate legend.
6. **Optional:** `setShortcut(QKeySequence(Qt::Key_Escape))` on the Stop button is acceptable **in addition to** a window-level shortcut if both stay in sync; the window-level path is required so Esc works while the read-only text edit has focus.
7. **Semantics unchanged:** cooperative stop, key-up on abort, partial progress highlight retained (§7.3).

## 8. AppImage packaging (self-contained — HARD REQUIREMENT)

### 8.0 What the **end user** gets (this is the product ask)

The human operator / second PC must only do:

```text
chmod +x Ditonira-x86_64.AppImage
./Ditonira-x86_64.AppImage
```

**No** Docker. **No** compiling. **No** “use an older PC to build”. **No** `apt install` of Qt, ONNX Runtime, ICU, PortAudio, or “library version N”.

That single AppImage is the deliverable. Anything about containers or Ubuntu 20.04 is **only** how the **Implementer** manufactures that file — never instructions pushed to the end user.

### 8.0.0 Product rule (non-negotiable)

The shipped `dist/Ditonira-x86_64.AppImage` must run on a **different** Linux machine/distro **without** asking the user to install matching app-library versions.

An AppImage that fails on another machine with `GLIBC_2.XX not found` / missing `.so` versions is **unacceptable** (P0). This was the original distribution goal.

**Why Implementers mention “old glibc / container” (internal only):**  
On Linux, an AppImage **embeds Qt/ORT/ICU/…** but **cannot embed the host `glibc`**. The binary always needs a host glibc **≥ the glibc of the machine that compiled it**. If we compile on a brand-new distro, older second PCs break. So the **factory** that *produces* the AppImage must use an **old-enough toolchain baseline** (convention: Ubuntu 20.04 / glibc 2.31). That is **not** “make the user compile on a old PC”; it is “Implementer builds the portable file correctly so the user never thinks about this.”

- **End user:** run AppImage. Stop.
- **Implementer factory:** produce AppImage with max needed GLIBC ≤ 2.31 (container/`ubuntu:20.04` or equivalent automated build). Do **not** ask the human to install Docker or fetch an old laptop unless they explicitly choose to help with builds.

### 8.0.1 What must be bundled inside the AppImage

| Component | Rule |
|---|---|
| Qt 5 (Core/Gui/Widgets/SerialPort/XcbQpa + plugins) | **Bundle** (linuxdeploy + qt plugin) |
| ICU and other Qt private deps | **Bundle** (whatever sonames the *build* Qt needs — must travel with the AppImage) |
| `libonnxruntime.so*` | **Bundle** under `$APPDIR/usr/lib` with RPATH `$ORIGIN/../lib` |
| DeepCW `deepcw_en.onnx` | **Bundle** under `usr/share/ditonira/models/` |
| PortAudio | Prefer **static** link into `ditonira` (already intended); do not require host `libportaudio` |
| `libstdc++` / `libgcc_s` | **Bundle** if the build toolchain is newer than Ubuntu 20.04’s; when building *inside* 20.04, host libstdc++ of 20.04-class targets is OK |

### 8.0.2 What may come from the host OS (whitelist)

Only interfaces that are effectively “the machine”:

- Dynamic linker / `libc.so.6` / `libm` / `libdl` / `libpthread` / `librt` (glibc ≥ build baseline).
- **ALSA** `libasound.so.2` (audio device). Optional later: Pulse via host — still no versioned app stack.
- X11/Wayland/GL driver stack as required by Qt platform plugins (linuxdeploy normally ships most userspace pieces; GPU drivers stay on host).

**Forbidden host requirements:** any specific `libicu*.so.NN` from the distro, system Qt5 packages, system ONNX Runtime, “install libfoo7 because we built against foo7”.

### 8.0.3 Release build procedure (**Implementer only** — invisible to end users)

**Audience:** Implementer agent / CI. **Never** present these steps as user documentation or as “what you must do on your old PC.”

1. Automate a **factory** build with glibc baseline ≤ Ubuntu 20.04 (preferred: `packaging/` script that uses Podman/Docker/`ubuntu:20.04` **on the build machine the Implementer controls**, or CI). Build deps stay inside that factory.
2. Produce AppImage with `packaging/build-appimage.sh` **from that factory**.
3. **Gate before Done / before handing the human the file:**
   - Extract AppImage; `objdump -T usr/bin/ditonira | grep GLIBC_` — **no symbol newer than `GLIBC_2.31`**.
   - Qt/ORT/ICU resolve from AppDir (bundled).
   - Smoke start under `QT_QPA_PLATFORM=offscreen`.
4. Hand the human **only** `dist/Ditonira-x86_64.AppImage` (+ “chmod +x && run”). In HANDOFF, record factory image for traceability — not as user steps.

### 8.0.4 Steps (unchanged mechanics)

1. Release build with relative RPATH (`$ORIGIN/../lib`).
2. Bundle with `linuxdeploy` + Qt plugin: binary, Qt libs, `platforms/libqxcb.so`, icon, `.desktop`.
3. Copy ORT + model into AppDir (script already does this; keep it).
4. Artifact: `Ditonira-x86_64.AppImage`.

Device permission: `/dev/ttyUSB*` / dialout — OS account config, not an apt dependency of Ditonira.

### 8.0.5 Lean AppImage allowlist (MUST follow §2.1)

**Rule in one line:** bundle **only** what Ditonira needs to open a window, key a serial line, capture audio, run local ONNX, and paint the waterfall — **nothing else**.

**Explicit NO (strip from AppDir if linuxdeploy pulls them; never link them in CMake):**

- `libQt5Network*`, `bearer` plugins, anything HTTP/FTP
- `libcups*`, `printsupport`, printing plugins
- `libssl*` / `libcrypto*` / `libnss*` / `libgnutls*` / certificate stores pulled for TLS **unless** proven `DT_NEEDED` (target = **not** needed)
- Telemetry SDKs, update checkers, fontconfig “downloadable fonts”, online icon themes
- QML, WebEngine, Multimedia (unless Architect later adds offline-only media — today: **no**), SQL drivers, Designer

**Explicit YES (examples):**

- Qt **Core / Gui / Widgets / SerialPort** + `platforms/libqxcb.so` (and minimal Xcb/GL deps Qt needs to show a window)
- ICU / png / harfbuzz / glib **only if** required by that Qt stack
- `libonnxruntime.so*` + shipped `deepcw_en.onnx`
- Host `libasound` OK for audio device (not an “internet” dep)

**Implementer checklist before Done:**

1. CMake `target_link_libraries` = only modules we use (no Network, no PrintSupport).
2. Post-`linuxdeploy` cleanup deletes forbidden plugin dirs/libs.
3. `readelf -d usr/bin/ditonira` / AppDir inventory: no Network/CUPS/TLS families, or HANDOFF explains a true `DT_NEEDED` exception (Architect must accept).
4. Zero code paths that use `QNetworkAccessManager`, download fonts/models, send telemetry, or open **remote** TCP/UDP/HTTP for product features. **Local** sockets for audio/GUI (Pulse/PipeWire/D-Bus/etc.) remain OK when the stack needs them.

Graphics + local audio (including local daemon sockets) = fine. Internet / remote phone-home = **defect**.

### 8.1 Clean rebuild policy (Implementer — mandatory, every change)

Users often launch `dist/Ditonira-x86_64.AppImage` or an old `build/ditonira` and see **pre-change** behavior. To prevent that:

**On every code/resource/packaging change, before Done or before asking anyone to run the app, the Implementer MUST:**

1. Delete previous artifacts: `build/`, `build-appimage/`, `AppDir/`, `dist/`, and any `*.AppImage` left in the repo.
2. Reconfigure and Release-build from scratch into `build/`.
3. Rebuild the AppImage when shipping or when verification uses AppImage (`./packaging/build-appimage.sh`).
4. State the exact fresh binary path to run (`build/ditonira` and/or `dist/Ditonira-x86_64.AppImage`).

Never leave a stale AppImage beside a newer `build/ditonira` without saying which one is current. Cursor rule: `.cursor/rules/implementer-clean-rebuild.mdc` (`alwaysApply`).

## 9. Repository layout

```text
docs/
  ARCHITECTURE.md              # Architect-owned; technical decisions
  ARCHITECTURE_RX_DEEPCW.md    # DeepCW RX port (normative for receive)
  HANDOFF.md                   # Cross-role inbox / decisions / open points
  ENGINEERING_LOG.md           # Chronological, indexed bugs & improvements
  THIRD_PARTY.md               # Licenses (DeepCW, ORT, …) — Implementer creates
CMakeLists.txt
src/
  main.cpp
  MainWindow.h / MainWindow.cpp
  MorseEncoder.h / MorseEncoder.cpp
  KeyerEngine.h / KeyerEngine.cpp
  SerialKeyLine.h / SerialKeyLine.cpp
  rx/                          # DeepCW port modules (see ARCHITECTURE_RX_DEEPCW.md)
resources/
  ditonira.desktop
  icons/ditonira.png
  models/                      # deepcw_en.onnx (+ narrow when RX-4)
packaging/
  build-appimage.sh
tests/                         # Tester-owned scripts (optional until first suite)
README.md
```

## 10. Acceptance criteria

- Port list and refresh.
- RTS or DTR + polarity act as the key.
- WPM and text drive Morse TX.
- Stop aborts and leaves the line key-up.
- AppImage runs on Linux x86_64 **without installing app libraries** on the host (§8.0); release builds from Ubuntu 20.04 baseline (no `GLIBC_2.34+` requirement from a newer build host).
- All visible UI strings are English (§7.2).
- During TX, already-handled text is visually distinct from remaining text; Stop leaves a partial highlight (§7.3).
- During TX, **Esc** aborts the same as **Stop (Esc)**; when idle, Esc does not quit or wipe the form (§7.4).
- RX (when a DEC-005 phase is Done): acceptance bullets for that phase in [`ARCHITECTURE_RX_DEEPCW.md`](ARCHITECTURE_RX_DEEPCW.md) §9.
- Relevant decisions are recorded in this document / RX companion doc.

## 11. LLM agent roles (hard boundaries)

This project is developed by specialized LLM agents. Roles must **not** be crossed. Each agent reads the files it is allowed to touch and leaves work for other roles via [`HANDOFF.md`](HANDOFF.md) and [`ENGINEERING_LOG.md`](ENGINEERING_LOG.md).

### 11.1 Architect

| | |
|---|---|
| **Owns** | `docs/ARCHITECTURE.md` and other architecture / design decision docs under `docs/` that define *what* and *why* (not application source). |
| **Does** | Writes and updates architecture; records technical decisions the Implementer must follow; turns Tester findings into new architectural decisions when warranted; updates `HANDOFF.md` with decisions for Implementer and closed architectural questions. |
| **Must never** | Write, edit, or patch application/production code (`src/`, build scripts that constitute product logic, UI resources beyond what architecture explicitly delegates). Must never “just fix it in code.” |
| **May** | Propose file/module layouts, APIs, timing rules, packaging constraints, and acceptance criteria in documents only. |

### 11.2 Code Implementer

| | |
|---|---|
| **Owns** | Application source (`src/`), product `CMakeLists.txt`, packaging scripts, desktop/icon resources needed to ship the app. |
| **Does** | Reads engineering docs (`ARCHITECTURE.md`, active items in `HANDOFF.md`); implements code; compiles; updates `HANDOFF.md` with implementation status; appends resolved implementation work to `ENGINEERING_LOG.md` when closing a logged item. |
| **Must never** | Edit `docs/ARCHITECTURE.md` or redefine architecture decisions. If the docs are insufficient or conflicting, write a question / blocker in `HANDOFF.md` for the Architect. |
| **Must** | Follow decisions already recorded in architecture; prefer small, compile-verified changes; **always** clean-rebuild per §8.1 before Done or before asking anyone to run the app. |

### 11.3 Tester / Debugger

| | |
|---|---|
| **Owns** | Test scripts under `tests/`, reproduction notes, log analysis write-ups. |
| **Does** | Runs the app / AppImage / tests; collects and evaluates failure logs; writes **findings and recommended concepts** (not final architecture) into `HANDOFF.md` for the Architect; appends open or observed bugs to `ENGINEERING_LOG.md`; may write and maintain automated or manual test scripts. |
| **Must never** | Modify architecture documents (`ARCHITECTURE.md`) directly. Must never write or patch application production code under `src/`. |
| **May** | Suggest architectural options in HANDOFF (e.g. “consider moving timing to …”); the Architect alone promotes those into `ARCHITECTURE.md`. |

### 11.4 Boundary summary

```text
Architect     → docs (architecture), HANDOFF decisions     ✗ no src/
Implementer   → src/, build/packaging, HANDOFF status      ✗ no ARCHITECTURE.md
Tester        → tests/, logs, HANDOFF findings, ENG_LOG    ✗ no src/  ✗ no ARCHITECTURE.md
```

### 11.5 Cursor agent rules (enforce roles in the IDE)

Project rules under `.cursor/rules/` mirror §11 for LLM agents in Cursor. They do **not** replace this document; if a rule and this file disagree, **this file wins**, and the Architect must update the rule.

| File | Applies when |
|---|---|
| `roles-boundaries.mdc` | Always |
| `architect.mdc` | Working under `docs/` |
| `implementer.mdc` | Working under `src/`, `CMakeLists.txt`, `packaging/`, `resources/` |
| `implementer-clean-rebuild.mdc` | Always (Implementer clean-rebuild mandate) |
| `implementer-offline-lean.mdc` | Always (offline-only; no telemetry/downloads; lean bundle) |
| `tester.mdc` | Working under `tests/`, HANDOFF, ENGINEERING_LOG |

Implementer coding constraints for the MVP are summarized in `implementer.mdc` and detailed in §1–10 of this document. Work is dispatched via `HANDOFF.md` (e.g. DEC-001).

## 12. Cross-agent handoff mechanism

All inter-role communication that must survive chat sessions is written to:

1. **[`docs/HANDOFF.md`](HANDOFF.md)** — active inbox (decisions, questions, findings, blockers).
2. **[`docs/ENGINEERING_LOG.md`](ENGINEERING_LOG.md)** — durable chronological index of bugs and improvements.

### Flow

```text
Tester finds failure
    → ENGINEERING_LOG entry (Open)
    → HANDOFF "For Architect" (concepts / evidence)
Architect decides
    → updates ARCHITECTURE.md
    → HANDOFF "For Implementer" (decision id + what to change)
Implementer codes + compiles
    → HANDOFF status Done / Blocked
    → ENGINEERING_LOG entry Resolved (how)
Tester verifies
    → ENGINEERING_LOG confirmation / reopen if needed
```

### Rules

- Do not rely on chat history alone for decisions; if it matters, write it to `HANDOFF.md` or `ENGINEERING_LOG.md`.
- Architect closes architectural questions; Implementer closes implementation tasks; Tester closes verification.
- One HANDOFF item should name: **From**, **To**, **Related ENG-id** (if any), **Summary**, **Action needed**.
