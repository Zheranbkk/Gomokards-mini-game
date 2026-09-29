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

## Unreal Migration Phase 0 Baseline

Reviewed on `ue-migration` at `6f912d6ad57f65da5e206143a57035e74140c9b4`. `Gomokards/` already contains the Unreal 5.8 C++ bootstrap; retain its structure. Phase 0 is documentation only; Phase 1 gameplay and tests have not started. The development machine has 32 GB RAM; architecture is driven by correctness and scope, not assumed memory limits. The pygame implementation remains a behavioral reference.

### Authority and frozen rules

Authority, highest first: (1) explicit current user clarifications; (2) intentional gameplay supported by code/tests; (3) historical `五子棋.docx`; (4) inference. Record unresolved intent separately from observed event-loop behavior.

- Both hands start empty. Ordinary legal placement does **not** automatically draw. A successful block earns **exactly one card**. This intentional balance change supersedes the historical one-card-per-placement rule.
- Keep 19x19, black first, and four-direction connected lines of at least five; existing barriers can interrupt winning lines. No implemented Renju restrictions.
- One completed placement or basic card play is one action/turn. A click is input; selection/preview/targeting is local interaction, not a completed action. Invalid/cancelled requests change no authoritative state, RNG or turn. A valid no-op effect can still consume a card/turn.
- Current catalog: the ten entries in `engine/cards.py`, sampled uniformly with replacement, without finite-deck exhaustion or a hand cap. Preserve the reference catalog; unresolved effects are migration gates, not silently redesigned cards. Do not add advanced effects or unfinished cards to a playable pool.

### Exact blocking predicate

Preserve `has_double_end_block` in `engine/rules.py`. On the post-placement board, for both signs of horizontal, vertical and the two diagonals, require an immediately adjacent opposite-color run of length at least one. The new stone closes its near end; its far end must be outside the board or nonzero. Any qualifying run returns true; several qualifying rays still award only one card. An open far end or a gap next to the placed stone fails that ray.

The predicate ignores barriers, forbidden flags and reward history; an empty forbidden point is still empty. It does not validate origin occupancy/color or turn ownership; the action caller must. The caller supplies actual placed color, including confusion, and rewards the acting player's hand. Repeated future eligible placements are not deduplicated. Ghost's value-3 board currently prevents finding opposite-color runs; that mode interaction is unresolved, not a predicate redesign. Tetris locks and card plays never call placement rewards.

### Resolution and card eligibility

The ordinary-action contract is validate before mutation, place using the effective color, evaluate the win, award one card iff blocking succeeds, progress applicable effects, complete once, then transfer control only if play continues. Preserve reward on a winning placement. Commit the result atomically to presentation; terminal state rejects subsequent gameplay. Legacy unconditional turn toggles/acted flags are not additional actions. Card handlers return effects/results; only the common resolver advances the action.

| Category | Basic effects and current semantics |
| --- | --- |
| Defined for early core migration | Restock consumes itself then draws two. Swap Hands consumes itself before exchanging remaining hands. Steal selects a random opposing-hand entry; an empty opposing hand still costs the card/action. Nuke clears one in-bounds point and marks it forbidden, even if empty/already forbidden. None triggers placement reward. |
| Implemented, clarification gates | Polarity flips a full in-bounds 2x2, preserving empty points; simultaneous wins need a ruling. Confusion, Barrier, Back to Basics, Ghost and Tetris require the lifetime/interaction decisions below. Keep known behavior as reference rather than restoring historical descriptions automatically. |
| Excluded new content | Advanced/double-card effects, Joker, Ctrl+Z, Fast Duel, and incomplete concepts (curling, Ultimate Chicken Horse, Sokoban, lava floor). Fast Duel's historical 10 turns/4 seconds specifies a goal, not complete entry, timing and interaction rules. |

### Lifetimes, modes and win order

| State/effect | Current progression and end |
| --- | --- |
| Confusion | Casting sets 3 then immediately decrements to 2, replacing the previous count. Ordinary/hidden placements and non-Ghost immediate cards decrement a positive count; deployment cards and Tetris locks do not. Tests explicitly preserve Swap/Tetris activation decrement and Ghost activation non-decrement. Confirm untested deployment exceptions; do not replace all events with a generic turn decrement. |
| Polarity, Nuke, Barrier | Polarity is an immediate board mutation, not a timed modifier. Nuke forbidden flags and Barrier centers persist until reset; no decay. Current Barrier blocks all six pairwise links among the four corners of one cell, not a confirmed historical 7x7 cross. |
| Back to Basics | Sets the shared card lock; no expiry before reset in ordinary play, and no clearing of confusion/barriers/forbidden points. Confirm intended scope. Permanent prohibition and temporary mode restrictions must not share a blindly cleared flag in UE. |
| Ghost | Activation consumes a card/action, switches player, locks cards and starts a five-second preparation. Preparation currently allows ordinary placements and wins. Hidden mode then lasts six legal placements, bars cards and suppresses immediate wins; each hidden placement switches player and decrements its counter. Restore colors, unlock and scan wins after the sixth. Exit performs no extra turn switch: next player follows the sixth placement; without extra preparation moves this is the caster's opponent. |
| Tetris | Activation consumes a card/action and locks normal clicks/cards. Opponent of caster operates first, alternating six blocks (three each) in opposite colors. Left/right/down and Space rotation are supported; automatic downward steps use a 500 ms interval. Only a block lock completes a mode sub-action, not movement/timer input. Clear straight five-plus lines after each lock. Sixth lock exits/unlocks; normal player remains the caster's opponent. No block-placement reward or confusion decrement on locks. |
| Reset | Clears board, hands, forbidden points, barriers, effects, timers/mode state, outcome and pending interactions; starts black with zero cards. |

