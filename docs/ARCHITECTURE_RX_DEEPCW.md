# Ditonira — Architecture: DeepCW receive port

**Status:** Approved for Implementer (phased).  
**Authority:** This document + [`ARCHITECTURE.md`](ARCHITECTURE.md). If they conflict on RX, **this file wins** until the Architect merges.  
**Language:** English only.

**Reference product analyzed:**  
`/home/libertad/Desktop/repos/web-deep-cw-decoder` (DeepCW PWA, UI v0.5.4)  
Upstream: https://github.com/e04/web-deep-cw-decoder · Engine: https://github.com/e04/deepcw-engine  

**Goal of this port:** Replicate DeepCW’s **real-time neural CW decode** inside Ditonira (Qt 5 / C++17 / Linux AppImage), with preprocessing and CTC post-processing **behaviorally equivalent** to the web app’s Normal mode. Ditonira already owns TX keying; do **not** port DeepCW’s Web Serial keyer.

---

## 0. Executive summary

DeepCW is a React/TypeScript PWA that:

1. Captures mono audio at **9600 Hz**.
2. Keeps a sliding ring buffer (default **12 s**).
3. Builds a magnitude spectrogram (Hann STFT **FFT=768**, **hop=144**, center reflect-pad).
4. Crops bins **400–1200 Hz** → **65 bins**, applies **`noise_q90_log1p`** normalization.
5. Runs ONNX **Conformer-CTC** (`spectrogram` → `log_probs`, **42** classes).
6. Greedy CTC-decodes to text (blank index **41**).

That pipeline (not the React UI) is what Ditonira must reimplement. Weights are already on disk under the reference repo’s `public/models/`. Code and engine are **MIT**; ship with attribution.

---

## 1. What DeepCW does (and what it does *not*)

### 1.1 In scope to port (product behavior)

| Capability | DeepCW location | Ditonira target |
|---|---|---|
| Live mic / line-in decode | `useAudioProcessing.ts` + worker | RX audio capture + decode thread |
| Sliding-window decode text | `useDecode.ts` + `DecodeDisplay` | RX transcript pane (plain + optional display) |
| Center frequency + bandwidth filter | Scope click/wheel + BANDWIDTH | RX controls + bandpass / shifted spectrogram |
| Spectrogram / scope UI | `Scope.tsx` | **Required** live waterfall (Phase RX-2 / RX-2b) — operator debugging (§7.3) |
| Streaming committed transcript | `useStreamingDecode.ts` | Phase RX-3 |
| Pileup multi-signal (narrow model) | pileup hooks + `en_narrow` | Phase RX-4 (optional) |

### 1.2 Explicitly out of scope for this port

- DeepCW **Web Serial keyer** / WPM TX UI (Ditonira already has RTS/DTR keyer).
- **RTTY** / DeepRTTY.
- **Japanese** model (`ja/…` referenced but **absent** in the analyzed checkout).
- Marketing “deep-learning noise reduction” (**not implemented** in the web repo; THRU is classical bandpass).
- WASM / WebGPU backends (use native **ONNX Runtime CPU**).
- PWA / Cloudflare / Mantine / React.
- Benchmark panel (optional later; not required for RX MVP).

### 1.3 Licensing (blocking checklist before shipping models)

| Asset | License | Action |
|---|---|---|
| DeepCW app code | MIT © 2026 e04 | May reimplement / port algorithms; keep notice |
| deepcw-engine | MIT (README) | Model usage OK with MIT notice |
| ONNX weights in `public/models/` | Treat as MIT via engine unless proven otherwise | Copy into Ditonira `resources/models/` + `THIRD_PARTY.md` |
| onnxruntime | MIT | Bundle shared libs in AppImage |
| Qt 5 | LGPLv3 | Existing Ditonira obligation |

Implementer must add `THIRD_PARTY.md` (or `docs/THIRD_PARTY.md`) listing DeepCW / deepcw-engine / ORT with copyright notices.

---

