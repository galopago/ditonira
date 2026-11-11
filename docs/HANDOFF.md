# Handoff board

Active inbox between LLM roles. Oldest open items first within each section.  
Move completed items to the archive at the bottom (or delete after they appear in `ENGINEERING_LOG.md`).

Role rules: see [ARCHITECTURE.md §11](ARCHITECTURE.md).  
Cursor agent rules: `.cursor/rules/` (`roles-boundaries`, `implementer`, `architect`, `tester`).  
Language: **English only** for all documentation and handoff entries (see ARCHITECTURE language convention).

---

## For Architect

_None._

---

## For Implementer

### 2026-11-10 — DEC-010 Monitor / THRU receive audio routing (headphones)
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-010
- **Summary:** Like DeepCW **THRU**: let the operator **route RX audio** to a selectable **output** (headphones/speakers), optionally bandpass-filtered to current Center/Bandwidth. Spec: `ARCHITECTURE_RX_DEEPCW.md` **§7.4** (Phase RX-5). Classical filter only — not neural NR.
- **Done by Implementer (2026-11-10):**
  1. UI on `RxPanel`: **Output** combo + Refresh, **Enable monitor**, **Filtered (center/BW)** (default on).
  2. Dedicated PortAudio output via `AudioMonitor`; capture mirrors 9600 Hz into lock-free `MonitorFifo`; optional `StreamingBandpass` (4-pass RBJ) tracks Center/BW.
  3. §10.1: PA out callback uses preallocated scratch + filter state; no heap/mutex. TX pause mutes monitor; Stop RX keeps silence while decode path stops independently.
  4. English strings; no Qt Multimedia / neural NR / downloads. Offline lean unchanged.
  5. Clean-rebuild + factory AppImage: `build/ditonira`, `dist/Ditonira-x86_64.AppImage` (GLIBC **2.29** + lean gate PASS).
- **Status:** Done

### 2026-11-10 — DEC-009 Harden audio/RX path (ENG-009 pileup segfault)
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-009
- **Summary:** Tester: AppImage **segfault** during multi-tone / Pileup RX (`pileup_4track_multispeed.wav`). ALSA probe messages are **not** the bug to “fix away.” Architecture: `ARCHITECTURE_RX_DEEPCW.md` **§10.1**.
- **Done by Implementer (2026-11-10):**
  1. **Root cause (ASan):** `PileupTracker::update` used `std::vector<bool>` + `push_back` into `m_tracked` while indexing `matched` → SEGV in `_Bit_reference::operator bool` (`PileupTracker.cpp`). Fixed with `vector<char>` + deferred newcomers.
  2. **§10.1 hardening:** `PortAudioHost` refcounted init; `listInputDevices` no longer `Pa_Terminate`s live PA; lock-free `AudioRingBuffer`; PA callback uses preallocated resample scratch only (no heap/tap/mutex); DecodeEngine pulls samples via `consumeNew` for streaming.
  3. **Evidence:** ASan before = SEGV stack above; after = `./build/pileup_wav_smoke tests/rx_wav/pileup_4track_multispeed_9600.wav` → `OK … hops=8` (exit 0). `gdb` not installed on build host; ASan used instead.
  4. Factory AppImage rebuilt: `dist/Ditonira-x86_64.AppImage` (GLIBC + lean gates PASS). Also `build/ditonira`.
- **Status:** Done + **Tester PASS** (2026-11-10 — no segfault in live Pileup)

