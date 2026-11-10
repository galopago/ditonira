# Ditonira

Linux CW (Morse) keyer: transmits text by keying **RTS** or **DTR** on a USB–serial converter.

Technical decisions: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

**Language:** all project documentation (`README`, `docs/*`, Cursor rules) and **end-user UI strings** are **English**.

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

### Receive (in progress — DeepCW port)

Neural CW decode ported from [DeepCW](https://github.com/e04/web-deep-cw-decoder) (ONNX Conformer-CTC, 9600 Hz STFT pipeline). Live RX UI includes a **required waterfall** for tuning/debugging (center/bandwidth overlay). Architecture: [docs/ARCHITECTURE_RX_DEEPCW.md](docs/ARCHITECTURE_RX_DEEPCW.md) §7.3. Phases: DEC-005 / DEC-006.

Supported characters: A–Z, 0–9, `= / ? . ,`, and spaces. Everything else is omitted (not keyed).

Not in the MVP: local sidetone, paddle/iambic, CW receive, macros, Windows/macOS.

## Recent updates

| Item | What changed |
|---|---|
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

## AppImage (no system Qt required on the host)

Produces a self-contained x86_64 binary that embeds Qt 5. Prefer a clean tree first (see Build and run), then:

```bash
./packaging/build-appimage.sh
./dist/Ditonira-x86_64.AppImage
```

Script requirements: `curl`, `cmake`, Qt5 build toolchain, and FUSE or `--appimage-extract` support (the script falls back).

For best compatibility with older distros, build the AppImage on **Ubuntu 20.04** (older glibc). The AppImage embeds Qt libraries; it does not replace the host glibc.

After rebuilding the app, do not launch an old AppImage left under `dist/` — rebuild it or run `./build/ditonira`.

## Development docs

| Doc | Purpose |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Technical decisions (source of truth) |
| [docs/ARCHITECTURE_RX_DEEPCW.md](docs/ARCHITECTURE_RX_DEEPCW.md) | DeepCW receive port (normative DSP/ML/UI phases) |
| [docs/HANDOFF.md](docs/HANDOFF.md) | Cross-role inbox for LLM agents |
| [docs/ENGINEERING_LOG.md](docs/ENGINEERING_LOG.md) | Bug / improvement index (`ENG-XXX`) |

## License

Project code: add the license you prefer when publishing. Qt 5 is distributed under LGPLv3; when shipping an AppImage, comply with Qt’s license obligations.