## 2. Source-of-truth files in the reference repo

Implementer should treat these as the **algorithmic specification** (read before coding):

| Path under `web-deep-cw-decoder` | Role |
|---|---|
| `src/const.ts` | Sample rate, FFT/hop, vocab, blank, freq bands, pileup constants |
| `src/stft.ts` | Hann window, FFT (radix-2 + **Bluestein** for 768), STFT framing |
| `src/utils/spectrogramUtils.ts` | Center-pad, crop 65 bins, shifted/narrow tensors, `noise_q90_log1p`, SNR |
| `src/utils/audioFilters.ts` | RBJ bandpass, **4 passes**, Q = f₀/BW |
| `src/utils/textDecoder.ts` | CTC greedy display/plain + spans |
| `src/workers/inferenceWorker.ts` | ORT I/O names, model paths, run loop |
| `src/hooks/useAudioProcessing.ts` | 9600 Hz capture, chunk 2048, ring buffer |
| `src/useDecode.ts` | Sliding-window decode cadence |
| `src/hooks/useStreamingDecode.ts` | Commit-by-word-space streaming |
| `public/models/en/39578E22-…` | Wide EN Conformer-CTC (~14.4 MiB) |
| `public/models/en_narrow/4C1FFB1B-…` | Narrow EN (~1.13 MiB) |

Do **not** port React components; port **numbers and math**.

---

## 3. Constants (must match DeepCW)

Copy these into a shared C++ header (e.g. `src/rx/DeepCwConstants.h`). Do not “improve” them without Architect approval.

```text
SAMPLE_RATE              = 9600
FFT_LENGTH               = 768
HOP_LENGTH               = 144
OUTPUT_TIME_STRIDE       = 2
OUTPUT_HOP_LENGTH        = 288          # HOP_LENGTH * OUTPUT_TIME_STRIDE
BIN_RESOLUTION           = 12.5         # SAMPLE_RATE / FFT_LENGTH
AUDIO_CHUNK_SAMPLES      = 2048         # ~213.3 ms at 9600
CENTER_PAD               = 384          # FFT_LENGTH / 2

DECODABLE_MIN_FREQ_HZ    = 400
DECODABLE_MAX_FREQ_HZ    = 1200
DEFAULT_CENTER_FREQ_HZ   = 800
DEFAULT_BANDWIDTH_HZ     = 800          # full decodable band

START_BIN                = ceil(400 / 12.5) = 32
END_BIN                  = floor(1200 / 12.5) + 1 = 97
CROPPED_BINS             = END_BIN - START_BIN = 65
NORMAL_CENTER_BIN        = round((32 + 97 - 1) / 2) = 64  # 800 Hz

NARROW_SHIFT_BINS        = 3

DEFAULT_DECODE_WINDOW_S  = 12
DECODE_WINDOW_OPTIONS_S  = {6, 12, 18, 30}

NOISE_LEVEL_QUANTILE     = 0.9
NOISE_LEVEL_RAYLEIGH_FACTOR = sqrt(-2 * ln(0.1))   # sqrt(-2*ln(1-0.9))
NOISE_LEVEL_MIN_RELATIVE_TO_PEAK = 1e-3
NOISE_LEVEL_MIN          = 1e-5

EN_VOCAB (41 symbols, indices 0..40):
  "," "." "/" "0".."9" "?" "A".."Z" " "
EN_BLANK_INDEX           = 41
EN_NUM_CLASSES           = 42
```

**Training note (do not change runtime math):** models were trained as if audio were **3200 Hz** with FFT 256 / hop 48; DeepCW scales ×3 at 9600 Hz to keep the same physical window (~80 ms), hop (~15 ms), and 12.5 Hz bins. Ditonira follows the **9600 path** used by the web app.

**Frame timing:** after center-pad, output CTC frame `t` is centered on sample `t * OUTPUT_HOP_LENGTH` (`getOutputFrameCenterSample` in `const.ts`).

