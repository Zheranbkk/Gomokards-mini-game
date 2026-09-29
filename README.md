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

## Unreal Migration Phase 2 Local Playable Slice

Implementation continues from Phase 1 `e1a55e2d8bec6a4a486709f26f43b902d5eb415f` on `ue-migration`. **Phase 2 acceptance is complete:** agent-performed compilation, automation and implementation checks passed, and the user personally completed and passed hands-on gameplay validation. The Phase 0/1 sections above are retained as historical records.

### Impact check and ownership

The Phase 1 state, rules, card definitions/effects and all nine existing tests required **no changes**. Required integration work is limited to a runtime owner, local input/presentation, a startup map and three adapter/runtime tests. Later work may replace the development HUD with UMG and add localization. A generalized rules framework, reflected copy of the whole match, replication and new card mechanics are premature for this slice.

- `Source/Gomokards/Public/Runtime/LocalMatchGameMode.h` and its private implementation define `ALocalMatchGameMode`, the sole owner of one live `FMatchState`. It exposes a const query, submits to `ResolveAction`, returns rejection results unchanged, and broadcasts once on an accepted action or new match. There is no mutable Blueprint state API.
- Normal initialization and New Match derive a session seed from a new GUID. The core retains its owned deterministic `FRandomStream`. An explicit map URL option `?Seed=<integer>` is available for reproducible debugging; the normal Play/restart path does not silently reuse seed zero.
- `ALocalMatchPlayerController` creates/removes one local viewport view, shows the cursor and directs UI input to it. There is one controller for the two-player hot-seat match, with no controller-side match copy.
- `Public/Presentation/MatchPresentation.h` and its private implementation centralize pixel-to-integer conversion, result/rejection/card labels and small local target-selection intent. Pixel hit bounds are half-open; an outside click never clamps into a legal point.
- `Private/Presentation/SLocalMatchView.h/.cpp` defines the replaceable native Slate view and its board widget. Input becomes a placement/card request, goes through GameMode and the core, then the view reads the committed state. Rendered stones never determine occupancy. Selection, hover and feedback are presentation-only; hands and board are not mirrored in widget-owned arrays.
- `Content/Maps/LocalMatch.umap` is the only new content asset: an intentionally empty gameplay map. `Config/DefaultEngine.ini` selects it for editor startup and game startup, with `LocalMatchGameMode` as the default GameMode. The existing module adds only private `Slate` and `SlateCore` dependencies. No runtime plugin, Blueprint, material or legacy artwork is required.

### Launch and controls

Build the Development Editor target as in Phase 1, open `Gomokards/Gomokards.uproject`, and Play the default `LocalMatch` map (prefer a viewport or New Editor Window of at least 1100 x 800). No console command or state injection is needed to play. Both hands begin empty and cards are earned by successful blocks.

- Left-click a board point to place a stone. The active player, action count and action/rejection feedback are visible.
- Click an active-hand Restock, Swap Hands or Steal to play it immediately. Inactive-hand buttons are disabled. Card identity is always `ECardId`, never display text.
- Click Tactical Nuke to enter targeting, then click an occupied or empty board point to execute it. A yellow hover outline previews the coordinate; red squares with white X marks show forbidden points.
- Escape, right-click, or clicking Nuke again cancels targeting without submitting an action. Accepted changes and restart clear local selection.
- New Match / Restart reconstructs a clean match with a fresh seed and clears selection, hands, stones, forbidden points and result. Black starts again.
- A winner is displayed and gameplay is blocked after a win. `AwaitingRuleDecision` is explicitly labelled, assigns no invented win/draw, and permits restart.

The only playable pool remains Restock, Swap Hands, Steal and Tactical Nuke. Each accepted placement or card play completes one action. The existing empty-opponent Steal behavior, consume-before-swap order, exactly-one blocking reward and winning-block reward are preserved by the unchanged core.

### Validation and acceptance

Agent-performed validation: Development Editor build succeeded with Unreal **5.8.2**, MSVC 14.44 and Windows SDK 10.0.22621.0. Automation result: **12 passed, 0 failed, 0 test warnings, 0 skipped** (nine unchanged Phase 1 groups plus three Phase 2 groups). `Private/Tests/MatchPresentationTests.cpp` covers:

- `BoardCoordinates`: all 361 painted centers and edge/outside hit bounds.
- `TargetingIntent`: select/reselect/cancel without state or RNG mutation, complete targeted requests, invalid-target atomicity, stale-player rejection and explicit terminal labels.
- `RuntimeOwner`: a transient runtime world/owner, deterministic explicit seeding, acceptance versus rejection notifications, rejection full-state equality and clean fresh-seed restart.

