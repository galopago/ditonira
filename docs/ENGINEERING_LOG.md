# Engineering log

Chronological, indexed record of bugs and improvements.  
Newest entries at the **top** of the Entries section. Keep the Index table updated.

Language: English (see [ARCHITECTURE.md](ARCHITECTURE.md) language convention).

---

## Index

| ID | Date | Type | Title | Status |
|---|---|---|---|---|
| ENG-006 | 2026-11-09 | Improvement | RX waterfall required for debugging | Resolved |
| ENG-005 | 2026-11-09 | Improvement | Neural CW receive — DeepCW ONNX port | In progress |
| ENG-004 | 2026-11-08 | Improvement | Stop TX via Esc; button label Stop (Esc) | Resolved |
| ENG-003 | 2026-11-08 | Improvement | Always clean-rebuild; delete stale binaries/AppImages | Resolved |
| ENG-002 | 2026-11-08 | Improvement | TX progress: color already-transmitted text | Resolved |
| ENG-001 | 2026-11-08 | Improvement | English-only end-user UI strings | Resolved |

**Type:** `Bug` | `Improvement` | `Regression`  
**Status:** `Open` | `In progress` | `Resolved` | `Won't fix` | `Deferred`

---

## Entries

### ENG-006 — RX waterfall required for debugging
- **Date opened:** 2026-11-09
- **Type:** Improvement
- **Status:** Resolved
- **Reported by:** Human (via Architect)
- **Symptoms / goal:** Live waterfall/spectrogram is necessary to debug RX (audio presence, tone frequency, center/bandwidth). Promote from optional to **required** in architecture §7.3; complete as Phase RX-2b if RX-2 UI already shipped without it.
- **Evidence:** Product request 2026-11-09; `ARCHITECTURE_RX_DEEPCW.md` §7.3 / §9 RX-2b; DEC-006.
- **Handoff:** DEC-006 in `HANDOFF.md` (For Implementer).
- **Resolution:** Implementer (2026-11-09): Added `RxSpectrumWidget` + `DecodeEngine::spectrumUpdated` (~20 fps). Waterfall X=time / Y=100–1500 Hz; center+BW overlay; click/wheel tune; pause with TX; clear on Stop RX. Clean paths `build/ditonira`, `dist/Ditonira-x86_64.AppImage`.
  - **Human field note (2026-11-09):** Waterfall **looked correct visually** during the phone→camera-mic live RX session (same setup as ENG-005 field note).
  - Tester (2026-11-09) **PASS:** click/wheel↔Center; Stop RX freeze/clear; BW/center overlays; TX-pause freezes waterfall then resumes; TX regression OK. ENG-006 verification closed.