**Output length relation:**  
`out_frames ≈ ((in_time_frames - 1) // 2) + 1` because the model frontend uses Conv stride 2 (`OUTPUT_TIME_STRIDE`).

---

## 4. End-to-end pipeline (Normative)

```text
Audio device
    → mono float32 @ SAMPLE_RATE (9600)   [resample if device ≠ 9600]
    → ring buffer of W seconds            [default W=12]
    → optional RBJ bandpass (4 passes)    [when filterFreq/width set]
    → center reflect-pad (±384 samples)
    → STFT Hann 768 / hop 144             [Bluestein FFT OK]
    → magnitude bins [START_BIN, END_BIN) → shape [T, 65]
    → noise_q90_log1p normalize
    → tensor float32 [1, 1, T, 65]        name: "spectrogram"
    → ONNX Runtime InferenceSession
    → tensor float32 [1, T_out, 42]       name: "log_probs"
    → greedy argmax per frame
    → CTC plainText (+ optional displayText)
    → UI (Qt)
```

### 4.1 Capture and ring buffer

**Reference:** `useAudioProcessing.ts`.

1. Open input device; produce a continuous stream of mono samples.
2. Convert to **float32** in approximately **[-1, 1]** (same spirit as Web Audio `getChannelData`).
3. Resample to exactly **9600 Hz** if the device rate differs (mandatory). Prefer a quality resampler (e.g. libsamplerate / Speex resampler). Document the chosen library in THIRD_PARTY.
4. Maintain a fixed-length ring buffer of `W * 9600` samples (default `W=12` → **115200** samples).
5. On each new chunk (target size **2048** samples ≈ 213 ms):
   - Shift buffer left by `chunkLen` (or equivalent ring write).
   - Append newest samples.
   - Bump a monotonic `version` / generation counter.
6. Decode thread consumes the latest buffer snapshot when idle/ready; never block the audio callback with ORT.

**Audio stack decision (Ditonira):**

| Choice | Decision |
|---|---|
| API | **PortAudio** (cross-backend ALSA/Pulse) **or** Qt5 `QAudioInput` + explicit resampler. Prefer PortAudio if Qt Multimedia sample-rate forcing is unreliable on the build host. |
| Threading | Audio callback / Qt audio thread → lock-free or short-mutex ring buffer → **dedicated `QThread` decode worker** owns ORT session. |
| Permissions | Document Pulse/PipeWire device access; no special group beyond normal audio. |

### 4.2 Optional bandpass (RBJ, 4 passes)

**Reference:** `audioFilters.ts` — port **exactly**.

```text
q     = centerFreq / bandwidth
omega = 2π * centerFreq / sampleRate
alpha = sin(omega) / (2q)

b0 = alpha;  b1 = 0;  b2 = -alpha
a0 = 1 + alpha;  a1 = -2 cos(omega);  a2 = 1 - alpha
Normalize b*, a1, a2 by / a0
Apply biquad recursively for `passes = 4` (state reset each pass).
```

UI bandwidth options in DeepCW: **100 / 150 / 250 / 500 / 800 Hz**. Default effective full-band decode uses center **800** and width **800** (or `filterFreq = null` path that skips bandpass and uses raw crop — see §4.4).

### 4.3 Center reflect-pad

**Reference:** `centerPad` in `spectrogramUtils.ts`.

```text
If audio.length <= CENTER_PAD + 1 → return null (too short; skip decode).
padded.length = audio.length + 2*CENTER_PAD
Left:  padded[i] = audio[CENTER_PAD - i]           for i in [0, CENTER_PAD)
Mid:   copy audio at offset CENTER_PAD
Right: padded[CENTER_PAD + n + i] = audio[n - 2 - i] for i in [0, CENTER_PAD)
```

### 4.4 STFT

**Reference:** `stft.ts`.

1. Hann window length `FFT_LENGTH`:  
   `w[i] = 0.5 * (1 - cos(2π i / FFT_LENGTH))` for `i = 0..767`.