Current win-check sites: ordinary placement checks before reward/confusion decrement; Polarity scans after flipping and before card consumption; hidden Ghost placements skip checks, then restoration scans after the sixth placement/turn switch. Nuke and Barrier deployment have no new win check. Tetris performs elimination after locks and no explicit normal-win check at exit. Ordinary wins belong to the connected stone color, not automatically the acting player. Scanning order currently overwrites simultaneous winners; this is not an accepted tie rule.

### Unresolved rules and gates

- Determine no-legal-action outcome (a full board can still permit cards) and simultaneous-color wins. Do not invent pass/draw/last-scanned-winner policies.
- Decide Ghost preparation input and hidden-placement reward eligibility, including information revealed by a reward. Keeping true colors separate from display must not silently change that policy.
- Confirm Confusion deployment exceptions, Barrier footprint/anchor, and whether Back to Basics only prevents future plays or also clears existing effects. Tested exceptions remain compatibility requirements unless explicitly changed.
- Tetris needs blocked-spawn handling, forbidden-point/barrier interactions, downward-blocked versus historical any-direction-contact locking, and exit win adjudication. Six-block order and tested straight-line clearing are existing references, not grounds to invent the missing cases.
- Fast Duel and Ctrl+Z decisions are future scope, not blockers for the currently implemented catalog.

### Architecture, next phases and acceptance

Use one authoritative C++ match state and one validate/resolve/commit path within the existing module. UI-independent rules receive explicit actions and controlled randomness/time; Blueprint/UMG only submits requests and presents results. Board Actors/Widgets own no gameplay truth. Local targeting never writes match state. A small definition table and explicit card dispatch suffice. Do not require a scene/GameMode to test rules; a later GameMode can own the runtime state without creating a second mutable copy.

Next agreed phase is **Phase 1: UI-independent Unreal rules core**, after resolving decisions affecting its selected scope and receiving implementation authorization. Start with ordinary placement/reward and defined simple card transactions; gated effects must not masquerade as completed behavior. Phase 2 adds minimal presentation and clarified basic effects; Phase 3 adds clarified Ghost/Tetris and cross-mode regression. These are migration phases, separate from the historical pygame refactoring labels. Preserve pygame for comparison and independent rollback; compare fixed random outcomes across runtimes rather than assuming equal seeds produce equal Python/UE streams.

Phase 1 acceptance:
- Empty starting hands; ordinary non-blocking placement gives zero cards; successful blocking gives exactly one, including multiple qualifying rays and a winning placement.
- Illegal placement/failed action changes no board, hands, effects, outcome, RNG or turn; cancelled targeting does likewise. Card play gives no placement reward.
- Cover both colors, all eight rays, single/multiple stones, boundaries, gaps/open ends, ignored barrier/forbidden metadata and repeated eligible positions using the exact predicate.
- One accepted action completes once; terminal state rejects gameplay; reset cleans all authoritative state. Validate long lines, both diagonals, actual-color winner and card consumption order.
- Results do not depend on Widgets/Actors, frame rate or animation callbacks. Fixed initial state, actions and supplied random outcomes reproduce results.
- Existing three block tests cover an enclosed singleton, open far end and multiple runs returning true; they do not test actual reward counts, invalid inputs or complete action/UI independence. Add these regressions in Phase 1, not this documentation pass.

Required now: clear ownership, validation, action semantics and explicit decision gates. Premature: service layers, duplicate state snapshots, generic lifetime/undo systems, caching and performance frameworks. Future only: GAS, ECS, effect graphs, event sourcing, plugin card systems, networking/replication, advanced cards and unfinished modes. None is justified by the current board/card set.

## Unreal Migration Phase 1 Rules Core

Implemented on `ue-migration` from Phase 0 commit `a321a3e`. This section updates migration status; the Phase 0 record above remains historical. The existing Unreal module, bootstrap, configuration and pygame prototype are unchanged. No Phase 2 presentation or integration is included.

### Architecture and behavior

