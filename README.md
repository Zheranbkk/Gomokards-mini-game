# Gomokards Card Gomoku

## Refactoring Progress

### Phase 1: Structural Split Without Gameplay Changes (Completed)

- Extracted the rules layer to `engine/rules.py`
- Extracted card metadata to `engine/cards.py`
- Added rule unit tests

### Phase 2A: State Consolidation (Initial Version Completed)

- Added `engine/state.py`
- Updated the main script to manage runtime state uniformly through `state.xxx`
- Changed `reset_game()` to delegate to `state.reset_match()`

### Phase 2B: Card Executor Registry (Initial Version Completed)

- Added `engine/card_effects.py`
- Changed instant-card logic in `handle_card_click()` to dispatch through executors
- Preserved the “select a point to cast” flow for deployment cards
- Added `tests/test_card_effects.py`

### Phase 2C: Cross-Rule Regression Test Matrix (First Batch Completed)

- Added `tests/test_cross_rules.py`
- Covered key cross-rule scenarios such as confusion, ghost, swap sides, barrier, and elimination

### Phase 2D: Fallback Rendering for Missing Assets (Initial Version Completed This Time)

Goal of this phase: keep the game runnable even when asset files are missing.

#### Completed

- Changed asset loading in `测试.py` to “optional loading”:
  - Missing files or load failures no longer crash the game.
- Added basic graphical fallback rendering:
  - Normal black/white stones, ghost stones, forbidden placement points, and Tetris blocks all support fallback drawing with `pygame.draw`.
- Added a runtime notification:
  - When missing assets are detected, a message appears at the bottom of the UI: “Basic graphics rendering mode enabled.”

### Phase 2E: Initial Bilingual UI Version (Completed This Time)

- Added a language selection dialog on first launch (English / Simplified Chinese).
- UI fixed text, card names, card descriptions, and key battle-log messages now support switching between English and Chinese.
- Added `engine/i18n.py` to centrally maintain the translation table and reduce hard-coded text.

## Suggested Later Phases (2E+)

1. Further separate input handling, rule resolution, and rendering
2. Add more fine-grained regression tests for the stone-placement flow and event loop

## Asset Loading Strategy

The current implementation supports two modes:

- Full asset mode: uses local image/font resources
- Fallback rendering mode: automatically draws basic graphics when assets are missing (runnable with simplified visuals)
- Pending with gomoku only mode

### Font Strategy (Preventing Garbled Chinese Text)

- Font loading uses a three-level fallback:
  1. Bundled font files inside the project (repository resource directory)
  2. A candidate chain of common system Chinese fonts (macOS / Windows / Linux)
  3. A final generic fallback font
- When publishing, it is recommended to bundle at least one commercially usable Chinese font (such as Source Han Sans / Noto Sans CJK), which can significantly reduce “tofu character” issues.