### 2026-11-10 — DEC-008 Offline-only lean AppImage (NO internet/telemetry/downloads)
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-008
- **Summary:** Product is **offline + lean**: no internet, no telemetry, no runtime downloads, no unused HTTP/CUPS/TLS stacks. **Clarification:** **local** sockets (Pulse/PipeWire, D-Bus, Unix IPC) **may be required** by audio/GUI — do **not** strip or forbid those. Forbidden = remote/internet. See `ARCHITECTURE.md` §2.1 / §8.0.5; `implementer-offline-lean.mdc`.
- **Done by Implementer (2026-11-10):**
  1. Audit: CMake/`src` link only Widgets+SerialPort (+ PortAudio/ORT); no `QNetwork*` / telemetry / runtime downloads.
  2. Packaging: `strip-lean-appdir.sh` after linuxdeploy; `check-appimage-lean.sh` gate; `--exclude-library` for Network/Print/CUPS/TLS; pack via `appimagetool` so strip sticks.
  3. Factory rebuild: **Lean gate PASS** + **GLIBC gate PASS** (newest **2.29**). Local D-Bus/audio IPC libs retained on purpose.
  4. Fresh end-user file: `dist/Ditonira-x86_64.AppImage`.
- **Status:** Done

### 2026-11-10 — DEC-007 P0 Self-contained AppImage (user runs ONE file)
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-007
- **Summary:** User ask = **one AppImage that runs on another Linux without installing libs**. Current file fails elsewhere (`GLIBC_2.34`). Fix the **artifact**. Do **not** tell the human to install Docker, compile on an old PC, or change their workflow — that is **not** the product.
- **Done by Implementer (2026-11-10):** Factory under `packaging/` (`Dockerfile` ubuntu:20.04, `build-appimage-docker.sh`, `factory-build.sh`, `check-appimage-glibc.sh`). Shipping AppImage rebuilt in that factory. **GLIBC gate PASS** — newest on `usr/bin/ditonira` is **GLIBC_2.29** (≤ 2.31). Qt/ORT/ICU bundled. README: user = chmod/run only; Docker notes under Developers. Factory record: `dist/Ditonira-x86_64.AppImage.factory.txt` (`ditonira-appimage-factory:20.04`, id `sha256:2c6c0fdd76159fef902e5cbf676dbb100405b9b9d51bbc233b656c14bb7cdb41`).
- **End-user file:** `dist/Ditonira-x86_64.AppImage` — `chmod +x` and run (no build steps).
- **Status:** Done + **Tester PASS** (2026-11-10 second Linux)

### 2026-11-09 — DEC-005 Port DeepCW neural CW receive into Ditonira
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-005
- **Summary:** Port DeepCW neural CW decode into Ditonira (phased). Normative: [`ARCHITECTURE_RX_DEEPCW.md`](ARCHITECTURE_RX_DEEPCW.md).
- **Phase RX-1:** **Done** + Tester PASS.
- **Phase RX-2 + RX-2b (waterfall):** **Done** + **Tester PASS / GREEN-LIGHT** (2026-11-09).
  - Live RX UI + `RxSpectrumWidget` waterfall (§7.3) verified: click/wheel↔Center; Stop RX freeze/clear; BW/center overlays; TX pauses RX + freezes waterfall then resumes; TX Esc/progress/polarity OK; controls/English OK. Prior field smoke: phone→camera mic decode OK.
- **Phase RX-3:** **Done** + **Tester PASS / GREEN-LIGHT** (2026-11-10). Streaming Transcript + Live tail verified (B1–B6): commit on word-space gaps; retune clears pending only; Stop/Start clears session; TX pause/resume OK; waterfall/Esc smoke OK.
- **Phase RX-4:** **Done** + **Tester PASS** (2026-11-10). Pileup multi-lock on fixture tones 500/700/900/1100 Hz + cyan markers; decode partial but accepted as relatively functional; residual false lock ~162 Hz and non-zero CER noted, not blocking. DEC-009 crash fix verified live.
- **Status:** Done (all RX phases Tester-accepted; ENG-005 Resolved)