- `Public/Core/MatchState.h` and `Private/Core/MatchState.cpp`: plain value state, a 361-cell board, player collection with stable player IDs/default stone identities, current-player index, completed-action count, result and owned `FRandomStream`. Coordinates use `Y * 19 + X`. Reset reconstructs the complete state using the supplied seed (default 0); callers choose a seed explicitly for varied matches.
- `Public/Core/MatchRules.h` and `Private/Core/MatchRules.cpp`: read-only validation, ordinary win/block predicates and the single `ResolveAction` entry point. Effective stone identity has a separate helper; player identity owns hands/actions. Current two-player opponent selection is localized. Only 19x19, five-in-a-row and two players are supported.
- `Public/Cards/CardDefinitions.h` and `Private/Cards/CardDefinitions.cpp`: stable nonlocalized IDs and a four-entry playable definition table. `Private/Cards/CardEffects.h/.cpp` implements explicit effect dispatch. The common resolver consumes the played card, completes the action and advances the turn; effects do none of those independently.
- Invalid requests leave every authoritative field and RNG unchanged. Resolution uses a small candidate-state copy and commits only success. Public value fields support fixtures; a live owner must route changes through `ResolveAction`/`Reset` and expose only const state to future presentation. There are no UObjects, Actors, Widgets, GameMode dependencies or callbacks in the rules core.
- Ordinary non-blocking placement draws nothing. The exact legacy eight-ray blocking predicate awards one card, including multi-ray and winning placements. Forbidden metadata is deliberately ignored by that predicate, but enforced by placement validation. Winning identity follows the actual stone, not the player ID. A terminal match rejects further gameplay.
- Restock consumes one then draws two; Swap consumes itself before exchanging hands; Steal transfers one random card or validly consumes its action against an empty hand; Nuke clears and forbids a point, including empty/already-forbidden targets. Card actions never receive placement rewards. Duplicate-card Steal preserves the prototype's removal of the first matching card value.
- Per the Phase 1 instruction, **only Restock, Swap Hands, Steal and Tactical Nuke are generated**, uniformly with replacement. This intentionally narrows Phase 0's ten-card reference pool. Gated/unknown IDs return `UnsupportedCard`, never consume a card as a placeholder.
- Selection/cancellation has no match-state representation: validate without committing, or discard the request. A successful nonterminal action transfers once; a winning action completes once without transferring.

### Validation

Validated on Windows with Unreal **5.8.2**, Development Editor, MSVC 14.44 and Windows SDK 10.0.22621.0. Build succeeded. **9 Automation Tests passed, 0 failed, 0 test warnings, 0 skipped**. The final run disabled startup-map loading and used NullRHI; tests create no worlds, Actors, GameModes, Widgets or PIE sessions.

`Private/Tests/MatchRulesTests.cpp` covers reset/board conversion; invalid-action full-state/RNG equality; both colors and all eight blocking rays; singleton/multi-stone/open/gap/edge cases; ignored forbidden metadata; multi-ray and repeated rewards; five/long/broken lines in all directions; stone/player identity separation; winning reward and terminal rejection; four card transactions; deterministic pool sampling; and the explicit no-legal-action decision boundary.

Reproduce from the repository root in PowerShell (set the installed engine path):

```powershell
$EngineRoot = 'G:\GameDev\Unreal\UE_5.8'
$ProjectFile = (Resolve-Path './Gomokards/Gomokards.uproject').Path
$ReportPath = Join-Path (Split-Path $ProjectFile) 'Saved/Automation/Phase1'
& "$EngineRoot/Engine/Build/BatchFiles/Build.bat" GomokardsEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $ProjectFile -Unattended -NullRHI -NoSound -NoSplash -NoP4 '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:LoadLevelAtStartup=None' '-ExecCmds=Automation RunTests Gomokards.Phase1' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$ReportPath"
```

Inspect the exported `index.json` test counts, not just process exit status. Generated build outputs, logs and reports stay in ignored directories and are not committed. Equal Python/Unreal seeds are not asserted to generate equal random sequences.

### Limits and next phase

- No-legal-action adjudication remains unresolved. After a completed non-winning action, if the next player has no legal placement or supported card, the core enters `AwaitingRuleDecision` with reason `NoLegalAction`, no winner, and rejects further actions until reset. This explicitly isolates the gap rather than inventing a draw/pass/loss.
- Starting from a clean match, the implemented actions cannot create simultaneous black/white wins: placement checks its new stone, terminal state stops play, and Nuke only removes stones. No simultaneous-win policy or general imported-state repair is implemented. Test fixtures are trusted value states, not a save-game import interface.
- Deliberately accepted constraints: fixed board/win constants, two current players, hands containing interchangeable card IDs rather than per-instance metadata, simple explicit dispatch, and copying a small state per action. These avoid a speculative framework while leaving player collections, stone identity resolution, card definitions and the action boundary available for later extension.
- Ghost, Tetris, Polarity, Confusion, Barrier, Back to Basics, Fast Duel, Undo/Joker, advanced effects, four-player rules, UI/Blueprint gameplay, board Actors, GameMode integration and networking remain unsupported. No additional modules/plugins/services were introduced.
- Next recommended phase: a minimal presentation adapter that sends requests and reads const state, after separate authorization. Resolve remaining Phase 0 decisions before adding affected cards/modes. Phase 2 has not started.