2. Frame count: `floor((signalLength - FFT_LENGTH) / HOP_LENGTH) + 1`.
3. For each frame starting at `i = 0, 144, 288, …`:
   - Copy `FFT_LENGTH` samples; multiply by Hann.
   - Real FFT size 768 (Bluestein or any correct DFT); output interleaved complex `[re0, im0, re1, im1, …]` length 1536, or equivalent planar form — **magnitude must match**.
4. Magnitude for bin `k`: `sqrt(re[k]^2 + im[k]^2)`.
5. Usable bins: `0 .. FFT_LENGTH/2` inclusive → **385** bins theoretical; crop uses 32..96.

**Bit-compatibility:** Prefer porting DeepCW’s Bluestein path (MIT) so results match the web app closely. Alternative FFT libraries (pocketfft, FFTW) are allowed if Tester confirms decode quality on the same WAV; Architect prefers **faithful port first**.

### 4.5 Wide spectrogram tensor (EN model)

**Reference:** `audioToSpectrogramTensor`.

1. Optionally bandpass (`filterFreq != null && filterWidth > 0`).
2. Center-pad → STFT.
3. For each time frame, copy magnitudes for `bin = START_BIN .. END_BIN-1` into row of length **65**.
4. Flatten row-major: size `T * 65`.
5. `normalizeSpectrogramNoiseLog1p` on the full flattened array.
6. Return dims **`[1, 1, T, 65]`**, dtype **float32**, ORT input name **`spectrogram`**.

### 4.6 Normalization `noise_q90_log1p` (critical)

**Reference:** `normalizeSpectrogramNoiseLog1p`.

```text
sorted = copy(noiseSource); ascending sort
noiseLevel = max(
  sorted[floor(0.9 * (sorted.length - 1))] / RAYLEIGH_FACTOR,
  sorted[last] * 1e-3,
  1e-5
)
for each x in spectrogram:
  x = log1p(x / noiseLevel)
```

- Default `noiseSource == spectrogram`.
- Shifted path uses only **populated** bins as `noiseSource` (zeros excluded from noise estimate) — required for filtered/pileup modes.

**Do not** replace with mean/std or mel scales. Quantile index uses `floor(q * (N-1))` as in JS.

### 4.7 Shifted spectrogram (filtered UI path)

**Reference:** `audioToShiftedSpectrogramTensor`.

When the operator picks a center frequency narrower than the full band:

1. STFT full spectrum (no prior bandpass required for this path; DeepCW shifts bins).
2. Zero-fill 65-bin rows.
3. Copy a slice of width ≈ `bandwidth` centered on `targetFreq` into the cropped layout **centered at 800 Hz** (`destCenterIdx = NORMAL_CENTER_BIN - START_BIN` = **32**).
4. Normalize using only populated columns as `noiseSource`.
5. Same tensor shape `[1,1,T,65]` for the **wide** EN model.

### 4.8 Narrow spectrogram (pileup model only)

**Reference:** `audioToNarrowShiftedSpectrogramTensor`.

- Shape **`[1, 1, T, 3]`** — bins `targetBin-1, targetBin, targetBin+1`.
- Same normalization.
- Used only with **`en_narrow`** ONNX (Phase RX-4).

### 4.9 ONNX inference

| | Wide EN | Narrow EN |
|---|---|---|
| Source file (reference) | `public/models/en/39578E22-27CE-4AFB-989F-450345767A53` | `public/models/en_narrow/4C1FFB1B-6F80-4B73-A5D2-6089EAF8E102` |
| Ditonira resource name | `resources/models/deepcw_en.onnx` | `resources/models/deepcw_en_narrow.onnx` |
| Approx size | ~14.4 MiB | ~1.13 MiB |
| Input | `spectrogram` float32 `[B,1,T,65]` | `[B,1,T,3]` |
| Output | `log_probs` float32 `[B, T_out, 42]` | same classes |
| Architecture | Conformer-CTC, d_model≈192, 6 blocks, QDQ frontend | Smaller, 3 blocks, d_model≈64 |

