# RX offline WAV fixtures (Tester)

Not shipped in AppImage. Used with `build/deepcw_wav_decode`.

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
