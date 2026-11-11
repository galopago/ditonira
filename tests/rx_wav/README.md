# RX offline WAV fixtures (Tester)

Not shipped in AppImage. Used with `build/deepcw_wav_decode` and live RX (play into mic / virtual cable).

## RX-4 pileup multi-track (DEC-005)

Aligned with `ARCHITECTURE_RX_DEEPCW.md` §5.3 (`PILEUP_WINDOW_S=8`, max 5 peaks, lock ≈12 dB).

| File | Rate | Notes |
|---|---|---|
| `pileup_4track_multispeed.wav` | 48 kHz mono | **Primary** — play this into Ditonira |
| `pileup_4track_multispeed_9600.wav` | 9600 Hz mono | Same mix, capture-rate copy |
| `pileup_4track_ref.txt` | — | Per-track frequency / WPM / reference text |
| `pileup_4track_manifest.json` | — | Machine-readable manifest |

| Track | Freq | WPM | Reference text |
|---|---|---|---|
| 1 | 500 Hz | 15 | `CQ CQ DE W1AW W1AW K` |
| 2 | 700 Hz | 20 | `CQ DE N0CALL TEST K` |
| 3 | 900 Hz | 25 | `QRZ DE HA5BT HA5BT K` |
| 4 | 1100 Hz | 18 | `5NN 599 TU DE K6XX` |

**How to use:** Start Ditonira → Mode **Pileup** → Start RX → play `pileup_4track_multispeed.wav` (loop) into the selected input. Expect cyan markers near those Hz and text in **Pileup tracks**. Also smoke Mode **Normal** tuned to one tone (e.g. Center 700 Hz).

Synth: raised-cosine keying (~5 ms), staggered starts, looped ~36 s, mild AWGN ≈15 dB SNR.

## Reproduce synthetic HF suite

From repo root, regenerate raised-cosine + AWGN/QSB/QRN fixtures under `/tmp/cw_testdata/synth_hf` (see Tester notes in `docs/ENGINEERING_LOG.md` ENG-005).

## External documented samples

| Source | Path / URL | Known text | Notes |
|---|---|---|---|
| Ditonira smoke | `resources/models/testdata_cq_800hz.wav` | `CQ` | Clean 800 Hz |
| cho45/CW-Morse-ML | `samples/{2,5,10}dB-25wpm-600Hz.ogg` | `CQ CQ DE JH1UMV JH1UMV PSE K` | SNR labeled; convert to 9600 Hz WAV |
| SM2CEW | https://www.sm2cew.com/plaintext59_17.wav | unpublished | Documented −17 dB |

```bash
export LD_LIBRARY_PATH="$(pwd)/.deps/onnxruntime-linux-x64-1.19.2/lib:$LD_LIBRARY_PATH"
./build/deepcw_wav_decode --model resources/models/deepcw_en.onnx <wav>
# optional: --bandpass centerHz widthHz
```