### ENG-005 — Neural CW receive — DeepCW ONNX port
- **Date opened:** 2026-11-09
- **Type:** Improvement
- **Status:** In progress
- **Reported by:** Human (via Architect)
- **Symptoms / goal:** Port the DeepCW web decoder’s real-time neural CW receive into Ditonira (Qt/C++/AppImage), matching the reference pipeline in `/home/libertad/Desktop/repos/web-deep-cw-decoder`.
- **Evidence:** Architect deep analysis; normative spec `docs/ARCHITECTURE_RX_DEEPCW.md`; handoff DEC-005.
- **Handoff:** DEC-005 in `HANDOFF.md` (For Implementer) — start Phase RX-1 (offline WAV + core DSP/ORT/CTC).
- **Resolution:**
  - Implementer (2026-11-09) **Phase RX-1 done:** `src/rx/` pipeline (Bluestein STFT 768/144, `noise_q90_log1p`, ORT wide EN, CTC blank 41); model + NOTICE/THIRD_PARTY; harness `build/deepcw_wav_decode`; smoke WAV → `CQ`. ORT in `.deps/onnxruntime-linux-x64-1.19.2`. RX-2+ not started.
  - Tester (2026-11-09) **Phase RX-1 verified:** Official smoke → `CQ`. Five synthetic known-text WAVs decoded with **0 CER** (exact REF match) at 18–25 WPM / 700–800 Hz. External pico-radio-9 WAVs produce coherent English (upstream `cw-content.txt` wrong vs audio). TX binary + AppImage still present. RX-1 closed; ENG-005 stays In progress until later phases.
  - Tester (2026-11-09) **Extended noisy/realism suite (post RX-1):**
    - **Synth HF channel** (raised-cosine ~5 ms keying fade + AWGN + mild QSB + QRN clicks, known REF): CER **0%** from +40…−8 dB on CQ-style strings; at **−10 dB** `5NN 599 TU` → `5NN 589 TU` (CER 10%); **−12 dB** still exact `CQ CQ DE TEST K`; **−15 dB** degraded; **−17 dB** collapse (`DLT`).
    - **External [cho45/CW-Morse-ML samples](https://github.com/cho45/CW-Morse-ML/tree/master/samples)** (documented SNR, REF from `generate.py` = `CQ CQ DE JH1UMV JH1UMV PSE K`): **10 dB / 5 dB exact**; **2 dB** near-miss `…JH1UM5…PSI…` (1–2 char errors).
    - **[SM2CEW plaintext59_17.wav](https://www.sm2cew.com/wavefiles.html)** (documented **−17 dB**, 59-char plaintext×2, **transcript not published**): almost empty decode (`WN` / blank) — consistent with synth collapse near −17 dB. Meteor-scatter DK3WG clip: no usable plaintext (expected).
    - ARRL W1AW MP3 archives: text files reachable; MP3 binaries redirect to missing.png from this host (not usable here).
  - Tester (2026-11-09) **Green-light Phase RX-2:** RX-1 offline decode accepted; Implementer authorized to start live RX UI (`ARCHITECTURE_RX_DEEPCW.md` Phase RX-2). RX-3/RX-4 still gated on RX-2 Tester PASS. See `HANDOFF.md` DEC-005.
  - Implementer (2026-11-09) **Phase RX-2 partial:** PortAudio capture + ring buffer + `DecodeEngine` QThread + `RxPanel` in MainWindow splitter; pause RX during TX; AppImage ships `deepcw_en.onnx` + ORT. Waterfall was optional at the time; Architect later made it required (ENG-006 / DEC-006 / RX-2b). Full RX-2 PASS gated on waterfall.
  - **Human field test (2026-11-09) — live RX acoustic robustness:** Morse audio generated from a **phone ~1 m** from the PC; capture via **camera microphone**. Operator **talked over** the Morse tone during the session; Ditonira still **decoded successfully**. Waterfall **appeared to work visually** (see ENG-006).
  - Tester (2026-11-09) **Phase RX-2 + waterfall PASS:** Formal checklist items 1–6 OK (tune sync, Stop, overlays, TX↔RX pause/resume, TX regression, controls). **Green-light Phase RX-3** on DEC-005. ENG-005 remains In progress until RX-3/RX-4 complete.
  - Implementer (2026-11-09) **Phase RX-3 done (awaiting Tester):** `StreamingTranscript` ports `useStreamingDecode.ts` EN commit semantics (min ~2 s pending/confirmed, ~1.25 s tail guard, ~30 s segment, ≥~1 s analysis). CTC `wordSpaceSpans`/`characterSpans`; capture sample tap → streaming buffer; `DecodeEngine::transcriptUpdated`; RxPanel **Transcript** + **Live tail**. Center/BW retune clears pending only. Clean paths: `build/ditonira`, `dist/Ditonira-x86_64.AppImage`. RX-4 still gated on RX-3 Tester PASS.


### ENG-004 — Stop TX via Esc; button label Stop (Esc)
- **Date opened:** 2026-11-08
- **Type:** Improvement
- **Status:** Resolved
- **Reported by:** Human (via Architect)
- **Symptoms / goal:** Operator can abort transmission with the Stop button or Esc. Button label should show the shortcut compactly: `Stop (Esc)`.
- **Evidence:** Product request; rules in `ARCHITECTURE.md` §7.2, §7.4, §10.
- **Handoff:** DEC-004 in `HANDOFF.md` (For Implementer).
- **Resolution:** Implementer (2026-11-08): `MainWindow` Stop label `Stop (Esc)`; window-level `QShortcut(Qt::Key_Escape)` with `Qt::WindowShortcut` connected to `onStop` (same `requestStop()` path). `onStop` returns immediately when idle so Esc does not quit or clear the form. Clean rebuild paths: `build/ditonira`, `dist/Ditonira-x86_64.AppImage`. Tester (2026-11-08): verified — Esc aborts TX successfully; ENG-004 accepted. Docs (2026-11-08): README Features / Recent updates and ARCHITECTURE MVP scope reflect shipped Esc stop.

### ENG-003 — Always clean-rebuild; delete stale binaries/AppImages
- **Date opened:** 2026-11-08
- **Type:** Improvement
- **Status:** Resolved
- **Reported by:** Human (via Architect)
- **Symptoms / goal:** After a code change, launching an old `dist/*.AppImage` or leftover binary shows the previous UI/behavior. Implementer must always wipe prior artifacts and Release-rebuild before Done or before asking anyone to run the app.
- **Evidence:** User reported seeing the old app when launching after a change.
- **Handoff:** DEC-003 in `HANDOFF.md`; policy `ARCHITECTURE.md` §8.1; rule `.cursor/rules/implementer-clean-rebuild.mdc` (alwaysApply).
- **Resolution:** Implementer (2026-11-08): First compliant clean rebuild applied — stopped running instances; deleted `build/`, `build-appimage/`, `AppDir/`, `dist/`, stray AppImages; Release rebuild + `./packaging/build-appimage.sh`. Fresh paths: `build/ditonira`, `dist/Ditonira-x86_64.AppImage`. Binary strings verified English (`Port:`, `Transmit`, `Invert polarity`). DEC-003 remains standing Open in HANDOFF for every future change.

### ENG-002 — TX progress: color already-transmitted text
- **Date opened:** 2026-11-08
- **Type:** Improvement
- **Status:** Resolved
- **Reported by:** Human (via Architect)
- **Symptoms / goal:** Operator should see how far transmission has advanced by showing already-handled characters in a different color from remaining text.
- **Evidence:** Product request; architectural rules in `ARCHITECTURE.md` §6, §7.3, §10.
- **Handoff:** DEC-002 in `HANDOFF.md` (For Implementer).
- **Resolution:** Implementer (2026-11-08): `KeyerEngine` walks `Job::text` and emits `progressChanged(charsCompleted)` per §7.3; `MainWindow` styles `[0, charsCompleted)` with `QPlainTextEdit::setExtraSelections` and `QPalette::Link` foreground (text content not mutated). Reset on Transmit; partial highlight retained on Stop; cleared when the user edits after TX. Tester (2026-11-08): verified — transmitted range shows in blue (Link palette); progress coloring accepted.

### ENG-001 — English-only end-user UI strings
- **Date opened:** 2026-11-08
- **Type:** Improvement
- **Status:** Resolved
- **Reported by:** Human (via Architect)
- **Symptoms / goal:** UI was Spanish; product decision is English-only UI for the MVP (docs already English).
- **Evidence:** Spanish `tr()` strings in `MainWindow` / `KeyerEngine`; decision recorded in `ARCHITECTURE.md` Language convention and §7.2.
- **Handoff:** DEC-002 in `HANDOFF.md` (For Implementer).
- **Resolution:** Implementer (2026-11-08): Replaced all end-user labels, buttons, status, and error strings in `MainWindow` and `KeyerEngine` with the English catalog from §7.2. Tester (2026-11-08): verified — visible UI/status/errors match §7.2 English catalog; language change accepted.

---

## ID rules

1. IDs are monotonic: `ENG-001`, `ENG-002`, …
2. Never reuse an ID; reopen by setting Status back to `Open` and adding a dated note under Resolution history.
3. Every Resolved entry must state **how** it was fixed (architectural decision id/section and/or code change summary) and **who verified** (role).
