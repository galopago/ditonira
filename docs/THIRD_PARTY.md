# Third-party licenses

English documentation. Shipping Ditonira (including AppImage) must preserve these notices.

## DeepCW / deepcw-engine

- Copyright © 2026 e04
- License: **MIT**
- Upstream: https://github.com/e04/web-deep-cw-decoder · https://github.com/e04/deepcw-engine
- Use in Ditonira: algorithmic port of the neural CW decode pipeline; ONNX weights copied into `resources/models/`.

MIT License text (summary obligation): include copyright and permission notice in distributions.

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

## ONNX Runtime

- Copyright © Microsoft Corporation
- License: **MIT**
- Upstream: https://github.com/microsoft/onnxruntime
- Use in Ditonira: CPU inference for `deepcw_en.onnx` and `deepcw_en_narrow.onnx` (bundled shared libraries in AppImage / `.deps` for local builds).

## PortAudio

- Copyright © PortAudio contributors / Ross Bencina et al.
- License: **MIT**
- Upstream: https://www.portaudio.com / https://github.com/PortAudio/portaudio
- Use in Ditonira: live RX capture (ALSA host API); static-linked from `.deps/portaudio-install`.

## Qt 5

- License: **LGPLv3** (as used by Ditonira MVP)
- Bundled in AppImage; respect LGPL obligations for Qt.

## Model checksum

| File | SHA-256 |
|---|---|
| `resources/models/deepcw_en.onnx` | `cd5ee38106348ad86a8f2d40ef1ce7dccea0d7f7fbfd0f2dee0fd1a8a28773ae` |
| `resources/models/deepcw_en_narrow.onnx` | `fc80c4baec72d756af1dbce9b70357a731b0b1a6b8306815bcfc61b6403c46e5` |

Original UUID names: `39578E22-…` (wide `public/models/en/`), `4C1FFB1B-…` (narrow `public/models/en_narrow/`).