Use the Phase 1 build/test commands with `Automation RunTests Gomokards` and report directory `Saved/Automation/Phase2` to run all 12. Read the exported `index.json` counts. The runtime-owner test creates a transient world; the Phase 1 tests remain UI/world-independent. Generated reports, binaries and caches are ignored.

Agent startup verification also confirmed that `/Game/Maps/LocalMatch` loaded with `LocalMatchGameMode`. The agent's earlier window-capture failure did not establish gameplay results and is no longer an acceptance blocker.

**User-performed manual gameplay validation: completed and passed.** The user personally tested the playable build and reported that all current Phase 2 gameplay logic works correctly. This hands-on result is user-reported, distinct from the agent's build, automated tests and static review. The requested manual validation gate is satisfied.

### Manual Gameplay Validation Checklist

The Phase 2 smoke pass below is complete per the user's report; retain this checklist for later regression passes in Unreal:

- Open `LocalMatch` and Play: an empty 19x19 board, empty hands and Black to act are visible. Place legal stones: the correct colors render and the turn changes once. Click an occupied point: feedback appears without spending an action.
- Close a known opposing run at both ends: exactly one card is added to the acting hand. Ordinary non-blocking placement adds none.
- Play Restock: consume it and draw two. Play Swap Hands: consume it before exchanging the remaining hands. Play Steal: transfer one opposing card; against an empty hand, still consume the card and action.
- Select Nuke and cancel using Escape, right-click or reselect: the hand, board and turn stay unchanged. Execute Nuke on occupied and empty points: each target becomes an empty, visibly forbidden point. Normal placement there is rejected without spending an action.
- Complete five in a row: show the correct winner and block further gameplay. Where practical, also close an opposing run with the winning placement and observe its one-card reward.
- Restart both from a result and with targeting pending: clear stones, forbidden points, hands, result and targeting, and start with Black again.

For future gameplay phases, the agent inspects C++ and Blueprint/Slate/UMG logic, checks state ownership and asset/config references, compiles, runs relevant Automation Tests and reports a concise manual checklist. The user performs hands-on gameplay validation; the agent does not play scenarios unless explicitly requested. Final commits require the user's pass report unless that gate is explicitly waived. Screenshots, when needed for editor inspection, do not substitute for gameplay validation.

### Deliberate limitations and next step

This is a fixed-size development HUD using engine-native lines, rounded stone shapes, colors and English text. Both hands are visible for testing, which establishes no permanent hand-visibility rule. It has no production responsive layout, art, audio, animation, gamepad/touch interface, saved games or packaged-build validation. Slate keeps the plain core independent of reflection; a future UMG view should add only the query/intent bridge it actually needs.

Preserved extension seams are the player collection (the view iterates it), player versus stone identity, stable card IDs/definitions, centralized coordinate conversion, local target intent and authoritative resolve/refresh boundary. No four-player rules, dynamic board, generic target framework, advanced effects, additional cards, special modes or networking were added. The Phase 0 unresolved rules remain unresolved, including no-legal-action adjudication and the gated card/mode interactions.

Phase 2 is accepted. Future work should prioritize playtesting feedback and rule clarification before migrating further cards or modes; no Phase 3 implementation is included.

## Unreal Migration Phase 3A Standard Cards

Implementation starts from completed Phase 2 `eb9437f92f05d8316b26b561a37411386ba2cdb4` on `ue-migration`. These explicit Phase 3A decisions supersede conflicting historical Phase 0–2 rules above. Hands-on gameplay validation for Phase 3A is **pending the user**. For this phase, the user explicitly authorizes an implementation commit/push after successful agent compile, automation and static checks, before their hands-on pass.

### Impact and state ownership

The same `ALocalMatchGameMode` owns one live value state and routes intentions to `ResolveAction`; no new runtime owner, UObject effect system, module, Blueprint, map or binary asset is required. `MatchState` gains `ConfusionActionsRemaining`, `bCardsDisabled` and `FBoard::Barriers` (unique cell anchors). Equality and reset include this metadata. `EMatchStatus::Draw` is a terminal result with no winning stone. Widgets still store only local target/hover intent and read committed state.

The existing definition table now uses a small `ECardTarget` enum for none, intersection, top-left region anchor or cell-center targeting. Exactly eight cards are generated uniformly with replacement: Restock, Swap Hands, Steal, Tactical Nuke, Polarity, Confusion, Barrier and Back to Basics. No unsupported IDs are generated.

### Rules and resolution

