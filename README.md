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

## UE Migration Phase 0 Baseline

Reviewed against gameplay commit `0d4233e1bf1e6f54414d50c93e4a43418e425ac4`. This section supersedes the earlier audit's automatic-draw recommendation. No gameplay or UE implementation is included.

### Source of truth

1. Explicit user clarifications during current development.
2. Intentional current gameplay supported by code/tests.
3. The historical `五子棋.docx` design document.
4. Assumptions and inferred behavior.

A legacy event-loop accident is not automatically intentional gameplay. Record unresolved conflicts rather than silently changing behavior to match the old document.

### Frozen rules and compatibility baseline

- Start with zero cards. An ordinary legal placement gives **no automatic card**. A placement satisfying the existing successful-block predicate gives **exactly one card**. This intentional rebalance supersedes the old document.
- Preserve `engine/rules.py::has_double_end_block`: on the post-placement board, examine both signs of horizontal, vertical and both diagonal directions. An immediately adjacent opposite-color run of length at least one qualifies if its far end is outside the board or nonzero. Any qualifying run returns true; several runs still yield one reward. The predicate ignores barriers, forbidden flags and reward history. Its caller must validate placement; the helper does not validate the origin's occupancy/color. The current caller uses the actual placed color and rewards the acting player's hand.
- Keep 19x19, black first, and at least five connected same-color stones in the four line directions. Existing barrier checks interrupt winning lines. There are no implemented Renju forbidden-move rules.
- One completed player action is one turn: ordinary placement OR one basic card. Selection, preview, cancellation and invalid requests consume no card, reward or turn. Card effects, including Restock, never trigger the placement reward.
- Preserve the current ten-card catalog in `engine/cards.py`; uniform sampling with replacement, no finite deck or hand cap. Do not add advanced effects, Joker, Ctrl+Z, Speed Duel or unfinished concepts to this migration's playable pool.
- Preserve Restock's consume-one/draw-two order, Swap Hands' consume-before-swap order, and Steal's random opposing-hand entry selection. Steal against an empty hand currently consumes the card and action without a transfer.
- Preserve 2x2 Polarity (empty cells unchanged) and single-point Nuke (clear occupancy and retain a forbidden flag until reset, including an empty target). Valid no-op targets still consume a card/action.
- Preserve the tested barrier interruption and straight-line Tetris clearing algorithms as compatibility references; do not automatically substitute the historical 7x7 barrier or connected-component clearing.
- Preserve tested confusion interactions: Swap Hands and Tetris activation consume one confusion count; Ghost activation does not. The full lifetime policy still needs the decisions below. Existing ordinary placements and other immediate cards decrement it; deployment cards and Tetris block locks do not.
- Ghost has a five-second preparation and six hidden placements with card use/win checks disabled during the hidden stage, then restoration and win checking. Tetris alternates six blocks, three per player, with opposite-color blocks, no placement reward, and no normal card/placement input. Current Tetris starts with the caster's opponent and resumes normal play with that opponent.
- Ordinary placement currently checks victory before rewarding a successful block; a winning placement can still earn its one reward. Polarity and Ghost restoration scan for wins. Traversal-order tie breaking is not an accepted rule.

### Unresolved decisions and release gates

- Full board/no legal action and simultaneous black/white wins need explicit outcomes; do not infer a draw or choose the last scanned winner.
- Ghost currently rewrites occupied cells to value 3, suppressing blocking rewards as a side effect. Decide whether hidden placements are reward-eligible and whether any reward reveals information. Its preparation currently permits normal placements/wins: decide whether this is intended or input must freeze.
- Confusion resets to two remaining counts after casting; confirm deployment-card exceptions and mode-boundary counting rather than changing tested exceptions to a universal decrement.
- Confirm the intended barrier footprint/anchor: current logic blocks the six pairwise connections of the four corners of one cell, unlike the historical 7x7 cross. UI preview and rule geometry must agree.
- Back to Basics currently only prevents future card use; existing confusion, barriers and forbidden points remain. Confirm this policy before broadening it to remove effects or undo board changes.
- Tetris needs explicit handling for blocked spawn, forbidden points and barrier interaction. Current downward-blocked locking differs from historical any-direction contact; settle that conflict before porting the mode. Its current six-block exit has no explicit win check: confirm exit adjudication.
- Ctrl+Z response/rollback priority and Speed Duel timing are future decisions, not prerequisites for the current ten-card migration.

### Minimal architecture and next phase

Use one authoritative match state and one validate/resolve/commit action path. C++ owns rules, rewards, RNG and turn advancement; Blueprint/UI owns presentation and local selection only. Card handlers return results and never advance turns themselves. Keep board data authoritative, separate real color from visibility, and preserve mode behavior only after its gates are resolved. A small card definition table and explicit effect dispatch are sufficient; no replicated public-state copy or generic effects framework is needed for the local prototype.

Next is **UE migration Phase 1: presentation-independent rule core and regression coverage**, distinct from the historical refactoring phases above. It is not started or authorized for implementation by this documentation pass. Resolve baseline blockers before implementing the affected behavior. Begin with ordinary actions/rewards and simple card transactions; add UI and gated special modes in later phases. Keep the pygame implementation unchanged as a comparison reference.

Phase 1 acceptance must cover: ordinary placement without a block gives zero cards; successful blocking gives exactly one; illegal placement leaves board/hands/turn/RNG unchanged; card play gives no placement reward; failed/cancelled targeting leaves reward and turn unchanged; single/multiple runs, both colors, all eight rays, boundaries, open far ends and repeated eligible positions follow the existing predicate; fixed state/seed/actions give repeatable results without UI. Also cover winning-placement reward, consume-before-swap, one turn advancement, terminal input rejection and complete reset.

The three current blocking tests cover a single enclosed stone, an open far end and multiple qualifying runs returning true. They do **not** test actual card awards, legality, RNG preservation, event-loop integration or UI independence of the complete action path. Identify/add those regressions in Phase 1, not in this README-only pass.

Do not build GAS, ECS, effect graphs, event sourcing, networking/replication infrastructure, plugins, caches, generalized undo or unfinished cards for this phase.