**Runtime:** ONNX Runtime C++ **CPU** execution provider. Session created once on the decode thread; reuse for all runs. Enable graph optimization default.

**Ops note:** Frontend uses quantized Conv (QDQ). ORT build must include `QuantizeLinear` / `DequantizeLinear`.

**AppImage:** bundle `libonnxruntime.so*` next to the binary (RPATH `$ORIGIN/../lib`) like Qt libs.

### 4.10 CTC decode

**Reference:** `textDecoder.ts` with `ENGLISH_CONFIG`.

1. For each time `t`, `class = argmax_c log_probs[0,t,c]` (42 classes).
2. **`plainText` (primary UI string for Ditonira RX):** classic CTC — skip blank (41); skip repeats of the same non-blank; map index → vocab char; concatenate.
3. **`displayText` (optional, for scrolling alignment UI):** blank → space; repeat non-blank → space; else vocab char (keeps one character slot per frame).
4. Optional helpers: `wordSpaceSpans`, `characterSpans` (needed for streaming commit logic in Phase RX-3).

Vocab index order **must** match §3 exactly (comma first, space last before blank).

---

## 5. Decode loops (behavior to mirror)

### 5.1 Sliding-window decode (Phase RX-1 / RX-2) — required

**Reference:** `useDecode.ts` pattern.

- Whenever ring buffer `version` advances and the worker is free, run inference on a **snapshot** of the full window.
- Replace the on-screen decode string with the new `plainText` (or show `displayText` if implementing scroll-sync UI).
- Cadence ≈ one inference per audio chunk when previous finished; drop intermediate versions if behind (always decode **latest** snapshot).
- Changing window length reallocates the ring buffer and bumps version.

### 5.2 Streaming transcript (Phase RX-3)

**Reference:** `useStreamingDecode.ts` (port semantics, not React).

- Accumulate audio; run detailed decode periodically (≥ ~1 s interval, ≥ ~2 s audio).
- Commit text using **word-space spans** (stable gaps ≥ ~2 s with tail margin ~1.25 s).
- Cap segment length (~30 s); retain enough audio for redecode (~120 s in web — may reduce for memory if documented).
- Append committed text to a growing transcript widget; keep a “live tail” from the latest decode.

### 5.3 Pileup (Phase RX-4)

**Reference:** `const.ts` pileup constants + `estimateCwBinSnr` + tracking hooks.

| Constant | Value |
|---|---|
| `PILEUP_WINDOW_S` | 8 |
| `PILEUP_MIN/MAX_FREQ_HZ` | 100 / 2000 |
| `PILEUP_MAX_PEAKS` | 5 |
| `PILEUP_LOCK_SNR_THRESHOLD_DB` | 12 |
| `PILEUP_RELEASE_SNR_THRESHOLD_DB` | 9 |
| Detect hop | `CW_DETECT_HOP_LENGTH = 192` |

Each locked track decodes with **narrow** model at that frequency. Defer until Normal mode is solid.

---

## 6. Ditonira module layout (Implementer)

Extend repo layout (TX modules unchanged):

```text
src/
  … existing TX …
  rx/
    DeepCwConstants.h
    Stft.h / Stft.cpp                 # Hann + FFT 768 (Bluestein port)
    BandpassFilter.h / .cpp           # RBJ 4-pass
    SpectrogramBuilder.h / .cpp       # pad, crop, shift, normalize
    CtcDecoder.h / .cpp               # vocab + plain/display
    OnnxCwModel.h / .cpp              # ORT session wrapper
    AudioRingBuffer.h / .cpp
    AudioCapture.h / .cpp             # PortAudio or Qt audio + resample
    DecodeEngine.h / .cpp             # QThread worker: snapshot → text signals
    RxSpectrumWidget.h / .cpp         # Required waterfall / spectrum (§7.3)
    RxPanel.h / .cpp                  # controls + waterfall + transcript widgets
resources/
  models/
    deepcw_en.onnx                    # copied/renamed from reference
    deepcw_en_narrow.onnx             # Phase RX-4; may ship unused until then
    NOTICE.txt                        # UUID originals + checksums
docs/
  ARCHITECTURE_RX_DEEPCW.md           # this file
  THIRD_PARTY.md                      # licenses
```