- **Polarity:** select the top-left intersection of a complete 2x2 region, matching pygame's anchor semantics (anchor coordinates 0–17 on each axis). Swap Black/White in those four cells, preserve empty cells and metadata, then evaluate the whole board. Black only or White only wins; both colors win simultaneously means Draw; neither continues. Scan order never selects the winner. This consumes one card/action without a placement reward.
- **Confusion:** casting establishes exactly two future successful actions. An affected placement uses the opposite of its player's assigned stone; player IDs and hand ownership do not change. Each successful placement/card action consumes one old duration. Invalid, rejected or cancelled interactions consume none. A recast consumes the old action then explicitly replaces the duration with two future actions. There is no set-to-three/decrement workaround. Winning color follows the actual stone; blocking rewards still belong to the acting player, including winning placements.
- **Barrier:** anchors are the 18x18 board cells. The four intersections A=(x,y), B=(x+1,y), C=(x,y+1), D=(x+1,y+1) share one `FBoard::RegionCorners` definition. A Barrier blocks exactly the six unordered local links AB, AC, AD, BC, BD, CD; all other links remain unchanged. It removes no stones and persists until reset or Basics. Duplicate deployment consumes a valid card/action but stores only one anchor. Win detection checks each traversed link. The legacy blocking-reward predicate still deliberately ignores barriers.
- **Back to Basics:** consume the card, permanently disable card play until restart, clear Confusion, every Barrier and all Nuke forbidden flags. Keep every current stone/color and remaining hand entry: past Nuke removal, Polarity flips, draws, swaps and transfers are not undone. Removing barriers can reconnect winning lines, so Basics uses the same global winner/Draw evaluation as Polarity. If play continues, transfer exactly once. Disabled card requests return `CardsDisabled`; disabled cards do not count as available actions on a full board. Existing blocking rewards are retained, but any resulting cards are inert; no new reward policy was invented.
- **Terminal behavior:** Won and Draw reject later gameplay and do not transfer control after the terminal action. New Match clears results/effects and restores card availability. A full board with no legal action remains `AwaitingRuleDecision`, not an invented draw. No sudden-death duel, post-draw play or timers are implemented.

### Presentation and shared geometry

The existing Slate HUD adds four card labels/buttons, Confusion duration/effective placement color, a card-disabled notice and explicit Draw text. Inert cards remain visible with disabled buttons. Targeting is local, and Escape/right-click/reselect cancels without a resolver call.

`FBoardLayout::TargetAt` centralizes hit conversion: normal placement/Nuke use intersection hit areas; Polarity uses a valid top-left intersection; Barrier uses the square between its four surrounding intersections. Barrier preview and committed rendering use the same small cyan/yellow cross endpoints derived from the core's corner definition. Polarity previews its four-point region. The placement hover shows the effective stone color and consults core validation. Neither visual geometry nor occupancy becomes game truth. Invalid edge targets reject without consuming the selection, card, action, duration or RNG.

### Agent validation

Agent-performed validation completed on Unreal **5.8.2**, Win64 Development Editor, MSVC 14.44 and Windows SDK 10.0.22621.0. **Build succeeded. All 21 Automation Test groups passed: 9 Phase 1, 3 Phase 2, 9 Phase 3A; 0 failures, 0 test warnings, 0 skipped.** The exported report was inspected, not inferred from process exit. Static review confirmed one authoritative state, the resolver mutation path, shared targeting geometry, disabled/terminal input guards and existing asset/config references. No agent-driven hands-on gameplay was performed; that validation remains pending the user.

The nine Phase 1 and three Phase 2 test groups are retained. Only two obsolete Phase 1 expectations change: the pool is now eight cards, and the four newly implemented IDs are removed from the unsupported-ID list. All other prior assertions remain. New `Private/Tests/StandardCardsTests.cpp` covers Polarity single/no/simultaneous results and scan-order independence; Draw rejection/reset; Confusion duration, refresh, actual-color wins and acting-hand rewards; all 324 Barrier anchors and six symmetric links; four-direction connectivity; Basics cleanup, restored-connectivity wins and disabled-hand no-legal-action handling; exact pool/determinism; target domains/cancellation; and reachable manual setups replayed solely via normal actions. The replay tests are automated rules checks, not human gameplay validation.

Reproduce with the Phase 1 build command, then `Automation RunTests Gomokards` and report directory `Saved/Automation/Phase3A`; inspect exported `index.json`. Existing `LocalMatch` map and configured GameMode references remain unchanged. Build outputs, logs, test reports and caches remain ignored.

### Manual Gameplay Validation Checklist — pending user

