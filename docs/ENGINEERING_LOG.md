# Engineering log

Chronological, indexed record of bugs and improvements.  
Newest entries at the **top** of the Entries section. Keep the Index table updated.

Language: English (see [ARCHITECTURE.md](ARCHITECTURE.md) language convention).

---

## Index

| ID | Date | Type | Title | Status |
|---|---|---|---|---|
| ENG-004 | 2026-11-08 | Improvement | Stop TX via Esc; button label Stop (Esc) | Resolved |
| ENG-003 | 2026-11-08 | Improvement | Always clean-rebuild; delete stale binaries/AppImages | Resolved |
| ENG-002 | 2026-11-08 | Improvement | TX progress: color already-transmitted text | Resolved |
| ENG-001 | 2026-11-08 | Improvement | English-only end-user UI strings | Resolved |

**Type:** `Bug` | `Improvement` | `Regression`  
**Status:** `Open` | `In progress` | `Resolved` | `Won't fix` | `Deferred`

---

## Entries

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