`MainWindow` gains an RX area (tab, splitter, or stacked panel) **without breaking TX**. TX and RX may run concurrently only if Architect later allows; **default:** RX capture may run while TX is idle; while TX is transmitting, mute/pause RX capture to avoid feedback (decision: **pause RX while TX active**).

---

## 7. UI requirements (English)

### 7.1 RX controls (Phase RX-2 minimum)

| Control | Spec |
|---|---|
| Start / Stop RX | Begin/end capture + decode loop |
| Input device | Combo of capture devices + Refresh |
| Window | 6 / 12 / 18 / 30 s (default 12) |
| Bandwidth | 100 / 150 / 250 / 500 / 800 Hz |
| Center frequency | Spin **and** waterfall interaction; clamp to decodable 400–1200 for wide model |
| **Waterfall / spectrum** | **Required** — see §7.3 (debugging + tune aid) |
| Decode text | Read-only `QPlainTextEdit` / label showing latest `plainText` |
| Status | Ready / Listening / Decoding / Error (model load, device open) |

### 7.2 Strings (examples)

`Start RX`, `Stop RX`, `Input:`, `Window:`, `Bandwidth:`, `Center Hz:`, `Decoded text`, `Listening…`, `Could not open audio device`, `Could not load CW model`, `RX paused while transmitting`.

### 7.3 Waterfall / spectrum (required — debugging)

**Why:** Without a live time×frequency view, operators and Testers cannot tell whether audio is reaching the decoder, where the tone sits, or whether center/bandwidth are wrong. This is a **required** RX UI element (not optional).

**Minimum behavior (must all pass):**

1. **Live waterfall** (preferred) or scrolling spectrogram: vertical = frequency, horizontal = time (or the common transpose — document which), updating while RX is listening.
2. **Frequency axis** labeled in Hz; display span at least **100–1500 Hz** (DeepCW Scope display range). Decode math remains 400–1200 / shifted tensor as elsewhere.
3. **Center + bandwidth overlay:** draw the current decode center and the active bandwidth (passband brackets or shaded band) so filter settings are visible.
4. **Interaction:** click (or click-drag) sets **Center Hz**; mouse wheel adjusts center (and optionally bandwidth with a modifier — wheel→center is enough for MVP). Keep the numeric spin in sync.
5. **Update path:** spectrum data computed off the GUI thread (reuse STFT magnitudes from capture/decode path or a light dedicated FFT); GUI only paints. Target roughly **10–30 fps** while listening; freeze or clear when RX stopped; pause updates when RX paused for TX.
6. **Debugging usefulness:** a steady CW tone must appear as a visible ridge/line at the correct frequency; silence/noise must look visibly different. This is the acceptance cue for “audio is flowing.”
7. **Fidelity:** pixel-perfect DeepCW `Scope.tsx` / AnalyserNode 4096 clone is **not** required; a clear, readable waterfall that supports the above is enough. Prefer magnitide dB-ish mapping with a simple colormap (e.g. dark→bright); avoid unreadable flat greyscale with no contrast.

**Module:** `RxSpectrumWidget` inside `RxPanel`. Signal example: `DecodeEngine` or capture side emits `spectrumUpdated(freqs/mags or image strip)`; widget scrolls and overlays markers.

---

## 8. Build / packaging impact

