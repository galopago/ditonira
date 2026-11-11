# Ditonira

Linux CW (Morse) keyer: transmits text by keying **RTS** or **DTR** on a USB–serial converter.

Technical decisions: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

**Language:** all project documentation (`README`, `docs/*`, Cursor rules) and **end-user UI strings** are **English**.

**Offline:** Ditonira does **not** use the internet (no telemetry, no downloading fonts/models/updates). Everything needed ships inside the AppImage. Local desktop sockets (e.g. audio via Pulse/PipeWire, D-Bus) may still be used by the OS stack — that is not “going online.”

## Features (current)

### Transmit (shipped)

- **Serial port** selection with **Refresh** (`/dev/ttyUSB*`, `/dev/ttyACM*`, etc.).
- **Key line:** **DTR** or **RTS** (unused line stays idle / key-up).
- **Invert polarity** (asserted = key-down by default; checkbox inverts).
- **Speed:** 5–50 WPM (PARIS timing, unit = `1200 / WPM` ms).
- **Text to transmit** with **Transmit** and **Stop (Esc)**.
- **Abort with Esc:** the **Esc** key stops TX the same way as the **Stop (Esc)** button (window shortcut; works while the text box has focus). When idle, Esc does nothing (does not quit or clear the form).
- **English UI** (labels, buttons, status, and errors).
- **TX progress coloring:** already-sent characters use a distinct color (`QPalette::Link`); remaining text stays normal. Stop/Esc keeps the partial highlight; editing the text clears it; a new **Transmit** resets progress.
- While transmitting: port, line, polarity, WPM, and Transmit are locked; the text box is read-only; **Stop (Esc)** stays enabled and leaves the key line key-up.
- **AppImage** x86_64 distribution (Qt 5 bundled; no system Qt required on the host).

### Receive (DeepCW port — shipped; Monitor awaiting Tester)

Neural CW decode ported from [DeepCW](https://github.com/e04/web-deep-cw-decoder) (ONNX Conformer-CTC, 9600 Hz STFT pipeline): live RX, waterfall, streaming transcript, pileup, and **Monitor/THRU** (RX → selectable output, optional center/BW bandpass — DEC-010 / §7.4). Architecture: [docs/ARCHITECTURE_RX_DEEPCW.md](docs/ARCHITECTURE_RX_DEEPCW.md).

Supported characters: A–Z, 0–9, `= / ? . ,`, and spaces. Everything else is omitted (not keyed).

Not in the MVP: local sidetone, paddle/iambic, CW receive, macros, Windows/macOS.

## Recent updates

| Item | What changed |
|---|---|
| ENG-010 | Monitor/THRU — route RX audio to headphones (DeepCW-style) |
| ENG-007 | **P0** AppImage must be portable (Ubuntu 20.04 baseline; no host lib version hell) |
| ENG-006 | RX **waterfall** required for debugging (DEC-006 / RX-2b) |
| ENG-005 | Neural CW **receive** — DeepCW ONNX port (architecture + DEC-005; implementation phased) |
| ENG-004 | Stop TX via **Esc**; button label **`Stop (Esc)`** |
| ENG-002 | Live TX progress coloring (sent vs pending text) |
| ENG-001 | English-only UI strings |
| ENG-003 | Clean-rebuild policy (delete stale `build/` / AppImage before shipping a change) |

## Build requirements

- CMake ≥ 3.16
- C++17 compiler (GCC/Clang)
- Qt 5: `qtbase5-dev`
- Qt Serial Port: runtime **and** development — `libqt5serialport5` + `libqt5serialport5-dev`

On Ubuntu/Debian:

```bash
sudo apt install build-essential cmake qtbase5-dev libqt5serialport5-dev
```

`libqt5serialport5` (runtime) alone is not enough to build: you need the headers and CMake config from `libqt5serialport5-dev`. If you cannot install the `-dev` package, headers may be extracted under `.deps/qt5serialport-dev` (fallback in `CMakeLists.txt`).

## Build and run

After any code change, remove old artifacts first so you do not launch a stale AppImage or binary:

```bash
rm -rf build build-appimage AppDir dist
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/ditonira
```

Implementers must always follow this clean-rebuild policy (`docs/ARCHITECTURE.md` §8.1).

## Usage

1. Connect the USB–serial adapter (typical CW interfaces use DTR or RTS as the key line).
2. Choose **Port:** and **Line:** (**DTR** or **RTS**). Use **Refresh** if the adapter was plugged in after startup.
3. Set **WPM:** (5–50) and, if needed, enable **Invert polarity**.
4. Enter text under **Text to transmit:** and press **Transmit**.
5. To abort: click **Stop (Esc)** or press **Esc**. Both leave the key line key-up and keep the partial progress color. Esc does nothing when you are not transmitting.
6. Watch progress: sent text changes color as each character is keyed.

## Serial port permissions

The user must be able to open `/dev/ttyUSB*`, `/dev/ttyACM*`, etc. On most distros:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in for the group membership to take effect.

## AppImage (what you run on any Linux PC)

**You (end user):** no Docker, no compile, no package installs for Qt/ORT/ICU.

```bash
chmod +x Ditonira-x86_64.AppImage
./Ditonira-x86_64.AppImage
```

The AppImage embeds Qt 5, ICU (as needed), ONNX Runtime, and the CW model. The host only needs a normal desktop + audio (e.g. ALSA) and serial permission (`dialout`) — not matching library versions from apt.

See `docs/ARCHITECTURE.md` §8.0.

### Packaging notes (developers / Implementer only — not for end users)

To **produce** the portable AppImage (Ubuntu 20.04 / glibc 2.31 baseline):

```bash
./packaging/build-appimage-docker.sh
```

That script builds inside an `ubuntu:20.04` container (Docker or Podman on the Implementer machine), runs the **GLIBC ≤ 2.31** gate and the **lean/offline** gate (no Network/CUPS/TLS/QML leftovers; local audio/D-Bus IPC kept), and writes `dist/Ditonira-x86_64.AppImage`. Do **not** ship an AppImage built only on a bleeding-edge host if it requires `GLIBC_2.34+`. Local `build/ditonira` on a newer machine remains fine for day-to-day development.

## Development docs

| Doc | Purpose |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Technical decisions (source of truth) |
| [docs/ARCHITECTURE_RX_DEEPCW.md](docs/ARCHITECTURE_RX_DEEPCW.md) | DeepCW receive port (normative DSP/ML/UI phases) |
| [docs/HANDOFF.md](docs/HANDOFF.md) | Cross-role inbox for LLM agents |
| [docs/ENGINEERING_LOG.md](docs/ENGINEERING_LOG.md) | Bug / improvement index (`ENG-XXX`) |

## License

Project code: add the license you prefer when publishing. Qt 5 is distributed under LGPLv3; when shipping an AppImage, comply with Qt’s license obligations.