### 2026-11-08 — DEC-003 Standing rule: always clean-rebuild (no stale binaries)
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-003
- **Summary:** Standing process rule. After every change, delete all old binaries/artifacts and Release-rebuild from scratch before Done or before asking the user/Tester to run anything. Stale AppImages were causing “I launched the change but still see the old app.”
- **Action needed (every future handoff, permanently):**
  1. Follow `ARCHITECTURE.md` §8.1 and `.cursor/rules/implementer-clean-rebuild.mdc`.
  2. Stop any running `ditonira` / old AppImage processes, then `rm -rf build build-appimage AppDir dist` (and stray `*.AppImage`).
  3. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`.
  4. Rebuild AppImage: `./packaging/build-appimage.sh` (so `dist/` is never a leftover from a prior tree).
  5. In HANDOFF Done notes, always name the **fresh** path(s) to run.
- **Immediate action (2026-11-08):** Done — wiped artifacts, killed old processes, fresh Release + AppImage for DEC-002 tree. Fresh paths noted on Tester item. ENG-003 Resolved.
- **Status:** Open (standing — keep visible; do not archive as “one-shot done”)

---

## For Tester

### 2026-11-10 — Verify DEC-010 Monitor / THRU (RX-5)
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-010
- **Summary:** Phase RX-5 — route live RX audio to headphones/speakers with optional center/BW bandpass (`ARCHITECTURE_RX_DEEPCW.md` §7.4).
- **Fresh paths (clean-rebuild §8.1):** `build/ditonira` · `dist/Ditonira-x86_64.AppImage` (factory GLIBC 2.29 + lean PASS).
- **Action needed (acceptance §7.4):**
  1. Select **Output**, check **Enable monitor**, Start RX → hear RX in headphones/speakers.
  2. **Filtered (center/BW)** on → filter follows Center/Bandwidth; off → unfiltered.
  3. Uncheck Enable monitor → silence while decode continues.
  4. TX mutes monitor; resume after TX; no segfault.
  5. AppImage still portable / lean (spot-check OK).
- **Status:** Open

---

## Blockers

<!-- Anything that stops progress; name the owner expected to clear it. -->

_None._

---

## Template (copy into a section)

```markdown
### YYYY-MM-DD — short title
- **From:** Architect | Implementer | Tester
- **To:** Architect | Implementer | Tester
- **ENG:** ENG-XXX or n/a
- **Summary:** …
- **Action needed:** …
- **Status:** Open | In progress | Done
```

---

## Archive (recently closed handoffs)

### 2026-11-10 — Verify DEC-005 Phase RX-4 (pileup + narrow) + DEC-009 no-segfault
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-005, ENG-009
- **Summary:** Phase RX-4 pileup + DEC-009 audio/tracker hardening.
- **Done by Tester (2026-11-10):** Live Pileup with `pileup_4track_multispeed.wav` — **no segfault**. Locks ~500/700/900/1100 Hz + cyan markers; decode fragments accepted as **relatively functional** (Human/Tester). Spurious ~162 Hz empty track and imperfect CER noted, not blocking. **RX-4 PASS; ENG-009 Resolved; ENG-005 Resolved.**
- **Status:** Done

### 2026-11-10 — ENG-009 AppImage segfault during pileup (Architect intake)
- **From:** Tester
- **To:** Architect
- **ENG:** ENG-009
- **Summary:** Segfault on multi-tone/Pileup AppImage test; ALSA probe noise + RT/PA suspects from code review.
- **Done by Architect:** Spec `ARCHITECTURE_RX_DEEPCW.md` §10.1 + **DEC-009** for Implementer; RX-4 remains blocked until fix.
- **Status:** Done

### 2026-11-09 — Verify DEC-005 Phase RX-3 (streaming transcript)
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-005
- **Summary:** Phase RX-3 streaming committed transcript (`ARCHITECTURE_RX_DEEPCW.md` §5.2 / Phase RX-3). Word-space commit + live pending tail; waterfall/TX pause unchanged.
- **Done by Tester (2026-11-10):** B1–B6 PASS — Live tail updates then commits to Transcript on word-space gaps; Transcript grows; retune resets Live tail only; Stop/Start clears session; TX while RX pauses/resumes; waterfall click/wheel + Esc TX smoke OK. **RX-3 PASS; green-light RX-4** on DEC-005.
- **Status:** Done

### 2026-11-10 — Verify DEC-007 portable AppImage (second Linux) + DEC-008 lean
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-007, ENG-008
- **Summary:** Portable + lean AppImage from Ubuntu 20.04 factory. Must run on another machine **without** apt of Qt/ORT/ICU; no Network/CUPS/TLS product stack.
- **Done by Tester (2026-11-10):** Second Linux — AppImage starts with no missing-lib / GLIBC warnings; serial port list + keying OK; waterfall visible; some RX decode (weaker than primary PC — attributed to mic/acoustic path, not packaging); host **offline** (no internet) and app did not require network. **ENG-007 + ENG-008 PASS.**
- **Status:** Done

### 2026-11-09 — Verify DEC-005 Phase RX-2 (live RX UI) + DEC-006 waterfall
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-005, ENG-006
- **Summary:** Live RX UI + required waterfall (§7.3 / RX-2b). DEC-006 implemented.
- **Done by Tester (2026-11-09):** Formal checklist PASS — (1) click/wheel syncs Center Hz; (2) Stop RX freeze/clear OK; (3) BW/center markers update OK; (4) TX while RX pauses status + freezes waterfall, resumes after TX; (5) TX Esc/progress/polarity OK; (6) input/window/English controls OK. Prior field smoke retained (phone→camera mic decode + visual waterfall). **ENG-006 closed; RX-2 full PASS; green-light RX-3** on DEC-005.
- **Status:** Done

### 2026-11-09 — DEC-006 Required RX waterfall (debugging)
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-006
- **Summary:** Mandatory live waterfall (§7.3) as Phase RX-2b.
- **Done by Implementer:** `RxSpectrumWidget` in `RxPanel`; `DecodeEngine::spectrumUpdated` (~20 fps STFT column 100–1500 Hz); overlay center+BW; click/wheel→center; pause on TX / clear on Stop. Clean rebuild: `build/ditonira`, `dist/Ditonira-x86_64.AppImage`. Orientation documented: X=time (right=newest), Y=freq (top=1500 Hz).
- **Status:** Done

### 2026-11-09 — Verify DEC-005 Phase RX-1 (WAV harness)
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-005
- **Summary:** Offline DeepCW port acceptance (§9 Phase RX-1). STFT/spectrogram/ORT/CTC + one-shot WAV harness.
- **Done by Tester (2026-11-09):**
  1. Smoke: `deepcw_wav_decode … testdata_cq_800hz.wav` → `CQ` (exact).
  2. Exhaustive known-text synth WAVs (ITU timing, 9600 Hz): exact match for `CQ`; `CQ CQ DE TEST K`; `CQ CQ DE HA5BT HA5BT K`; `5NN 599 TU`; `CQ CQ DE W1AW` (700 Hz, with/without `--bandpass 700 250`).
  3. External: [bt-soft/pico-radio-9 `test/cw`](https://github.com/bt-soft/pico-radio-9/tree/master/test/cw) WAVs decode fluent English prose; upstream `cw-content.txt` (`CQ CQ CQ DE HA5BT…`) does **not** match those files (metadata mismatch, not a Ditonira failure). ARRL W1AW MP3s not downloadable here (redirect to missing.png). DeepCW README bench attachment is MP4 SNR demo without published transcript; decode is non-empty garbage-ish groups (expected without reference).
  4. TX still OK: `build/ditonira` starts under `QT_QPA_PLATFORM=offscreen`; `dist/Ditonira-x86_64.AppImage` present/executable.
  5. Phase RX-1 accepted; ENG-005 remains In progress for RX-2+.
  6. Follow-up noisy suite (cho45 / SM2CEW / HF synth) logged under ENG-005; **green-light issued for Phase RX-2** on DEC-005 Implementer item (2026-11-09).
- **Status:** Done

### 2026-11-08 — Verify DEC-004 Esc stop + `Stop (Esc)` label
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-004
- **Summary:** Stop button labeled `Stop (Esc)`; window-level Esc calls the same `onStop` → `requestStop()` path. Idle Esc is a no-op (does not quit or clear text).
- **Done by Tester:** Esc abort during TX verified OK. ENG-004 verification closed.
- **Status:** Done

### 2026-11-08 — DEC-004 Stop via Esc + button label `Stop (Esc)`
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-004
- **Summary:** Abort TX with Stop button or Esc; label `Stop (Esc)` (§7.2 / §7.4).
- **Done by Implementer:** Button text `Stop (Esc)`; `QShortcut(Qt::Key_Escape, WindowShortcut)` → `onStop`; idle guard so Esc does nothing when not transmitting. Timing/polarity/highlight unchanged. Clean rebuild §8.1: `build/ditonira`, `dist/Ditonira-x86_64.AppImage`.
- **Status:** Done

### 2026-11-08 — Verify DEC-002 English UI + TX progress coloring
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-001, ENG-002
- **Summary:** DEC-002 implemented. UI/status/errors are English (§7.2). `KeyerEngine` emits `progressChanged(int)`; `MainWindow` paints sent range via `QPlainTextEdit::setExtraSelections` with `QPalette::Link`.
- **Done by Tester:** English UI (§7.2) accepted. TX progress coloring accepted — transmitted `[0, charsCompleted)` appears blue (Link palette). ENG-001 and ENG-002 verification closed.
- **Status:** Done

### 2026-11-08 — DEC-002 English UI + TX progress coloring
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-001, ENG-002
- **Summary:** English UI (§7.2) + live TX progress coloring (§7.3).
- **Done by Implementer:**
  - All `MainWindow` / `KeyerEngine` user-visible strings switched to English catalog.
  - `KeyerEngine::progressChanged(int)` emitted while walking `Job::text` (mapped after marks+trailing gap/EOS; spaces after word gap; unknowns immediate; timing unchanged vs `MorseEncoder::encode`).
  - Progress painted with `setExtraSelections` + `QPalette::Link` foreground; reset on Transmit; partial kept on Stop; cleared on post-TX edit.
  - Release build: `build/ditonira`.
- **Status:** Done

### 2026-11-08 — Verify DEC-001 MVP acceptance (§10)
- **From:** Implementer
- **To:** Tester
- **ENG:** n/a
- **Summary:** MVP CW keyer implemented and Release-built per `docs/ARCHITECTURE.md` §1–10 / DEC-001. Ready for acceptance verification.
- **Done by Tester:** Manual acceptance OK on `build/ditonira` and/or AppImage. Confirmed §10: port list + refresh; RTS/DTR + polarity keying during TX (idle UI does not hold the line; invert asserts key-up only while port is open); WPM + text drive TX; Stop aborts and leaves line key-up; no failures opened in ENGINEERING_LOG.
- **Status:** Done

### 2026-11-08 — DEC-001 Implement MVP CW keyer per architecture
- **From:** Architect
- **To:** Implementer
- **ENG:** n/a
- **Summary:** Build Linux Qt5 CW keyer MVP per `docs/ARCHITECTURE.md` §1–10.
- **Done by Implementer:** Modules match §9 (`MorseEncoder`, `SerialKeyLine`, `KeyerEngine`, `MainWindow`, `main.cpp`). CMake Qt5 Widgets + SerialPort + RPATH `$ORIGIN/../lib`. Release build OK (`build/ditonira`). Existing AppImage at `dist/Ditonira-x86_64.AppImage`; script `packaging/build-appimage.sh` unchanged. Smoke: binary starts (`QT_QPA_PLATFORM=offscreen`). Hardware TX/Stop key-up left to Tester.
- **Status:** Done