1. **CMake:** locate ONNX Runtime (`find_package(onnxruntime)` or explicit `ONNXRUNTIME_ROOT`). Link `onnxruntime`.
2. **Install/AppImage:** copy `deepcw_en.onnx` into AppDir `usr/share/ditonira/models/` (or under `usr/bin/../share/...`). Resolve path via `QCoreApplication::applicationDirPath()` + relative `../share/ditonira/models/deepcw_en.onnx` (document exact layout in HANDOFF Done notes).
3. Bundle ORT `.so` with linuxdeploy **or** manual copy into `AppDir/usr/lib`.
4. Clean-rebuild policy (`ARCHITECTURE.md` §8.1) still applies; AppImage will grow (~+15–30 MB for model + ORT).
5. Do **not** require network download of weights at runtime for MVP (ship embedded).

---

## 9. Phased delivery (mandatory order)

### Phase RX-1 — Offline correctness (no GUI required beyond CLI/test)

**Acceptance:**

1. Copy models into `resources/models/` with NOTICE + checksums (SHA-256 of originals).
2. Implement STFT + spectrogram + CTC + ORT wide model.
3. Provide a small **WAV decode harness** (Tester-owned script later OK): read mono WAV, resample to 9600, run one-shot decode, print `plainText`.
4. On a known CW WAV (from deepcw-engine examples or a recorded tone), output is **plausible Morse text** (CER not required to match README heatmap yet, but must not be garbage). Prefer comparing against DeepCW web / deepcw-engine example on the **same** WAV.

### Phase RX-2 — Live RX UI

**Acceptance:**

1. Device select, Start/Stop, window, bandwidth, center freq.
2. Live sliding-window `plainText` updates while listening.
3. Pause RX automatically during Ditonira TX.
4. English UI; clean Release + AppImage with model+ORT.
5. Existing TX acceptance (§10 of main architecture) still passes.
6. **Required waterfall** per §7.3 (live update, freq axis, center/BW overlay, click/wheel to tune, tone visible for debug).

### Phase RX-2b — Waterfall completion (if RX-2 UI shipped without §7.3)

If controls+decode landed first without the waterfall, treat waterfall as **RX-2b**: same acceptance bullet 6; RX-2 is not fully accepted until §7.3 passes. Do not start RX-3 until waterfall is verified.

### Phase RX-3 — Streaming transcript

Committed transcript with word-space logic; live tail remains.

### Phase RX-4 — Pileup + narrow model

Only after RX-2 stable.

---

## 10. Concurrency & safety

```text
GUI thread (MainWindow / RxPanel)
    │ start/stop, parameters, show text
    ▼
DecodeEngine (QThread)
    │ build spectrogram + ORT + CTC
    ▲
AudioCapture callback → AudioRingBuffer (lock-free preferred)
```

- ORT + STFT **never** on GUI thread.
- Signals: `decodedTextChanged(QString)`, `rxStatusChanged(QString)`, `spectrumUpdated(...)` (**required** for waterfall §7.3).
- Destroy session on thread shutdown; stop capture before joining.

---

## 11. Testing guidance (for Tester after Implementer Done)

1. Load AppImage / `build/ditonira`; open RX; select device; Start; inject CW audio (radio or WAV→virtual cable).
2. Verify text appears and updates; Stop RX clears/stops updates.
3. Change bandwidth/center; confirm decode reacts.
4. Start TX while RX listening → RX pauses; after TX, RX can resume.
5. Confirm no regression on TX Stop (Esc), polarity, progress color.
6. Log ORT/model path failures into ENGINEERING_LOG if any.

---

## 12. Non-goals / anti-patterns

- Do not invent a classical Goertzel/HMM decoder “instead of” the ONNX model.
- Do not change vocab order, blank index, FFT/hop, or normalization.
- Do not call the model with 48 kHz audio without resampling.
- Do not block audio callbacks with inference.
- Do not download models from the network as the only path.
- Do not port JA until the ONNX file is present and Architect adds a decision.

---

## 13. Traceability

| Ditonira ID | Meaning |
|---|---|
| DEC-005 | Adopt DeepCW RX port per this document (phased) |
| ENG-005 | Feature: neural CW receive (DeepCW port) |

Reference analysis date: **2026-11-09**. Reference tree: local clone `web-deep-cw-decoder` as analyzed (models EN + EN-narrow present; JA missing).
