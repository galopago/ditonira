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

_None._

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
