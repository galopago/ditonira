# Ditonira — Architecture

Living document of technical decisions. All implementation must follow this text.

## Language convention

- **All project documentation is English:** `README.md`, everything under `docs/`, Cursor rules (`.cursor/rules/`), engineering notes, and any other docs agents create or update.
- Code comments and identifiers (types, functions, variables): **English**.
- **All end-user UI strings are English** (labels, buttons, status messages, placeholders, errors, window title). No Spanish (or other) UI copy in the MVP; i18n/`tr()` may remain for future localization, but the source strings must be English.
- Agents must not write or leave new documentation or UI strings in Spanish (or other languages); translate or rewrite into English.

## 1. Goal

Linux GUI application that transmits Morse (CW) text by keying the **RTS** or **DTR** line of a USB–serial converter.

### MVP scope (shipped)

- Serial port selection.
- Keyer line selection: RTS or DTR.
- Invertible polarity (key-down = asserted or deasserted).
- Speed in WPM.
- Text box with the message to transmit.
- **Transmit** and **Stop (Esc)** actions; **Esc** aborts TX the same as Stop (§7.4) — implemented (`QShortcut` + idle no-op).
- English UI throughout.
- Live TX progress: already-transmitted text shown in a distinct color (§7.3).

### Out of scope (MVP)

- Local sidetone.
- Manual key / paddle / iambic.
- CW receive.
- Macros, decoder, memory banks.
- Windows / macOS (Linux x86_64 AppImage only).

## 2. Stack

| Piece | Decision | Rationale |
|---|---|---|
| Language | C++17 | Required; enough for `chrono`, `atomic`, etc. |
| GUI | Qt 5 Widgets | Better fit for older distros; mature AppImage tooling |
| Serial | QtSerialPort (`QSerialPort`) | Stable RTS/DTR API without data I/O |
| Build | CMake ≥ 3.16 | Standard `find_package(Qt5 …)` |
| Distribution | AppImage x86_64 | End user needs no system Qt; see `packaging/build-appimage.sh` |

If `find_package(Qt5SerialPort)` fails but the runtime `.so` is present, CMake may use headers extracted under `.deps/qt5serialport-dev` (local dev only; the AppImage still bundles the library). Prefer installing `libqt5serialport5-dev` on the build host.

**Qt 6 is not used** in the MVP: higher glibc/ABI risk on older hosts.

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

## 8. AppImage packaging

Goal: **no Qt packages required on the end-user host**.

1. Release build with relative RPATH (`$ORIGIN/../lib`).
2. Bundle with `linuxdeploy` + Qt plugin: binary, `libQt5Core/Gui/Widgets/SerialPort`, `platforms/libqxcb.so`, icon, `.desktop`.
3. Artifact: `Ditonira-x86_64.AppImage`.

**glibc compatibility:** prefer building on **Ubuntu 20.04** (or an equivalent container) so the AppImage runs on hosts no older than that base. The AppImage embeds Qt; it does not embed a newer glibc than the build host.

Device permission: the user needs access to `/dev/ttyUSB*`, `/dev/ttyACM*`, etc. (typically `dialout`). Not an app dependency; documented in the README.

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
  ARCHITECTURE.md          # Architect-owned; technical decisions
  HANDOFF.md               # Cross-role inbox / decisions / open points
  ENGINEERING_LOG.md       # Chronological, indexed bugs & improvements
CMakeLists.txt
src/
  main.cpp
  MainWindow.h / MainWindow.cpp
  MorseEncoder.h / MorseEncoder.cpp
  KeyerEngine.h / KeyerEngine.cpp
  SerialKeyLine.h / SerialKeyLine.cpp
resources/
  ditonira.desktop
  icons/ditonira.png
packaging/
  build-appimage.sh
tests/                     # Tester-owned scripts (optional until first suite)
README.md
```

## 10. Acceptance criteria

- Port list and refresh.
- RTS or DTR + polarity act as the key.
- WPM and text drive Morse TX.
- Stop aborts and leaves the line key-up.
- AppImage runs on Linux x86_64 without system Qt.
- All visible UI strings are English (§7.2).
- During TX, already-handled text is visually distinct from remaining text; Stop leaves a partial highlight (§7.3).
- During TX, **Esc** aborts the same as **Stop (Esc)**; when idle, Esc does not quit or wipe the form (§7.4).
- Relevant decisions are recorded in this document.

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