Pull `ue-migration` into your main checkout, close its editor before rebuilding, compile Development Editor, then open `LocalMatch`. Use at least 1100x800 for the development HUD. Coordinates below are zero-based intersections from top left. A Barrier anchor `(x,y)` means click the center between `(x,y)` and `(x+1,y+1)`.

For reproducible card acquisition, optionally use the existing explicit session seed instead of repeatedly drawing: using `$EngineRoot` and `$ProjectFile` from the build example, launch `& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" $ProjectFile '/Game/Maps/LocalMatch?Seed=7' -game -windowed -ResX=1100 -ResY=800`. Relaunch with a different seed for another scenario; New Match intentionally generates a fresh seed. This changes only the initial RNG seed and never injects board state or hands.

1. **Basic regression / acquire a card:** in a fresh match click `(0,0)`, `(1,0)`, `(2,0)`, `(18,18)` in order. Black earns exactly one card on action 3; ordinary moves give none; Black acts next at action count 4. Seed 7 earns Polarity, seed 10 Confusion, seed 12 Barrier, seed 15 Basics. Clicking an occupied point must leave action count/turn/hand unchanged.
2. **Polarity targeting and ordinary inversion:** with seed 7's opening, select Polarity; preview must cover four points from the selected top-left intersection. Try a last-row/column anchor and cancel: no authoritative change. Reselect and click `(0,0)`: only `(0,0)` becomes White and `(1,0)` becomes Black; empty lower corners stay empty; card disappears and White acts once. Nuke remains intersection-targeted.
3. **Simultaneous win / Draw:** relaunch seed 7, do the four opening moves, then click `(0,2)`, `(0,3)`, `(1,2)`, `(1,3)`, `(2,2)`, `(2,3)`, `(3,3)`, `(3,2)`, `(4,3)`, `(4,2)`. Black plays Polarity at `(3,2)`. Both five-stone rows must form, Draw must show at completed action 15, neither color is named winner, and subsequent placements/card plays must be blocked. Restart must clear everything and restore Black/empty hands/cards enabled.
4. **Confusion lifetime and rejection:** use seed 10's opening, then Black plays Confusion. It must show 2 remaining actions and White to act. An occupied click leaves 2. White places `(5,5)`: Black stone, 1 remaining. Black places `(7,7)`: White stone, effect ends. White places `(9,9)`: White stone normally. Separately, while active, play any available card instead of placing: exactly one old action expires. Selecting/cancelling a target consumes none. For a reproducible recast setup, relaunch seed 10 and click `(0,0)`, `(1,0)`, `(2,0)`, `(4,4)`, `(5,4)`, `(6,4)`; both players earn Confusion. Black casts it, then White recasts: the visible count must refresh to 2 after the second card action rather than stack or immediately reduce it. The following two successful actions exhaust it.
5. **Barrier targeting/connectivity:** use seed 12's opening, select Barrier, hover the cell anchored `(6,8)` and cancel once. The preview must be centered between intersections; no stone/card/action changes. Deploy there: cyan small cross appears, no stones removed, White acts. Fill a Black horizontal run `(5,8)` through `(9,8)`, interleaving White moves at spaced distant points; the Barrier must prevent a win across `(6,8)`–`(7,8)`. Other unblocked runs still win. Also deploy at boundary cell `(17,17)` when a Barrier is available; outside-cell targeting rejects without spending an action.
6. **Basics cleanup:** in an ongoing match retain Basics while first using Nuke on an occupied point, Polarity on a populated region and Barrier on a non-winning region. Cast Confusion, then have the next player play Basics. All crosses/red forbidden marks and Confusion must disappear; removed stones stay absent, flipped colors stay flipped, other hands remain visible but disabled. The next player can place normally, including on a formerly forbidden point. Cards cannot be played. If removing a Barrier reconnects five, immediately show the corresponding winner (or Draw for both), instead of transferring. Restart clears the lock and permits cards again.

### Accepted limits and remaining scope

The fixed-size English development HUD and both-hands-visible policy remain temporary. Eight-card hands may require scrolling. There is no art/polish, production responsive layout, packaging validation, replay/save-game importer or in-game fixture editor. Small value-state copying, a plain anchor array, explicit effect dispatch and full-board scans after connectivity mutations are intentional simple choices for a 19x19 board. No generalized effect framework or arbitrary topology was added.

Ghost, Tetris, Ctrl+Z/Undo, Fast Duel, Joker, advanced/double-card effects, networking, four-player rules and sudden-death draws remain unsupported. Phase 3B has not started. The user's manual Phase 3A pass remains pending after the implementation push.
