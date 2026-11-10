# Handoff board

Active inbox between LLM roles. Oldest open items first within each section.  
Move completed items to the archive at the bottom (or delete after they appear in `ENGINEERING_LOG.md`).

Role rules: see [ARCHITECTURE.md §11](ARCHITECTURE.md).  
Cursor agent rules: `.cursor/rules/` (`roles-boundaries`, `implementer`, `architect`, `tester`).  
Language: **English only** for all documentation and handoff entries (see ARCHITECTURE language convention).

---

## For Architect

<!-- Tester findings and Implementer blockers that need an architectural decision. -->

_None._

---

## For Implementer

### 2026-11-09 — DEC-005 Port DeepCW neural CW receive into Ditonira
- **From:** Architect
- **To:** Implementer
- **ENG:** ENG-005
- **Summary:** Port DeepCW neural CW decode into Ditonira (phased). Normative: [`ARCHITECTURE_RX_DEEPCW.md`](ARCHITECTURE_RX_DEEPCW.md).
- **Phase RX-1:** **Done** + Tester PASS.
- **Phase RX-2 + RX-2b (waterfall):** **Done** + **Tester PASS / GREEN-LIGHT** (2026-11-09).
  - Live RX UI + `RxSpectrumWidget` waterfall (§7.3) verified: click/wheel↔Center; Stop RX freeze/clear; BW/center overlays; TX pauses RX + freezes waterfall then resumes; TX Esc/progress/polarity OK; controls/English OK. Prior field smoke: phone→camera mic decode OK.
- **Phase RX-3:** **Done by Implementer** (2026-11-09) — awaiting Tester. Streaming commit per §5.2: `StreamingTranscript` + CTC word-space spans; capture tap → pending audio; `DecodeEngine::transcriptUpdated(committed, pending)`; RxPanel **Transcript** + **Live tail**. Retune clears pending only. Fresh paths: `build/ditonira`, `dist/Ditonira-x86_64.AppImage`.
- **Do not start yet:** RX-4 pileup+narrow (wait for RX-3 Tester PASS).
- **Status:** Open (RX-3 implemented; **awaiting Tester**)

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

### 2026-11-09 — Verify DEC-005 Phase RX-3 (streaming transcript)
- **From:** Implementer
- **To:** Tester
- **ENG:** ENG-005
- **Summary:** Phase RX-3 streaming committed transcript (`ARCHITECTURE_RX_DEEPCW.md` §5.2 / Phase RX-3). Word-space commit + live pending tail; waterfall/TX pause unchanged.
- **Fresh paths (clean-rebuild §8.1):** `build/ditonira`, `dist/Ditonira-x86_64.AppImage`
- **Action needed:**
  1. Start RX with Morse audio (≥ a few words with spaces). Confirm **Live tail** updates within ~1–2 s, then text moves into **Transcript** after stable word-space gaps (not a full-window replace).
  2. Confirm Transcript grows across phrases; Live tail clears briefly after each commit then shows the new pending decode.
  3. Change Center Hz / Bandwidth while listening: Live tail resets; Transcript (committed) is kept.
  4. Stop RX / Start RX: transcript clears for a new session.
  5. TX while RX: still pauses capture + freezes waterfall; after TX, streaming resumes.
  6. Smoke: waterfall click/wheel and Esc Stop TX still OK.
  7. On PASS: mark Done here, note green-light (or not) for RX-4 on DEC-005; update ENG-005. Do **not** authorize RX-4 until RX-3 PASS.
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
