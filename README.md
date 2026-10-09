# Gomokards Card Gomoku

> **当前活动产品线：单人 Roguelike（SP0.5 架构基线，尚无单人对局）。**
> 活动分支为 `singleplayer-roguelike`。多人 Demo v0.1.2 保存在 `multiplayer-v0.1.2` 与 `demo-v0.1.2-multiplayer`，源提交 `b975b6310128a4113d5a118acfc5f1d280586623`。
> 下方 pygame、UE Phase 0–6 与 Demo 操作说明属于历史记录；其中多人启动、十张牌池及旧随机奖励不适用于当前活动 Game target。SP0.5 替代 SP0 中“保留多人运行时并行编译”的工程建议，产品规则以文末冻结规则为准。


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

Implementation starts from completed Phase 2 `eb9437f92f05d8316b26b561a37411386ba2cdb4` on `ue-migration`. These explicit Phase 3A decisions supersede conflicting historical Phase 0–2 rules above. The user has completed hands-on Phase 3A testing and accepted Polarity inversion/Draw, Confusion color/action lifetime, Barrier targeting/connectivity and Basics card locking. The two Phase 3A.1 corrections below have a separate pending manual check. For this phase, the user explicitly authorizes an implementation commit/push after successful agent compile, automation and static checks, before their hands-on pass.

### Impact and state ownership

The same `ALocalMatchGameMode` owns one live value state and routes intentions to `ResolveAction`; no new runtime owner, UObject effect system, module, Blueprint, map or binary asset is required. `MatchState` gains `ConfusionActionsRemaining`, `bCardsDisabled` and `FBoard::Barriers` (unique cell anchors). Equality and reset include this metadata. `EMatchStatus::Draw` is a terminal result with no winning stone. Widgets still store only local target/hover intent and read committed state.

The existing definition table now uses a small `ECardTarget` enum for none, intersection, top-left region anchor or cell-center targeting. Exactly eight cards are generated uniformly with replacement: Restock, Swap Hands, Steal, Tactical Nuke, Polarity, Confusion, Barrier and Back to Basics. No unsupported IDs are generated.

### Rules and resolution

- **Polarity:** select the top-left intersection of a complete 2x2 region, matching pygame's anchor semantics (anchor coordinates 0–17 on each axis). Swap Black/White in those four cells, preserve empty cells and metadata, then evaluate the whole board. Black only or White only wins; both colors win simultaneously means Draw; neither continues. Scan order never selects the winner. This consumes one card/action without a placement reward.
- **Confusion:** casting establishes exactly two future successful actions. An affected placement uses the opposite of its player's assigned stone; player IDs and hand ownership do not change. Each successful placement/card action consumes one old duration. Invalid, rejected or cancelled interactions consume none. A recast consumes the old action then explicitly replaces the duration with two future actions. There is no set-to-three/decrement workaround. Winning color follows the actual stone; blocking rewards still belong to the acting player, including winning placements.
- **Barrier:** anchors are the 18x18 board cells. The four intersections A=(x,y), B=(x+1,y), C=(x,y+1), D=(x+1,y+1) share one `FBoard::RegionCorners` definition. A Barrier blocks exactly the six unordered local links AB, AC, AD, BC, BD, CD; all other links remain unchanged. It removes no stones and persists until restart; Basics does not remove it. Duplicate deployment consumes a valid card/action but stores only one anchor. Win detection checks each traversed link. The legacy blocking-reward predicate still deliberately ignores barriers.
- **Back to Basics (corrected in Phase 3A.1):** only disable future card play until restart. Consume the played card and complete/transfer once normally; preserve the board, forbidden flags, Barriers, other hand entries and existing effects. Playing Basics while Confusion is active consumes one action through the normal successful-action lifecycle, but does not otherwise clear or rewrite the effect (2 becomes 1, then the next successful placement remains confused and expires it). No special global win reevaluation occurs: Polarity still performs its global scan and placement retains its normal win check. Disabled card requests return `CardsDisabled`; disabled cards do not count as available actions on a full board. Future blocking rewards still add cards, which remain visible but inert.
- **Terminal behavior:** Won and Draw reject later gameplay and do not transfer control after the terminal action. New Match clears results/effects and restores card availability. A full board with no legal action remains `AwaitingRuleDecision`, not an invented draw. No sudden-death duel, post-draw play or timers are implemented.

### Presentation and shared geometry

The existing Slate HUD adds four card labels/buttons, Confusion duration/effective placement color, a card-disabled notice and explicit Draw text. Inert cards remain visible with disabled buttons. Targeting is local, and Escape/right-click/reselect cancels without a resolver call.

`FBoardLayout::TargetAt` centralizes hit conversion: normal placement/Nuke use intersection hit areas; Polarity uses a valid top-left intersection; Barrier uses the square between its four surrounding intersections. Barrier preview and committed rendering use the same small cyan/yellow cross endpoints derived from the core's corner definition. Polarity previews its four-point region. The placement hover shows the effective stone color and consults core validation. Neither visual geometry nor occupancy becomes game truth. Invalid edge targets reject without consuming the selection, card, action, duration or RNG.

### Agent validation

Agent-performed validation completed on Unreal **5.8.2**, Win64 Development Editor, MSVC 14.44 and Windows SDK 10.0.22621.0. **Build succeeded. All 21 Automation Test groups passed: 9 Phase 1, 3 Phase 2, 9 Phase 3A; 0 failures, 0 test warnings, 0 skipped.** The exported report was inspected, not inferred from process exit. Static review confirmed one authoritative state, the resolver mutation path, shared targeting geometry, disabled/terminal input guards and existing asset/config references. No agent-driven hands-on gameplay was performed. The user subsequently reported the accepted Phase 3A checks listed above; the Phase 3A.1 correction check remains pending.

The nine Phase 1 and three Phase 2 test groups are retained. Only two obsolete Phase 1 expectations change: the pool is now eight cards, and the four newly implemented IDs are removed from the unsupported-ID list. All other prior assertions remain. New `Private/Tests/StandardCardsTests.cpp` covers Polarity single/no/simultaneous results and scan-order independence; Draw rejection/reset; Confusion duration, refresh, actual-color wins and acting-hand rewards; all 324 Barrier anchors and six symmetric links; four-direction connectivity; Basics state/effect preservation, normal Confusion progression, no special global scan, inert rewards and disabled-hand no-legal-action handling; exact pool/determinism; target domains/cancellation; and reachable manual setups replayed solely via normal actions. The replay tests are automated rules checks, not human gameplay validation.

Reproduce with the Phase 1 build command, then `Automation RunTests Gomokards` and report directory `Saved/Automation/Phase3A`; inspect exported `index.json`. Existing `LocalMatch` map and configured GameMode references remain unchanged. Build outputs, logs, test reports and caches remain ignored.

### Phase 3A manual reference — user testing completed

Pull `ue-migration` into your main checkout, close its editor before rebuilding, compile Development Editor, then open `LocalMatch`. Use at least 1100x800 for the development HUD. Coordinates below are zero-based intersections from top left. A Barrier anchor `(x,y)` means click the center between `(x,y)` and `(x+1,y+1)`.

For reproducible card acquisition, optionally use the existing explicit session seed instead of repeatedly drawing: using `$EngineRoot` and `$ProjectFile` from the build example, launch `& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" $ProjectFile '/Game/Maps/LocalMatch?Seed=7' -game -windowed -ResX=1100 -ResY=800`. Relaunch with a different seed for another scenario; New Match intentionally generates a fresh seed. This changes only the initial RNG seed and never injects board state or hands.

1. **Basic regression / acquire a card:** in a fresh match click `(0,0)`, `(1,0)`, `(2,0)`, `(18,18)` in order. Black earns exactly one card on action 3; ordinary moves give none; Black acts next at action count 4. Seed 7 earns Polarity, seed 10 Confusion, seed 12 Barrier, seed 15 Basics. Clicking an occupied point must leave action count/turn/hand unchanged.
2. **Polarity targeting and ordinary inversion:** with seed 7's opening, select Polarity; preview must cover four points from the selected top-left intersection. Try a last-row/column anchor and cancel: no authoritative change. Reselect and click `(0,0)`: only `(0,0)` becomes White and `(1,0)` becomes Black; empty lower corners stay empty; card disappears and White acts once. Nuke remains intersection-targeted.
3. **Simultaneous win / Draw:** relaunch seed 7, do the four opening moves, then click `(0,2)`, `(0,3)`, `(1,2)`, `(1,3)`, `(2,2)`, `(2,3)`, `(3,3)`, `(3,2)`, `(4,3)`, `(4,2)`. Black plays Polarity at `(3,2)`. Both five-stone rows must form, Draw must show at completed action 15, neither color is named winner, and subsequent placements/card plays must be blocked. Restart must clear everything and restore Black/empty hands/cards enabled.
4. **Confusion lifetime and rejection:** use seed 10's opening, then Black plays Confusion. It must show 2 remaining actions and White to act. An occupied click leaves 2. White places `(5,5)`: Black stone, 1 remaining. Black places `(7,7)`: White stone, effect ends. White places `(9,9)`: White stone normally. Separately, while active, play any available card instead of placing: exactly one old action expires. Selecting/cancelling a target consumes none. For a reproducible recast setup, relaunch seed 10 and click `(0,0)`, `(1,0)`, `(2,0)`, `(4,4)`, `(5,4)`, `(6,4)`; both players earn Confusion. Black casts it, then White recasts: the visible count must refresh to 2 after the second card action rather than stack or immediately reduce it. The following two successful actions exhaust it.
5. **Barrier targeting/connectivity:** use seed 12's opening, select Barrier, hover the cell anchored `(6,8)` and cancel once. The preview must be centered between intersections; no stone/card/action changes. Deploy there: cyan small cross appears, no stones removed, White acts. Fill a Black horizontal run `(5,8)` through `(9,8)`, interleaving White moves at spaced distant points; the Barrier must prevent a win across `(6,8)`–`(7,8)`. Other unblocked runs still win. Also deploy at boundary cell `(17,17)` when a Barrier is available; outside-cell targeting rejects without spending an action.
6. **Basics state preservation (Phase 3A.1 correction):** with an existing Barrier, Nuke forbidden point and active Confusion, play Basics. The Barrier and forbidden point remain; all other hand entries remain visible but disabled. Confusion progresses by the normal one-action step and any remaining action still affects the next placement. Previously removed stones stay absent and flipped colors stay flipped. Basics itself does not trigger a global win scan. Restart clears the normal match state and card lock.

### Accepted limits and remaining scope

The fixed-size English development HUD and both-hands-visible policy remain temporary. Eight-card hands may require scrolling. There is no art/polish, production responsive layout, packaging validation, replay/save-game importer or in-game fixture editor. Small value-state copying, a plain anchor array, explicit effect dispatch and full-board scans after connectivity mutations are intentional simple choices for a 19x19 board. No generalized effect framework or arbitrary topology was added.

Ghost, Tetris, Ctrl+Z/Undo, Fast Duel, Joker, advanced/double-card effects, networking, four-player rules and sudden-death draws remain unsupported. Phase 3B has not started. The user's accepted Phase 3A checks are recorded above; only the short Phase 3A.1 correction checklist below is pending.


## Unreal Migration Phase 3A.1 Corrections

**Historical rule record:** Phase 4B.3 below supersedes only the Basics/Confusion preservation rule: Basics now clears active Confusion while retaining all board history. Earlier tests and manual steps describing Basics taking Confusion from 2 to 1 are historical, not the current acceptance criteria.

Continues from `54bec6478de6082c464efc8f565e8225071ed10b`. No cards, modes, assets or topology were added. The Phase 3A rules above are corrected to reflect the current authoritative Basics behavior; unrelated Phase 0–2 history is retained.

- Barrier preview and committed rendering both call the existing `FBoardLayout::BarrierCross`. Each arm now extends **12.5% of one cell spacing** beyond its nearest grid line. This is presentation only: `RegionCorners`, `IsLinkBlocked`, the six blocked links, 18x18 anchor domain and hit conversion are unchanged.
- Basics now only sets the card-disabled flag. Removed its board-metadata cleanup, Confusion-clearing exception and global winner/Draw scan. Normal action completion still decrements existing Confusion. The HUD now displays both card lock and remaining Confusion together.
- Focused tests update only superseded Basics expectations and add preservation, continuing confused placement, inert future reward, persistent Barrier win blocking, absence of a Basics-triggered winner/Draw scan and proportional visual-overhang checks. Existing unrelated coverage is retained.

Agent validation: Unreal 5.8.2 Win64 Development Editor build succeeded. The complete `Gomokards` suite exported **21 passed, 0 failed, 0 test warnings, 0 skipped** (9 Phase 1, 3 Phase 2, 9 Phase 3A groups), verified in `Saved/Automation/Phase3A1/index.json`. Static review confirms preview/render share the extended endpoints and gameplay topology/hit conversion are unchanged. Hands-on correction validation is **pending the user**, and does not block this implementation commit. No agent gameplay or screenshot automation was performed.

### Manual Gameplay Validation Checklist — Phase 3A.1 only

A. Select Barrier and inspect its preview, then deploy it. All four arms should extend slightly across the nearest grid lines; preview and committed shape should agree. Targeting and connectivity must behave as before.

B. With an existing Barrier, a Nuke forbidden point and Confusion at 2, play Basics. Expect cards disabled and other cards retained visibly; Barrier/forbidden point unchanged; Confusion 2 -> 1. A rejected card/forbidden-point request must not reduce it. The next valid placement still uses the opposite color and takes Confusion to 0. Normal placement remains available, the forbidden point stays forbidden and the Barrier still interrupts connectivity. No full replay of already accepted Phase 2, Polarity or Draw scenarios is requested.

## Unreal Migration Phase 3B Ghost

Continues on `ue-migration` from Phase 3A.1 `cb8d1290a19189a9581ae849ab42779cf736cab0`. **The user has accepted the Phase 3A.1 baseline**, including corrected Basics behavior and Barrier geometry/topology. Earlier sections retain their historical validation status; this section records the current baseline and supersedes their obsolete pool/unsupported-card statements. Phase 3B implements **Ghost only**. The implementation commit/push was authorized after agent checks and before the hands-on pass. The user has subsequently completed and passed manual Ghost gameplay validation.

### Impact, state and timer ownership

One `ALocalMatchGameMode` still owns one authoritative `FMatchState`. The only new rules state is `EGhostPhase { None, Preparation, Hidden }` and `GhostPlacementsCompleted`, with a fixed limit of six. Equality/reset include both. The board still stores only Empty, Black and White: no gray identity, color snapshot or restoration pass exists.

Ghost is a non-targeted card. `ResolveAction` consumes it, progresses any existing Confusion once, completes one action and transfers normally into Preparation. **The 5-second preparation period freezes gameplay.** Authoritative validation rejects all gameplay requests without state/RNG changes; Slate also disables board and card input. The next player remains visible and retains the turn. Stones retain true colors throughout Preparation.

GameMode owns a core `FTSTicker` callback and a monotonic `FPlatformTime::Seconds()` deadline five real seconds after casting, independent of world time dilation/pause. The first runtime tick at/after the deadline calls the explicit, deterministic `BeginGhostHidden` transition and broadcasts once. That transition changes only Ghost phase/count; it never spends an action, changes player, advances Confusion or draws cards. The display countdown is derived from the runtime deadline, never authoritative rules state. Weak callback binding, cancellation on restart/EndPlay/destruction and a generation token prevent old callbacks from affecting a new match or a newer Ghost cast. Automation invokes the deadline check directly without sleeping. No generic scheduler or phase framework was added.

### Hidden placement and reveal rules

- Hidden permits only legal placements. All card play/targeting is temporarily prohibited, independently of the permanent Basics lock. Occupied, forbidden and out-of-bounds requests preserve the entire state, including counters and RNG.
- Each placement stores its true effective color immediately. Confusion uses the existing lifecycle: remaining 2 -> Ghost cast leaves 1 -> Preparation leaves 1 -> first Hidden placement uses the opposite color and leaves 0. Ghost does not clear or refresh it.
- **Hidden Ghost placements retain normal successful-block card rewards; visible reward information is intentional.** The unchanged blocking predicate sees true colors, still ignores Barrier metadata, and grants exactly one card to the acting hand even for multiple qualifying rays or a winning placement. Earned cards stay visible but cannot be played until Ghost ends.
- Each successful placement advances the Hidden count once. The first five suppress immediate winning termination, preserving any completed lines. The sixth resolves placement, reward and Confusion first, then clears Ghost and performs the existing full-board evaluation using true colors and Barrier connectivity: Black only wins, White only wins, both means Draw, neither continues. The action completes once; only a continuing match transfers normally, with no additional reveal turn switch.
- Existing Barriers, Nuke forbidden/removed points, committed Polarity colors and hands persist. Reveal clears only Ghost's temporary restriction; it cannot clear an independent Basics lock. Restart uses the existing complete state reset, cancels the runtime timer, restores visibility and starts Black with empty hands.

The prior core changes are limited to Ghost state/equality, validation, card availability, the explicit timer transition and delayed reveal adjudication. The accepted blocking predicate, Barrier topology, Basics effect and Polarity evaluation remain unchanged. Preparation is exempt from the ordinary post-action no-legal-action check because it awaits a runtime transition rather than player input.

### Presentation and playable pool

The existing Slate HUD shows the Preparation countdown and Hidden placements remaining. A shared `StoneDisplayColor` maps every occupied Hidden stone and placement preview to identical gray; true effective color is also omitted from the Hidden Confusion label. Forbidden markers, Barrier crosses, current player and reward/hand changes remain visible. Reveal immediately returns true-color rendering from the same authoritative board. Slate owns no gameplay timer or match copy.

Exactly nine cards are generated uniformly with replacement: Restock, Swap Hands, Steal, Tactical Nuke, Polarity, Confusion, Barrier, Back to Basics and Ghost. No new assets, Blueprint classes, modules, plugins, maps or config references are needed; `/Game/Maps/LocalMatch` and `/Script/Gomokards.LocalMatchGameMode` remain the launch references.

### Agent validation and manual status

Unreal **5.8.2 Win64 Development Editor build succeeded**, using MSVC 14.44 and Windows SDK 10.0.22621.0. Full `Gomokards` Automation export: **29 passed, 0 failed, 0 test warnings, 0 skipped, 0 in progress**: 9 Phase 1 + 3 Phase 2 + 9 Phase 3A + 8 Phase 3B groups. Counts were read from `Saved/Automation/Phase3B/index.json`, not inferred from process exit. Reproduce using the earlier build/test commands with `Automation RunTests Gomokards` and that report directory.

The eight Ghost groups cover activation/preparation atomicity; Hidden counting/restrictions; true-color rewards and Confusion; all four reveal outcomes; early/sixth winning rewards; persistent effects/reset; visibility/pool/determinism; and runtime deadline, one-shot notification, restart/stale-callback/teardown behavior. Existing groups remain, with only superseded pool/unsupported-ID expectations updated. An initial failed run exposed incorrect seeded test setup assumptions: White earns an edge-block reward before Black's opening reward. Correcting the setups preserved the existing rules and all prior assertions; the final complete rerun passed.

Static review checked state ownership, action ordering, timer lifecycle, Slate targeting/color/refresh paths and existing map/config references. Generated binaries, caches, logs and test reports are ignored and excluded from the commit. No agent gameplay or screenshot automation was performed. **User-performed manual Ghost gameplay validation is completed and passed**, reported by the user after testing Phase 3B commit `ea5f4500449ffe1957b60d9e7227ea55592741e0`. This hands-on result is distinct from the agent checks above; the checklist below is retained as the completed validation reference.

### Manual Gameplay Validation Checklist — Ghost only

Use the existing launch procedure with `/Game/Maps/LocalMatch?Seed=9`. Coordinates are zero-based. Play `(0,0)`, `(1,0)`, `(2,0)`, `(18,18)`: White earns Confusion on move 2 against the edge, Black earns Ghost on move 3, and Black acts next. This normal-action acquisition is exercised by the runtime automation test. Old eight-card seed examples are historical; for this nine-card opening, Polarity remains seed 7 and Confusion seed 10, while Barrier is seed 13 and Basics seed 6. Restart intentionally changes the seed; relaunch for a repeatable setup.

1. **Cast/freeze:** Black plays Ghost. Expect one consumed card/action, White next, five seconds of visible Black/White stones and a countdown. Board/card clicks during the countdown do nothing; action count, turn and Confusion stay fixed. At timeout all occupied stones become identical gray.
2. **Counter/reward/reveal:** White places `(0,1)` during Hidden: expect gray, remaining 6 -> 5 and one visible blocking reward. Click an occupied point: no count/action/turn change. Card buttons, including earned cards, remain unusable. Continue with five legal spaced placements such as `(6,6)`, `(8,8)`, `(10,10)`, `(12,12)`, `(14,14)`: each decrements once, the sixth reveals true colors, removes Ghost status and restores the active player's cards if the match continues and no independent lock exists.
3. **Confusion:** Relaunch seed 9; use the first three opening moves, then White plays its Confusion instead of `(18,18)`. Black casts Ghost: Confusion 2 -> 1, unchanged throughout Preparation. White's first Hidden placement appears gray; after reveal it must be Black, with Confusion having expired after that first placement. Invalid clicks must not spend its duration or expose color through the preview.
4. **Persistent effects:** In a match with Ghost plus an existing Nuke forbidden point/Barrier, cast Ghost. Their marks remain visible, forbidden placement does not reduce the counter, and the Barrier still blocks winning connectivity at reveal. Existing removed/flipped stones retain their state. Test only these Ghost interactions, not a full replay of accepted cards.
5. **Delayed wins:** Before casting Ghost, arrange four Black stones in row 2 and four White stones in row 5 without completing either line. During Hidden complete White's row on placement 1 and Black's on placement 2; neither ends play. Placement 6 reveals Draw. Repeat with only one prepared/completed line to check its correct winner; the no-line case in step 2 continues normally. A winning hidden block still earns its one card, including on placement 6; terminal reveal blocks later gameplay without an extra turn transfer.
6. **Restart:** Restart once during Preparation, once during Hidden and once after a revealed result. Expect an empty, normally visible board, empty hands, Black to act, normal card availability and no Ghost/countdown. Wait beyond the abandoned Preparation deadline: the new match must stay normal.

### Accepted limits and unsupported scope

The fixed-size English development HUD, visible hands, small state copies and full-board reveal scan remain intentional. Timer delivery occurs on the next engine tick after its real deadline; a suspended process cannot repaint or execute callbacks until resumed. Packaged-build validation, production UI/art/localization and save/replay imports remain out of scope. The pre-existing no-legal-action adjudication gap is retained: an exhausted board before six Hidden placements cannot complete the normal Ghost sequence, and no pass/early reveal/draw rule is invented; restart remains available.

Tetris, Fast Duel, Ctrl+Z/Undo, Joker, advanced/double-card effects, networking and four-player modes remain unsupported in the Phase 3B baseline. No speculative framework or additional card implementation is included in that phase. Phase 3B is complete and its manual Ghost pass is accepted. Subsequent authorized Phase 3C work is recorded below.

## Unreal Migration Phase 3C Tetris

Continues on `ue-migration` from accepted Phase 3B `ea5f4500449ffe1957b60d9e7227ea55592741e0`. The user personally completed and passed Ghost gameplay validation; its status above is updated. This phase implements **Tetris only**, using the newly specified four-edge rules. Earlier migration records remain historical. The user authorizes one implementation commit/push after agent validation, before the manual pass. The user subsequently passed core Tetris functionality and identified the against-gravity movement issue corrected in Phase 3C.1; focused manual recheck has now passed per the user.

### Impact, ownership and action boundaries

`ALocalMatchGameMode` remains the only live owner of one `FMatchState`. New `FTetrisState` contains active status, one-based block opportunity, operator index, shape, clockwise rotation, origin, edge and true color. Gravity derives from the edge. Equality/reset include the whole value. There is no falling-piece copy in Slate, physical Actor or generic mode framework.

Tetris goes through `ResolveAction`: consume once, progress existing Confusion once, complete one ordinary action, transfer once to the opponent, then `BeginTetris` spawns for that opponent. Explicit `ApplyTetrisInput` and `StepTetrisGravity` are deterministic mode operations. Movement, rotation, gravity, locking and skipping never increment `CompletedActions`, transfer the normal turn, consume Confusion or award blocking cards. The normal current player remains the caster's opponent throughout and on continuation after exit.

Operators alternate opponent/caster for six opportunities, three each. Each controls the opposite of their assigned stone color, ignoring Confusion. Remaining Confusion survives for later ordinary successful actions. Authoritative validation rejects ordinary placement/cards during Tetris; targeting is unavailable. Ghost and Tetris cannot overlap through normal play. Basics' permanent card lock remains separate. Existing Polarity colors, Nuke removals/forbidden points and Barrier anchors persist.

### Shapes and four-edge spawn

`Public/Core/TetrisRules.h` and its implementation define the six reference integer footprints: Square (4 cells), L (4), Cross (5), vertical 1x4 Line (4), Z (4), T (4). Each opportunity draws one shape uniformly with replacement from the match RNG. No minimum-variety rule is imposed.

For each edge, enumerate complete canonical footprints touching it; reject only footprints overlapping occupied stones or Nuke forbidden points. For every legal candidate, count consecutive inward translations before an obstacle or the opposite boundary. Keep greatest clearance, then closest bounding-box center to board center, then lowest Y/X origin. Center comparisons use squared integer doubled-center distances.

- **Preferred clearance is 5 grid steps.** Uniformly select among edges whose best candidate reaches at least five. Only multiple eligible choices consume an edge RNG draw.
- If none reaches five but a legal footprint exists, choose greatest clearance across all edges, then closest center. Final ties use Top, Bottom, Left, Right, then lowest Y/X. Fallback consumes no edge RNG.
- If no starting footprint exists, skip that opportunity, write/clear no cells, award nothing and advance to the next operator/shape. A skip consumes its shape draw but no normal action, turn or Confusion duration. All six blocked opportunities can exit immediately; there is no top-out loss.

Gravity maps **Top -> Down, Bottom -> Up, Left -> Right, Right -> Left**. Physical obstacles are stones and forbidden points. Barrier never blocks physical movement; it still blocks line connectivity. Edge selection uses the actual shape footprint, not an unrelated board-wide obstruction heuristic.

### Controls, runtime timer and locking

Arrows are absolute in all orientations: Up = -Y, Down = +Y, Left = -X, Right = +X. Movement with gravity and perpendicular to it is allowed. Phase 3C.1 supersedes the original permission to move against gravity: **Arrow controls remain absolute, but the direction opposite the current gravity vector is disabled so players cannot cancel natural progression indefinitely.** Space rotates clockwise within the shape's bounding rectangle with its top-left board origin fixed. There are no wall kicks or corrective translations. Invalid movement/rotation changes no state or RNG.

GameMode owns a weak core ticker, monotonic **0.5-second** deadline and callback generation, separate from Ghost. Core operations contain no wall-clock/frame time. A successful manual move exactly along gravity resets the next deadline to 0.5 seconds after that move. Perpendicular movement, rotation and rejected input (including opposite-gravity input) do not. Only a blocked automatic gravity attempt locks the current footprint; blocked manual movement and lateral contact do not.

Due callbacks advance one cell or lock. Steady deadlines advance by 0.5 seconds. After a long stall, missed intervals are not replayed as a burst; the next deadline is 0.5 seconds after the resumed callback. Delivery occurs on an engine tick, independent of world time dilation. Restart, mode exit, EndPlay and destruction cancel/invalidate the timer. Stale callbacks cannot affect a fresh match or newer Tetris activation. Automation supplies explicit timestamps without sleeping.

### Clearing and final adjudication

Lock writes true block colors into legal cells, then scans the whole board with the existing `HasWinningLine` predicate. Collect all cells in same-color connected horizontal, vertical or either diagonal runs of **at least five**, then delete simultaneously. Long runs clear entirely, intersecting runs clear their union, and both colors participate. Ordinary and deposited stones are treated equally. Barrier links can split a visual run and prevent clearing. Forbidden metadata/Barrier anchors remain. No committed stones collapse; this is not traditional full-row Tetris clearing.

Intermediate locks do not adjudicate ordinary wins. After opportunity six, complete any actual lock/clear, reset Tetris state, then evaluate the whole board: Black only wins, White only wins, both means Draw, neither continues. A real final lock has just cleared qualifying winning lines; trusted no-deployment fixtures exercise winner/Draw branches without inventing reachable ordinary wins. Skips perform no lock/clear. Exit never adds a normal turn switch. If continuation has no legal ordinary placement or supported card, retain `AwaitingRuleDecision / NoLegalAction`.

### Development presentation and pool

Slate renders all committed stones as true-color squares during Tetris and the active footprint as true-color squares with an amber outline. Forbidden marks and Barrier crosses remain visible. HUD shows TETRIS, block number out of six, operator/controlled color, edge, gravity and controls. Ordinary board/card inputs are disabled. Root `OnPreviewKeyDown` intercepts arrows/Space before focused buttons or scroll navigation can consume them; the existing UI-only controller still owns focus setup. Exit restores round stones and ordinary input immediately if play continues.

The generated pool is exactly **10 cards**, uniformly with replacement: Restock, Swap Hands, Steal, Tactical Nuke, Polarity, Confusion, Barrier, Back to Basics, Ghost, Tetris. Prior behavior changes only for this authorized addition: old pool/unsupported-ID expectations and acquisition seeds are updated without weakening gameplay assertions. The existing blocking predicate, Ghost rules, Basics and Barrier topology remain unchanged. The ordinary legal-action query is shared with Tetris exit. No map, Blueprint, config, asset, module or plugin change is needed.

### Agent validation and manual status

Unreal **5.8.2 Win64 Development Editor build succeeded**, with MSVC 14.44 and Windows SDK 10.0.22621.0. Full Automation export: **40 passed, 0 failed, 0 test warnings, 0 skipped, 0 in progress**. Counts inspected in `Saved/Automation/Phase3C/index.json`: 9 Phase 1 + 3 Phase 2 + 9 Phase 3A + 8 Phase 3B + 11 Phase 3C. Reproduce using the earlier commands with `Automation RunTests Gomokards` and report directory `Saved/Automation/Phase3C`.

`Private/Tests/TetrisTests.cpp` covers activation/effects; shapes/rotation; four-edge geometry/clearance/eligible sampling; crowded fallback/skips; absolute movement/collision; gravity/locks; six operators/full-sequence determinism; simultaneous connected clearing; final clear/evaluation/reset; pool/shape RNG/presentation key mapping; and runtime deadlines, soft drops, rejection, restart/stale callbacks, exit and teardown. All existing 29 groups remain and pass. Static review checks one authoritative state, action ordering, timer lifecycle, root keyboard routing and unchanged LocalMatch map/GameMode references. No agent gameplay or screenshot automation was performed. Generated binaries/caches/logs/reports remain ignored and excluded from the commit.

**User-performed Phase 3C validation: core Tetris functionality passed.** Hands-on testing found the against-gravity movement issue, corrected in Phase 3C.1. **Focused manual recheck completed and passed per the user**, distinct from the agent's compile, automation and static checks.

### Manual Gameplay Validation Checklist — Tetris only

Use the existing launch procedure with `/Game/Maps/LocalMatch?Seed=9` and at least 1100x800. Zero-based opening `(0,0)`, `(1,0)`, `(2,0)`, `(18,18)` naturally gives Black Tetris; Black acts next. White's edge-block reward occurs first. This ten-card setup is tested through runtime requests. Previous pool-specific seeds are historical; current Black opening acquisitions include Ghost seed 6, Polarity seed 4, Confusion seed 7, Barrier seed 10 and Basics seed 3. Relaunch for a repeatable seed; Restart intentionally changes it.

1. **Entry/input:** cast Tetris. One card/action is consumed; White controls the first Black block. Stones become squares, the active block is outlined, and the HUD identifies operator/edge/gravity. Ordinary placement/card clicks do nothing.
2. **Edges/controls:** across several blocks and board layouts, check available-edge spawns and Top/Down, Bottom/Up, Left/Right, Right/Left inward travel. Arrows retain their screen directions, but the key opposite gravity is rejected (Phase 3C.1 correction). Observe roughly one automatic step per 0.5 seconds. A successful manual inward step postpones the next step; perpendicular moves and rejected opposite input do not.
3. **Collision/rotation:** occupied stones and Nuke forbidden points reject overlapping movement; Barrier permits physical passage. Space rotates clockwise; obstructed/out-of-bounds rotation leaves the block unchanged. Blocked manual movement does not lock; the next blocked automatic step does. Nuke/Barrier marks remain visible.
4. **Six blocks/effects:** verify six alternating opportunities, three per operator, controlling opposite assigned colors. Movement/rotation/locks do not advance ordinary action count/turn or remaining Confusion, and locks never earn cards. Crowded undeployable opportunities may skip immediately without a loss or extra ordinary action.
5. **Clearing:** extend same-color horizontal/vertical/diagonal runs to 5+, including existing stones. Entire qualifying connected runs and intersecting unions disappear without collapse. A Barrier-split visual run remains when neither connected segment reaches five.
6. **Exit/restart:** after block 6, final clearing precedes normal play; round stones and normal input return to the original caster's opponent without an extra turn. Restart while moving: empty board/hands, Black starts, no old block or timer reappears. A subsequent ordinary successful action consumes saved Confusion normally.

### Accepted technical debt and unsupported scope

Keep the fixed-size English development HUD, visible hands, small value state, explicit dispatch and full-board scans. Rotation has the documented fixed bounding-box origin and no kicks. Missed real-time deadlines do not cause catch-up bursts. Packaged-build validation, production UI/art/audio, score, hold, hard drop, next-piece preview, projection, animation, save/replay imports and generic mode/timer frameworks remain out of scope.

Fast Duel, Ctrl+Z/Undo, Joker, advanced/double-card effects, four-player rules and networking remain unsupported. Ordinary no-legal-action adjudication retains its existing explicit decision boundary. Phase 3C core functionality passed user testing; the focused Phase 3C.1 movement recheck has also passed per the user.

## Unreal Migration Phase 3C.1 Tetris Movement Correction

Continues from `cb7340f570c73a5ce8ae8e43d3e383a793f03746`. The user supersedes Phase 3C's against-gravity permission after hands-on testing. `ApplyTetrisInput` now rejects translation equal to `-TetrisGravity(CurrentEdge)` before committing state: Top/Down rejects Up; Bottom/Up rejects Down; Left/Right rejects Left; Right/Left rejects Right. Absolute key mapping is unchanged.

The rejection preserves the complete match state, RNG, rotation, ordinary actions, Confusion and runtime gravity deadline, and never locks. Gravity-direction soft drop still resets the deadline to 0.5 seconds; both perpendicular moves remain valid without resetting it. Only blocked automatic gravity locks. Spawn, collision, rotation, alternation, clearing, Barrier/Nuke behavior and the ten-card pool are unchanged. No networking, new card or movement-policy abstraction was added.

The existing movement and runtime test groups now explicitly cover all four rejected keys and all twelve allowed orientation/direction combinations. Complete-state equality and unchanged runtime deadline/notifications are asserted on rejection. All prior groups are preserved.

Agent validation: Unreal 5.8.2 Win64 Development Editor build succeeded. The complete Gomokards Automation suite exported **40 passed, 0 failed, 0 test warnings, 0 skipped, 0 in progress**, verified in `Saved/Automation/Phase3C1/index.json`. Final diff review confirms only the core guard, movement/runtime regression assertions and README changed; generated/cache files are excluded. User-reported core Tetris functionality passed; the identified movement issue is corrected here, with focused manual recheck completed and passed per the user. No agent gameplay is performed.

User-performed Phase 3C.1 focused manual validation: **completed and passed** on `3af07c4cc96eab330d8323f2bfac6bcc7ac447c7`. The user confirmed all four opposite-gravity rejections and correct gravity/perpendicular movement. Retained completed Manual Gameplay Validation Checklist: for Top, Bottom, Left and Right spawns, verify respectively Up, Down, Left and Right do nothing; verify the gravity-direction key and both perpendicular keys still work when space exists. Gravity must continue normally after rejected input.


## Phase 4A Multiplayer Architecture Impact Pass (historical proposal)

**Historical README-only proposal at `d2cfa1a447da726bdcb14e585e8570b1d2449020`; superseded by the Phase 4A implementation record below.** Reviewed against accepted `ue-migration` implementation `3af07c4cc96eab330d8323f2bfac6bcc7ac447c7`. Phase 3C.1 focused manual validation passed. This pass changes README only: no networking code, dependencies, configuration or assets are added. Multiplayer has not been demonstrated. The following is the plan for a separately authorized Phase 4A implementation, followed by Phase 4B card/mode networking and later Phase 4C connection/session services.

### Current dependencies and exact breakpoints

Paths below are relative to `Gomokards/Source/Gomokards/`.

| Inspected component | Current behavior and multiplayer impact |
| --- | --- |
| `Public/Runtime/LocalMatchGameMode.h`, `Private/Runtime/LocalMatchGameMode.cpp` | Own the sole plain `FMatchState`; `InitGame` immediately seeds/resets; `Submit` resolves actions and broadcasts `OnMatchChanged`; Ghost and Tetris use weak core tickers, monotonic deadlines and generations. Keep this server authority. Add connection assignment/start gating and publish projections after accepted changes. Public C++ access alone is not network authorization. |
| `Private/Runtime/LocalMatchPlayerController.cpp` | `BeginPlay` requires `GetAuthGameMode` before constructing Slate. A remote client therefore gets no view. Create UI for local controllers with a viewport, independent of GameMode; wait for replicated identity/state readiness. Dedicated servers create no UI. |
| `Private/Presentation/SLocalMatchView.h/.cpp` | Holds GameMode, calls `GetMatch`, `Submit`, `NewMatch`, `SubmitTetris`, countdown query and subscribes to its delegate. `Refresh` exposes both hands; `CardClick` passes a player ID; hover calls core validation against full state. Replace this entire access boundary with the owning controller's read-only presentation queries and intent methods. |
| `Public/Presentation/MatchPresentation.h`, private implementation | `FTargetSelection::BoardRequest` chooses the current player's ID, appropriate for hot-seat but unsafe as a client identity source. Target selection, color/result/effect labels consume full state. Retain geometry and key mapping; adapt state-dependent helpers to projected information and local assigned identity. Local eligibility/hover checks are hints, not authorization. Do not manufacture a partial `FMatchState` on clients to run rules. |
| `Public/Core/MatchState.h`, `Private/Core/MatchState.cpp` | Includes both hands, full true board, RNG, result/effects/modes. Players default to IDs 0/1 and Black/White; `SingleOpponentIndex` centralizes the two-player assumption. Preserve gameplay IDs; do not substitute connection index, UE PlayerId, host status or controller index. |
| `Private/Core/MatchRules.cpp`, `Private/Cards/CardDefinitions.cpp`, `CardEffects.cpp` | Validation trusts the caller to supply a player ID, then checks turn/ownership/targets. Candidate-copy resolution includes RNG/rewards and commits atomically. Ten-card definitions and draw/Steal randomness already belong to the core. Only the server adapter may construct authoritative requests from controller assignment. |
| `Private/Core/TetrisRules.cpp` and GameMode timer/input methods | Mode input has no caller identity, deliberately adequate for local hot-seat. Network adapter must authorize against `Players[Tetris.OperatorIndex].Id`, not the unchanged ordinary `CurrentPlayerIndex`. Timers remain server-only; rejected input must not reset a deadline. |
| `Gomokards.Build.cs`, project/config/map | Already uses Core/CoreUObject/Engine/InputCore/EnhancedInput and private Slate/SlateCore; no active OnlineSubsystem dependency. `DefaultEngine.ini` uses `/Game/Maps/LocalMatch` and `/Script/Gomokards.LocalMatchGameMode`. Reuse the map; select the new GameState in the GameMode constructor. Basic Engine replication needs no online service/plugin. |
| `Private/Tests/*Tests.cpp` | 40 accepted groups include pure rules, presentation helpers and transient runtime worlds with deterministic timer seams. These do not demonstrate connections, replication privacy or RPC routing; keep them and add dedicated boundary/integration coverage. |

### Proposed ownership and data flow

Initial model: two-player listen server plus remote client. Host contains a server and one local client; both players use the same controller contract. This is server-authoritative, not peer-authoritative simulation or lockstep. A future dedicated server runs the same owner/core/projection code without a local viewport.

```mermaid
flowchart LR
  H[Host local Slate] --> HP[Owning PlayerController intent contract]
  C[Remote Slate] --> CP[Owning PlayerController intent contract]
  HP -->|Server RPC, local execution on host| G[Server GameMode: assignment and validation]
  CP -->|Server RPC| G
  G --> R[Pure rules and authoritative FMatchState]
  T[Server timers] --> G
  R --> P[Server projection conversion]
  P --> GS[GameState public snapshot]
  P --> PV[Each owning controller: private view]
  GS --> V[Local read-only view refresh]
  PV --> V
  G -->|Owner Client RPC: action result| V
  V --> H
  V --> C
```

| Object | Proposed responsibility |
| --- | --- |
| GameMode, server only | Sole authoritative match, controller-to-`FPlayerId` table, two-seat lifecycle, intent authorization, core calls, all RNG/timers, resets, projection publication. No host-player shortcut. |
| New `ALocalMatchGameState : AGameStateBase` | Replicated public projection and runtime session status; read-only on clients. No hands, RNG or rule execution. `OnRep` announces refreshed local presentation. |
| Existing PlayerController, server and owner | Reliable intent RPCs; server derives actor from its own assignment table. Owner-only replicated private view, local read-only access to public/private views, small action-result Client RPC and view-change delegate. Owns UI, never a second authoritative match. |
| PlayerState | Keep Unreal's existing connection/player metadata. Its engine PlayerId is not `FPlayerId`. A custom PlayerState is unnecessary initially: public gameplay assignments live in GameState; own assigned ID and hand live on the owning controller. Do not put unconditionally replicated hands on globally visible PlayerStates. |
| Slate | Reads only projected data through controller; sends coordinates/card/input intent; owns hover, selection and pending-feedback state only. Host UI follows the same privacy/view contract. |
| Core | Keep plain types and deterministic operations unchanged by networking: no reflection/replication macros, RPCs, authority branches, UI or OnlineSubsystem. |

A direct reflected/replicated `FMatchState` (option A) entangles core types with replication and risks exposing secrets; per-field conditions still leave visibility and ownership problems. Prefer option B: a few explicit reflected projection DTOs outside Core. A small server conversion function called by GameMode produces public and per-owner projections after commits, resets and timer transitions. These are derived transport/display state, never independently mutable gameplay copies. No custom serialization framework, per-cell Actors or new service layer is warranted.

### Public/private inventory and refresh consistency

| Visibility / owner | Exact proposed information |
| --- | --- |
| Public GameState, Phase 4A | 361 row-major display cells (ordinary Empty/Black/White plus forbidden flag); Barrier anchors; two gameplay ID/stone assignments and occupied-seat status; current gameplay ID; completed actions; result status/winning stone/decision reason; Basics lock and Confusion count (currently intentionally displayed); session status and match epoch/revision. Existing mode fields are inactive defaults until exposed in Phase 4B. |
| Public mode extensions, Phase 4B | Ghost phase, placements remaining, display deadline and occupancy/redacted cells; Tetris active flag, block opportunity, operator gameplay ID, shape, origin, rotation, edge/derived gravity and true block color. Never expose future random draws. |
| Owner-private PlayerController | Assigned gameplay ID, own ordered hand card IDs including duplicates, epoch/revision. Use owner-only replication conditions; only the corresponding client receives these fields. Own count derives from own array. Rebuild both owners' private views after Swap/Steal later; no public hand-content event. |
| Server only | Full `FMatchState`, both authoritative hands, RNG seed/current stream, Ghost-hidden true colors, monotonic timer deadlines/generations, assignment map and development restart authorization. |
| Resolved public field | Opponent hand count is always public; card identities/order remain owner-private. The implementation below follows the subsequent user decision. |

During Ghost Hidden, merely painting replicated true colors gray is insufficient. Future public cell DTO must support occupied-but-color-hidden independently of core `EStone`; redact all hidden cell colors before transport and replace client presentation cache, then publish real colors on reveal. Previously visible information cannot be made unknown again; players may also infer colors from public turns/Confusion. Do not promise cryptographic secrecy against such inference. A listen-server operator can inspect server memory containing all secrets; owner-only replication protects remote delivery, not against a malicious host. Dedicated hosting improves that trust boundary without changing rules.

Public/private properties on different Actors and result RPCs may arrive in different orders. Use a small match epoch (changes on reset) and committed revision shared by projections/acknowledgements. The controller waits for matching public/private revisions before enabling a complete interactive view; old epochs are discarded. Phase 4A publishes both owner projection revisions on each accepted ordinary commit even if a hand is unchanged. These are snapshot coherence markers, not an event log or generic request protocol. Later high-frequency Tetris pose updates can be separate from the slower coherent board/hand snapshot.

GameMode's `OnMatchChanged` may remain a server-side commit notification for tests/publication. It must no longer be Slate's access to authority. GameState/private-view `OnRep` callbacks feed one controller-local refresh delegate. Server publication must explicitly invoke that same refresh path for its local host (and standalone) view rather than assume C++ server assignment invokes a client RepNotify. Handle initial GameState/assignment arrival in either order; unbind safely on teardown. Refresh never runs core rules.

### Identity, intent and acknowledgement contract

Server `PostLogin` assigns the first available gameplay seat; initially first connection gets core ID 0/Black and second ID 1/White. This is seat allocation, not a host rule: authority checks consume assigned IDs, and tests must swap controller-seat mappings, including a White host. Neither the client nor a URL-supplied player ID chooses the actor. Unreal connection ownership is the boundary here; no backend/account authentication is claimed. Reject a third gameplay connection; no generic lobby/spectator path.

Use runtime `WaitingForPlayers`, `Playing`, `SessionEnded` outside `FMatchState`. First connection receives identity plus waiting view, with gameplay rejected. Second connection triggers server reset/start and coherent projections; Black moves regardless of which process owns Black. `InitGame` must stop opening a playable network match immediately. On disconnect, end/invalidate the test session, cancel timers and reject further intents without inventing a Gomoku winner; reconnect/recovery is deferred.

Proposed typed reliable Server RPCs on the owning controller:

- Phase 4A: `ServerPlaceStone(epoch, expectedCompletedActions, x, y)`. Derive actor from the calling controller on the server, verify session/epoch/turn token, then construct `FActionRequest::Place`. No client player ID, stone, result, RNG or board payload.
- Phase 4B: `ServerPlayCard(epoch, expectedCompletedActions, cardId, optionalTarget)` uses the same route, with bounded enum/target checks then core ownership/target validation. Phase 4A rejects/unexposes card play at the server boundary, including host attempts, while retaining all core cards and earned hands.
- Phase 4B: `ServerTetrisInput(epoch, activationToken, blockNumber, input)` authorizes the assigned ID against the current Tetris operator before `SubmitTetris`; rejects stale piece input and unknown directions. The block token prevents delayed controls moving the next operator's block. No prediction or client gravity.

The ordinary action token prevents a delayed duplicate click becoming a fresh move when the same player's next turn arrives; epoch prevents a queued pre-restart intent entering a new match. These small stale-intent guards have concrete failure cases. Reliable ordering on a controller plus one outstanding ordinary UI request is enough; do not add per-request sequence numbers, resend queues or an event bus. Validate all requests regardless of UI gating; malformed/wrong-turn/unassigned/stale requests produce zero core/RNG/timer mutation. Use a modest controller input-rate bound when exposing key repeats in Phase 4B, not an anti-cheat framework.

Return a small reliable owner Client RPC `{epoch, revision, accepted/rejectionReason, blockingReward}` for each processed ordinary intent. Rejection needs explicit feedback because no replicated property changes. Accepted feedback does not patch the board or invent the drawn card: projections deliver truth. Buffer accepted feedback until a coherent view in the same epoch has reached at least the acknowledged revision (replication may coalesce intermediate revisions); discard obsolete epochs. Server errors should disclose no opponent secret. No multicast hand/reward payload. Whether observers see reward-count feedback is governed by the visibility decision, not by this private acknowledgement.

Development restart is a separate administrative capability: a server console/test command, or an explicitly server-authorized local-host development control. If exposed through a controller, check server-granted admin permission, not gameplay seat or a client boolean. Remote arbitrary reset requests reject. Reset uses the same server owner/reset/publish boundary and advances epoch. A dedicated server uses its console instead; this is not a privileged host gameplay path or a production rematch vote.

### Timers, future modes and bandwidth

Keep existing monotonic Ghost and Tetris tickers on the server, with generation cancellation on reset/session end. Clients cannot call transitions, lock blocks or advance RNG. Ghost countdown should use a replicated phase-end timestamp plus `AGameStateBase::GetServerWorldTimeSeconds()` for display, not countdown replication every frame. Convert server remaining real duration into that display clock when publishing; do not subtract a raw `FPlatformTime` deadline from world time. Installed UE 5.8 source shows the synchronized API uses world game time. Initial network sessions therefore keep world pause/time dilation disabled (1x); if those features are later supported, add a small real-time clock anchor/rebase for display. Even at displayed zero, wait for the replicated phase change; the real-time server callback remains authoritative.

Ghost Phase 4B publishes Preparation/Hidden/reveal, redacts hidden cells and retains intentional blocking rewards/private delivery. Tetris Phase 4B authorizes the operator separately from ordinary turn, applies corrected absolute-direction inputs on the server, advances real-time gravity there, and publishes active shape/pose/color. Pending UI controls may be shown as pending, never as committed movement. Plain arrays and a public snapshot suit the 19x19 low-frequency Phase 4A board. Re-sending that board/hand presentation on every Tetris input would be wasteful: split an independently replicated small active-piece projection from board/result state when Phase 4B needs it; publish board on locks/clears. This changes transport layout, not core ownership. No FastArraySerializer, custom compression, sockets/protocol, state hashing, prediction or rollback now; measure actual latency/bandwidth first.

### Standalone preservation, files and scope

Replace UI-to-GameMode access in standalone as well. A controller submits through the same server adapter and consumes the same projections. In explicit `NM_Standalone` hot-seat only, the server adapter may authorize its sole local controller for the current ordinary player/current Tetris operator; never enable that fallback on listen/dedicated servers. Show the currently operated player's private hand; retaining a separate both-hands debug display is unnecessary. Keep local card/Ghost/Tetris capability behind this same boundary, with their existing rules and timers. Multiplayer Phase 4A's card restriction is an adapter capability limit, not a rewrite/removal of the ten-card core. Local mode presentation fields can be populated for development without claiming network mode acceptance.

Expected implementation footprint: add `Public/Runtime/LocalMatchGameState.h` + private `.cpp`, a small `Runtime/MatchNetTypes.h` projection header and matching projection conversion functions/file if needed; change existing GameMode/PlayerController headers and implementations; change `SLocalMatchView` and state-dependent presentation helpers; add authority/projection tests and update adapter tests/README. No custom PlayerState, GameInstance subsystem, online module, new map or assets required initially. Register GameState through GameMode; add `Net/UnrealNetwork.h` where needed, keeping network includes outside Core. A server build target and dedicated packaging are later deployment work; viewport creation must already be absent on headless authority.

Required now: identity/authorization, private projection, consistent view refresh/acknowledgement, waiting/start/admin reset and ordinary placement/reward/outcome. Later Phase 4B: expose/regress all ten cards and both timers/modes. Later Phase 4C: chosen discovery/session provider; it connects clients to the same gameplay server. Do not build matchmaking, accounts, ranking, reconnect, replay infrastructure, spectator systems, rematch voting, NAT/relay/EOS/Steam, generic N-player/services/message bus, anti-cheat frameworks or rollback.

### Implementation sequence and acceptance (future work)

1. Resolve the narrow visibility gate below; define public/private DTOs and pure projection tests. Freeze existing 40-group core baseline; no reflected-core migration.
2. Add GameState and server seat assignment, waiting/start/session-end states and epoch. Test normal and reversed controller-seat mappings without UI.
3. Add controller placement RPC/acknowledgement and server validation; publish projections after commits and authorized reset. Reward draws still use the existing ten-card server pool; hands are readable only by their owners and card play is unavailable in this network slice.
4. Migrate Slate and standalone to controller queries/intents, own-hand display and readiness/OnRep refresh; preserve pure geometry and local targeting intent. Remove all UI GameMode/full-state access.
5. Compile and run the full core suite plus new projection/authority tests: spoof/unassigned/wrong-turn/stale epoch/duplicate turn/invalid coordinates/remote reset leave complete state and RNG unchanged; owner A's projection never contains B's hand; initial, reward and reset projections are coherent. Exercise existing Won/Draw/AwaitingRuleDecision representation via trusted server fixtures. Ordinary placement alone cannot create the existing simultaneous-win Draw; do not invent a draw rule to make it reachable.
6. User manual PIE pass: two windows (listen server plus client), assigned identities and waiting/start; Black moves only from its own window; both see the same board/action/turn/result; White wrong-turn attempts reject; correct owner alone receives the blocking card contents and feedback; terminal input rejects; admin reset clears both views and queued old intents cannot mutate the new match. Spoofing and actual property privacy require automated/server inspection, not merely disabled buttons. Repeat with host assigned White via a server test fixture, and use separate PIE processes when inspecting privacy.
7. Then user LAN pass on two physical PCs using direct address connection; no Internet/session provider required. Check no client GameMode access, no host-only gameplay route and no viewport dependency on server-only execution. Actual dedicated deployment testing follows later, without changing rules.

Phase 4A acceptance requires successful compilation/full regression, passing new boundary/privacy tests, synchronized two-client ordinary play, owner-private reward delivery, explicit rejection feedback, authorized restart and a user-reported manual pass. It does not claim all-card multiplayer, Internet connectivity or dedicated packaging. No build/tests/gameplay were run in this documentation pass; the prior 40-test result is historical evidence, not network validation.

### Decision gate and accepted limitations

**Visibility decision resolved by the user before implementation:** opponent hand counts are always public, opponent hand contents always private. There are no Phase-specific count exceptions. The original decision gate is closed.

Other items are implementation constraints, not reopened gameplay decisions: connection-order seats are a replaceable assignment policy; restart is a development admin operation; disconnect invalidates the session; no account authentication or hostile-host confidentiality is promised. A full-board Phase 4A test with earned cards may still have core-legal card actions that the network slice deliberately does not expose. Label that as the test slice's capability limit and stop/restart via runtime session status, without rewriting `NoLegalAction`, inventing Draw or changing card availability in the core. Existing no-legal-action product rules remain deferred. Main failure risks are leaked DTO fields, host shortcuts, trusting request IDs as identity, public/private arrival races, late intents crossing resets/blocks, and mixing clock domains; the ownership/projection/epoch/operator checks above address them without a framework.

Unreal-native checks were grounded in the installed UE 5.8 `GameStateBase.cpp` clock implementation and gameplay framework headers, and Epic's [GameMode/GameState guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-mode-and-game-state-in-unreal-engine) and [replication execution-order documentation](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/replicated-object-execution-order-in-unreal-engine). Those engine guarantees support the proposal; no networking implementation was present in that architecture-only baseline commit.


## Phase 4A — Two-player server-authoritative ordinary-placement slice

**Implemented; automated validation passed; Phase 4A PIE manual validation: PASSED.** Based on architecture baseline `d2cfa1a447da726bdcb14e585e8570b1d2449020`. Phase 3C.1 manual acceptance on `3af07c4cc96eab330d8323f2bfac6bcc7ac447c7` remains accepted. This section supersedes the implementation/proposed wording and visibility gate in the historical impact pass above. The user validated two-player PIE on `3a60dfc6b1babce27ff175ce7672dcfb1081f322`; two-physical-PC LAN validation remains pending until the first Development packaged build and is not a blocker for Phase 4B. Internet and dedicated-server deployment are not claimed. This Phase 4A record describes its accepted baseline; the Phase 4B.1 section below supersedes its card-capability restrictions.

### Implemented ownership and transport

- `ALocalMatchGameMode` is the only authority for `FMatchState`, RNG, resolution and existing mode timers. It owns controller-to-gameplay-ID assignments, session state, epochs/revisions and development-admin grants. The pure Core and Cards code is unchanged and contains no new networking/reflection/presentation dependencies.
- New `ALocalMatchGameState` contains a replicated public snapshot only, selected by the GameMode constructor. `MatchNetTypes.h/.cpp` defines reflected display/transport types and explicit server conversions; no replicated `FMatchState`, RNG, full player state or per-cell Actor exists.
- Existing `ALocalMatchPlayerController` sends intents and owns Slate. Its private snapshot uses `COND_OwnerOnly` on an owner-relevant PlayerController. Default Unreal PlayerState remains connection metadata; no custom PlayerState, GameInstance service, online dependency, config, map or asset change was needed.

| Projection | Actual fields / visibility |
| --- | --- |
| Public GameState | 361 Empty/Black/White + forbidden cells; Barrier anchors; two gameplay IDs, stone assignments, occupied flags and hand counts; current gameplay ID, completed actions; result, winning stone and decision reason; Basics flag and Confusion count; WaitingForPlayers/Playing/SessionEnded; epoch and revision. **Both hand counts are always public. No card IDs or RNG.** |
| Owner-private controller | Own assigned gameplay ID/stone, own ordered card IDs (duplicates retained), server-granted development-admin availability, epoch and revision. **No opponent hand contents.** In standalone hot-seat only, the displayed private hand follows the currently operated player. |
| Owner action acknowledgement | Epoch/revision, accepted flag, safe adapter/core rejection code and blocking-reward flag. No board patch or drawn card ID. |

Card rewards still draw from the existing ten-card pool on the server. Both public counts update, while only the earning owner receives the drawn identity. Disabled hand entries explain: “Card play networking arrives in Phase 4B.” The server adapter also rejects card play in initialized network sessions; there is no card or mode RPC.

### Identity, RPCs and session lifecycle

`PostLogin` assigns the first free gameplay seat (initially ID 0/Black then ID 1/White). Server authorization reads this mapping; it does not equate host, controller index or Unreal PlayerId with Black. Automated fixtures reverse the connections so the development host is White and the remote-assigned Black starts.

The owning controller exposes reliable `ServerPlaceStone(Epoch, ExpectedCompletedActions, X, Y)`. It has no client-supplied actor ID, color, reward or result. GameMode checks assignment, Playing status, epoch and action token before constructing the core placement request with its own actor ID. The unchanged core validates turn/cell/outcome and commits atomically. Rejections preserve the complete authoritative state, RNG, revisions and timer deadlines. Action tokens stop delayed duplicates from becoming valid when the same player's next turn arrives; epochs reject requests from before restart.

First network player waits; the second resets/starts the match and Black moves first. A third connection stays unassigned and cannot act. An assigned disconnect publishes SessionEnded, cancels timers and rejects further play without inventing a Gomoku winner. Reconnect is unsupported. A full-board in-progress fixture with core-legal but unexposed card actions also ends the runtime slice; the core outcome is not rewritten to Draw. Start a new session after SessionEnded.

Development restart is an explicit server grant to the local listen-server controller, separate from seat/color. Reliable `ServerDevelopmentRestart(Epoch)` rechecks this grant and the current session/epoch; arbitrary remote requests reject even on that player's turn. Authorized restart resets board/hands and RNG, advances epoch and publishes both views. This is a development control, not a rematch vote. No dedicated-console command or server packaging is included yet.

### Presentation consistency and standalone scope

Slate now holds only its owning controller, reads its coherent public/own-private display queries and sends coordinate/restart intents. It has no GameMode/full-match query, direct submit/reset/timer call or reconstructed partial core state. Hover uses public empty/forbidden/session/turn hints; the server remains the final validator. Wrong-turn and invalid placement requests can return explicit feedback instead of silently disappearing behind a UI filter.

Local viewport creation no longer needs `GetAuthGameMode`; headless/dedicated authority creates no view. GameState/private `OnRep` callbacks refresh presentation, and server publication explicitly uses the same refresh path for the listen host. Public/private snapshots must share epoch and revision before input is ready. Each accepted commit updates both owners' revision even if their hands did not change. Accepted acknowledgements wait for a coherent revision at least as new as the acknowledgement, supporting coalescing; acknowledgements do not fabricate board/card state. One UI request remains outstanding until its acknowledgement is accounted for, including across resets. Old-epoch display contents/feedback are discarded.

Standalone ordinary hot-seat uses the same controller/server adapter; only an explicitly initialized `NM_Standalone` world permits its local controller to act for the current player. This fallback is unavailable in listen/dedicated sessions. **Temporary presentation limitation:** this Phase 4A view exposes ordinary placement and earned own-hand inspection only, including in standalone. The previous card-targeting/Ghost/Tetris controls are not exposed by this view; their core rules, local runtime timer/input methods, geometry/targeting helpers and all prior tests remain intact. Rebuilding their presentation/transport is deferred to Phase 4B rather than maintaining a second UI-to-GameMode path.

### Agent validation

Unreal **5.8.2 Win64 Development Editor build succeeded**. The full `Gomokards` Automation export at `Saved/Automation/Phase4A/index.json` reports **43 passed, 0 failed, 0 test warnings, 0 skipped/not run, 0 in progress**: all 40 accepted groups plus:

- `Gomokards.Phase4A.ProjectionSeparation`: full ordinary board/metadata, gameplay IDs/counts, per-owner hand order/duplicates, no projection mutation/RNG use, all result representations.
- `Gomokards.Phase4A.ReflectedPrivacyAndRpcContract`: compiled reflected field inventory, actual lifetime replication conditions, owner-relevant controller, typed reliable server placement parameters without an actor ID, and owner Client acknowledgement (no multicast).
- `Gomokards.Phase4A.AuthorityLifecycleAndPresentation`: waiting/two-seat/third/disconnect lifecycle; reversed seat mapping; complete-state rejection safety; stale/duplicate tokens; exact core reward/private delivery; win/terminal fixtures; remote/admin restart; public/private/ack ordering and coalescing; reset acknowledgement gating; constructing/reading Slate in a world with no authoritative GameMode.

The first test run exposed fixture setup errors (replication indices needed the engine's runtime initialization; a synthetic future epoch needed to be advanced past when restoring the fixture). Those were corrected, and the final exported complete suite above passed. Review also added the reset/ack outstanding-request regression. The editor logs contain environment/startup warnings about the G: file journal and editor layout compatibility; these are not Automation test warnings.

Static review confirms only explicit projection properties are replicated, no hand IDs are reachable through the public DTO, private hand replication is owner-only, the server derives actors and grants restart permission, and Slate has no GameMode access or both-hands debug rendering. Existing Core/Cards, all 40 prior test groups, map/config/build dependencies and assets are unchanged. Generated/cache files are excluded from the implementation commit. These are in-process authority/projection/reflection checks, **not a packet-capture test or proof of real two-process delivery**. No agent-driven gameplay was performed.

### Manual PIE validation — user PASSED

**Phase 4A PIE manual validation: PASSED.** The user personally completed and passed the complete two-player Listen Server + Client PIE checklist on implementation commit `3a60dfc6b1babce27ff175ce7672dcfb1081f322`, confirming:

- Both Listen Server and Client created working UI; WaitingForPlayers transitioned correctly when the second player joined.
- Black/White identity and turn ownership were correct; ordinary placements replicated consistently to both windows.
- Wrong-turn and invalid placement requests rejected without state mutation.
- Successful-block rewards exposed the exact card identity only to the owner; the opponent saw only the public hand-count increase.
- Ordinary win/result synchronized; terminal actions rejected.
- Arbitrary remote restart was unavailable/rejected; authorized Host development restart reset both views coherently, and play continued normally afterward.
- Disconnect moved the remaining player to SessionEnded without inventing a winner.

This is user-performed hands-on validation, distinct from the agent's implementation-time build and 43-test Automation results above. This README-only acceptance update runs no build or Automation tests.

### Retained Manual PIE Validation Checklist — completed by the user

Use `/Game/Maps/LocalMatch`, two PIE players, **Play As Listen Server**, preferably separate PIE processes when checking ownership. For observing the waiting state, launch the listen server before connecting the second instance; automatic two-window startup may make it brief.

1. First player shows “Waiting for second player” and cannot place. Second join starts an empty board with Black current. Each window shows its own correct identity, and both show public hand counts.
2. Place Black from Black's window: both boards/action counts/turns agree. Click from White's window while Black is current, then test an occupied point on the correct turn: rejection feedback appears and neither board changes. Actor spoofing and stale/duplicate requests are covered automatically; no user-facing spoof control is added.
3. From a fresh match, use zero-based board coordinates: Black `(5,5)`, White `(6,5)`, Black `(0,0)`, White `(7,5)`, Black `(8,5)`. Black should earn exactly one card. Black sees its name; White sees only Black's count increase. Card entries remain disabled in both windows.
4. Continue/restart and form an ordinary five-in-a-row. Both windows show the same winner; further placements reject without changing the result.
5. Remote development restart is disabled (server rejection is covered automatically). Use the host's development restart: both boards/hands/counts clear, Black starts, and further normal input works without stale feedback. Rapid input around restart must not add an old move.
6. Close/disconnect one player: the remaining window shows SessionEnded, no winner is invented and placements reject. A new session is needed. If practical, repeat identity/turn/restart checks with the reversed assignment fixture; the automated suite already covers a White development host, and no client-selectable seat switch is exposed.

**Phase 4A PIE manual validation has passed; two-physical-PC LAN validation remains explicitly pending until the first Development packaged build.** LAN is not a blocker for Phase 4B. Internet multiplayer has not been demonstrated. The PIE acceptance does not establish LAN or Internet validation. Phase 4B.1/4B.2/4B.3 below add basic, targeted and persistent-rule card networking; Ghost/Tetris networking (including Ghost redaction and mode clock/pose transport) and Phase 4C Internet/session-provider work remain pending. No promise is made against a malicious listen-server host, who owns authoritative memory.


## Phase 4B.1 — Basic card networking

**Implemented; automated validation passed; Phase 4B.1 user manual two-player PIE validation: PASSED.** Built on accepted Phase 4A implementation `3a60dfc6b1babce27ff175ce7672dcfb1081f322` and its user PIE acceptance record `28bb4a96289dd2443f403ed1555aaffa4a9cbeff`. Phase 4A manual acceptance remains passed. Physical two-PC LAN validation is intentionally pending until the first Development packaged build, not a prerequisite for this phase.

This section records the accepted Phase 4B.1 baseline; Phase 4B.2 below extends its capability restrictions. At this baseline, only **Restock, Swap Hands and Steal** were network-playable. The existing authoritative **ten-card draw pool and probabilities are unchanged**. Tactical Nuke, Polarity, Confusion, Barrier, Back to Basics, Ghost and Tetris can still be earned/drawn and appear by their real names in the owner's hand, but remain disabled with “networking not enabled yet.” No targeting or mode transport was added.

### Server path and exact card behavior

Slate calls its owning controller's `RequestCard`; the same reliable `ServerPlayCard(Epoch, ExpectedCompletedActions, uint8 CardId)` path serves host and remote players. There is no client actor ID, target, random index or result payload. GameMode's `CardFrom` checks assignment, Playing status, epoch and action token, then the explicit three-card `IsNetworkCardEnabled` whitelist before constructing the existing core `FActionRequest::Play` with the server-assigned gameplay ID. It uses the existing `Submit` → `ResolveAction` commit path. Core remains responsible for turn, ownership, terminal/lock rules, consumption and effects; **no Core or Cards source was changed**.

The adapter rejects all seven later-phase cards, unsupported IDs and malformed bytes before core mutation. A new runtime-only `CardNotNetworkEnabled` acknowledgement reason produces “Card networking not available until a later phase.” Core card definitions/error enums remain unchanged. All rejections preserve complete state/RNG, revision, public/private snapshots and timer deadlines. Standalone's existing current-player fallback remains restricted to `NM_Standalone`; its view uses the same three-card intent path.

| Card | Accepted existing Core behavior |
| --- | --- |
| Restock | Remove the played Restock, then draw exactly two cards with the authoritative RNG. Actor hand size changes by +1 net; opponent hand stays unchanged. |
| Swap Hands | Remove the played Swap Hands first, then exchange the remaining two hands, including their exact order/duplicates. No RNG use. |
| Steal | Remove the played Steal first. If the opponent has cards, choose with authoritative RNG, remove the first occurrence of that selected card ID (existing duplicate-order semantics), then append it to the actor's hand. Actor count is unchanged net; victim count decreases by one. If the opponent is empty, Steal is still consumed and the action/turn completes, but no card is transferred and RNG does not advance. |

Every accepted card completes one ordinary action and uses the existing turn/effect/outcome processing. A necessary adapter correction extends the Phase 4A full-board capability check: a current player with a legal network-enabled card can continue even with no empty legal point. If only later-phase actions remain available to Core, the runtime slice still ends with SessionEnded rather than inventing a core Draw.

### Privacy, projections and presentation

Every accepted card increments the normal committed revision and rebuilds the public projection **and both owners' private projections**, with matching epoch/revision. Public GameState continues to contain both hand counts and no card identities. Each owner receives only their resulting own ordered hand. Swap/Steal replace each controller's coherent own-hand display; there is no shared both-hand cache or extra copy of the opponent's new hand. Information players infer from their own previous hand is not artificially erased.

The existing owner acknowledgement and coherence handling are reused unchanged: no drawn/stolen card ID, hand array or board patch is added to acknowledgements. An early accepted acknowledgement cannot fabricate a new hand; input waits for coherent projections. Clients never roll or predict draws/steals. RNG and complete authoritative hands stay in GameMode. Owner-private replication remains `COND_OwnerOnly` on owner-relevant controllers. No new public event, client log or both-hands debug display reveals identities. This does not provide confidentiality against a malicious listen-server host.

Own-card buttons use projected session/turn/result/Basics status, possession, the whitelist, coherence and outstanding-request state as local hints. The server independently validates every request. Host UI still has no direct GameMode call. No new service, network abstraction, targeted payload, map, asset, config or dependency was introduced.

### Agent validation

Unreal **5.8.2 Win64 Development Editor build succeeded**. Exported `Saved/Automation/Phase4B1/index.json` reports **45 passed, 0 failed, 0 test warnings, 0 skipped/not run, 0 in progress**. The prior 43 groups and their source are preserved. Added:

- `Gomokards.Phase4B1.CardRpcAndWhitelist`: compiled reliable RPC has exactly epoch/action token/card byte; all 256 byte values permit exactly the three supported IDs; all ten cards remain in Core.
- `Gomokards.Phase4B1.CardAuthorityPrivacyAndCoherence`: all three cards' ownership/turn/session/terminal/token rejection safety; explicit rejection of every later-phase card; deterministic full-state/RNG parity with Core for both seat mappings; both own-hand snapshots/public counts; early acknowledgement and both property arrival orders; no stale hand after Swap/Steal; empty-victim Steal; full-pool later-card rewards/Restock and rejection without consumption; placement after cards; full-board capability handling; and the exact seed/opening recipe below through the server adapter.

Static/reflection review confirms the Phase 4A public/private/ack reflected field inventories and owner-only replication conditions still pass, no `FMatchState`/RNG or card identity array was added to GameState/acknowledgements, both private projections rebuild on each commit, and Slate has no GameMode/full-state resolution path. The only GameState header change grants access to the new Automation fixture. No generated/cache files are included. Startup file-journal/editor-layout warnings are outside Automation test results. These checks are in-process authority/projection tests and compiled reflection inspection, not new real-client gameplay or packet capture. No agent-driven gameplay was performed.

### Phase 4B.1 user manual PIE validation — PASSED

The user reports that two-player PIE manual validation of Phase 4B.1 is **completed and PASSED**, on the latest accepted implementation `e72ddfa62bbf6007e7c8285b562ddf824ab32965`. This is user-performed gameplay validation, distinct from the implementation-time agent build and 45-test Automation result above. The acceptance is recorded with Phase 4B.2, without a separate documentation commit. Physical two-PC LAN validation remains pending until the first Development packaged build.

### Retained Phase 4B.1 Manual PIE Validation Checklist

Use two players, Listen Server + Client, on `/Game/Maps/LocalMatch`. Confirm ordinary placement still works and each window shows only its own exact hand plus both public counts. Test all three cards on their owners' turns; verify both owners can use eligible cards, subsequent ordinary play works, and later-phase cards stay visibly disabled without changing the match. Wrong-turn/absent-card/stale/crafted unsupported RPC rejection is covered automatically; there is no user-facing spoof control.

For reproducible hands without waiting for random rewards, reuse the existing server `?Seed=5751` option. No hand-injection/cheat mechanism was added. For normal in-editor PIE, launch Unreal Editor with this one-session engine config override, then start the two-player PIE session:

```text
UnrealEditor.exe "<path-to-project>/Gomokards.uproject" "-ini:Engine:[/Script/UnrealEd.EditorEngine]:InEditorGameURLOptions=?Seed=5751"
```

The installed UE 5.8 `BuildPlayWorldURL` appends `InEditorGameURLOptions` to the PIE map URL. Alternatively, when launching the listen server as a new editor game process, its **Additional Server Game Options** field can supply `?Seed=5751`; that setting is handled by UE's new-process launch path, so do not assume it affects an in-editor host. Do not use the development Restart button before/during the recipe: it intentionally chooses a new seed. Stop/restart PIE to repeat. Remove the optional seed override for ordinary random sessions. The opening/results below are verified by Automation and retained for repeat checks; Phase 4B.1 user manual PIE acceptance is recorded above.

Coordinates are zero-based `(column, row)`, counted from the top-left. In each row, Black plays first, then White:

| Pair | Black placement | White placement | Expected reward |
| --- | --- | --- | --- |
| 1 | `(5,5)` | `(6,5)` | None |
| 2 | `(0,0)` | `(7,5)` | None |
| 3 | `(8,5)` | `(1,0)` | Black: Restock; White: Swap Hands |
| 4 | `(9,9)` | `(10,9)` | None |
| 5 | `(12,12)` | `(11,9)` | None |
| 6 | `(12,9)` | `(5,12)` | Black: Steal |
| 7 | `(6,12)` | `(0,18)` | None |
| 8 | `(7,12)` | `(8,12)` | White: Tetris (held, disabled) |

1. After the opening, Black sees `[Restock, Steal]`, White sees `[Swap Hands, Tetris]`; each sees the opponent count **2** only.
2. Black plays **Restock**: Black's hand becomes `[Steal, Ghost, Steal]`, count **3**; White keeps its own two cards and sees only Black's count change.
3. White plays **Swap Hands**: Black now sees `[Tetris]`, count **1**; White sees `[Steal, Ghost, Steal]`, count **3**. Neither window gains a second opponent-hand display; old own-hand entries are replaced.
4. On Black's turn, Tetris remains disabled with the later-phase explanation; clicking it changes nothing. Black places `(17,17)` normally instead.
5. White plays **Steal**: Black becomes empty (**0**); White sees `[Ghost, Steal, Tetris]` (**3**). No public stolen-card message appears. Continue with an ordinary Black placement and confirm both boards/turns agree. Host and remote client have both played cards through the same UI path.

**Phase 4B.1 user manual PIE validation: PASSED.** Physical LAN remains pending until a Development packaged build; no LAN/Internet validation is claimed or required now. Phase 4B.2 targeted-card and Phase 4B.3 persistent-rule implementations follow below; later Ghost/Tetris networking and Phase 4C Internet/session infrastructure remain pending.


## Phase 4B.2 — Targeted board card networking

**Implemented; automated validation passed; Phase 4B.2 user manual two-player PIE validation: PASSED.** Phase 4A and Phase 4B.1 user two-player PIE acceptance remain passed. This section records the accepted Phase 4B.2 baseline; Phase 4B.3 below extends card availability. This phase adds exactly **Tactical Nuke, Polarity and Barrier**. Together with Restock, Swap Hands and Steal, six cards are network-enabled. Confusion, Back to Basics, Ghost and Tetris remain visible when held but disabled for network play. The full authoritative ten-card pool/probabilities and all Core/Cards source remain unchanged.

### Target intent, authority and projections

The existing non-targeted `ServerPlayCard(Epoch, ExpectedCompletedActions, CardId)` still accepts only Restock/Swap Hands/Steal. New reliable `ServerPlayTargetedCard(Epoch, ExpectedCompletedActions, CardId, TargetX, TargetY)` accepts only Nuke/Polarity/Barrier. Targets are normalized integer intersections/anchors, not raw screen coordinates. No actor ID, client-selected target domain, stone color, random result, board patch or predicted outcome is sent.

GameMode checks assignment, Playing status, epoch, action token and the dedicated target whitelist, derives the actor from its server mapping (with the existing standalone-only fallback), then calls the same `Submit`/Core `ResolveAction` path with `FActionRequest::Play(actor, card, target)`. Core independently checks turn, card ownership, target domain and terminal/effect restrictions. Wrong-RPC requests, the four later cards and malformed IDs reject before mutation. No temporary restriction was added to Core, no generic action/target protocol was created, and the six-card union is used only where the runtime needs overall availability, including the full-board capability guard.

| Card | Target and accepted Core behavior |
| --- | --- |
| Tactical Nuke | One intersection (`FBoard::Contains`, including the final row/column). Removes any stone and sets Empty + forbidden. No new global result evaluation. |
| Polarity | Top-left intersection anchor of a 2×2 region (`ContainsAnchor`, indices 0–17 on each axis). Flips the four stones with existing `OppositeStone`; Empty stays Empty. Existing Core global evaluation may yield Black win, White win, simultaneous-line Draw or no result. |
| Barrier | Visual center between four intersections, normalized by existing `FBoardLayout::TargetAt(..., Barrier)` to the top-left anchor (`ContainsAnchor`). Core `AddUnique` records connectivity blocking without moving stones. Repeating an existing Barrier is still legal and consumes the card/action; no duplicate rejection was introduced. |

Accepted cards use existing action/turn/Confusion lifecycle; fixtures with active Confusion consume one action normally even though Confusion cannot yet be played over the network. These target effects introduce no RNG use. Rejections preserve the complete authoritative match/RNG, revision and public/private projections.

**No new public or private projection fields.** Existing cells/forbidden flags, Barrier anchors, results and public hand counts represent every effect. Each accepted target advances the normal revision and republishes the public snapshot and both exact owner-private snapshots at the same epoch/revision. The actor's played card is consumed by Core; the opponent receives only its own hand. The existing acknowledgement carries no added card/target/effect/hidden-hand data. Coherence and the one-outstanding-request gate are reused, with no prediction or client result simulation.

### Local targeting and previews

The owning controller stores only a local selected card byte, never a selected player ID or a copied rules state. Clicking an eligible targeted card selects it; clicking it again, right-clicking or pressing Escape cancels. Selection, hovering and cancellation send no RPC and consume no action, revision or RNG. The UI restores the accepted geometry: Nuke intersection highlight, Polarity 2×2 outline, and Barrier cross at the cell center.

A board click takes exactly one branch: ordinary placement when unselected, targeted intent when selected. Only sending the intent engages the existing pending gate. Selection clears when sent; a rejected target shows the existing safe error and can be reselected/retried. Invalid geometry yields an invalid coordinate for server rejection, never an ordinary placement fallback. New revisions, resets, loss of coherence/eligibility and other submitted actions clear stale selection. Slate never reads GameMode/`FMatchState` or runs authoritative validation. The old state-based hot-seat targeting helper is not used by this network view.

### Agent validation

Unreal **5.8.2 Win64 Development Editor build succeeded**. The complete exported `Saved/Automation/Phase4B2/index.json` reports **47 passed, 0 failed, 0 test warnings, 0 skipped/not run, 0 in progress**. All 45 accepted groups remain; two older assertion labels now describe the non-targeted input contract rather than calling newly enabled target cards globally disabled. Their assertions were not weakened.

New groups:

- `Gomokards.Phase4B2.TargetRpcAndGeometry`: reflected reliable RPC parameters, exact/disjoint whitelist for all 256 byte values, every intersection/anchor edge, Barrier cell-center normalization and preview-cross geometry.
- `Gomokards.Phase4B2.AuthorityOutcomesAndLocalTargeting`: each target card's assignment/turn/ownership/session/epoch/token/terminal rejection safety; malformed/wrong-RPC requests; full-state Core parity and reversed seats; unchanged RNG and Confusion lifecycle; Nuke removal/forbidden data; Polarity no-result/Black-win/White-win/Draw with terminal rejection; no invented global evaluation for Nuke/Barrier; duplicate Barrier and subsequent authoritative connectivity; both private snapshots/public board coherence; selection/reselect/Escape/right-click cancellation; exclusive ordinary/targeted click routing; rejection retry/reset cleanup; verified deterministic opening below.

The first UI-routing test run exposed that the transient test world had not begun play, so Unreal suppressed `ProcessEvent`. The fixture now scopes Unreal's local editor script-execution allowance around controller RPC-wrapper/Slate-handler checks. This is test-only, with no production authority workaround. It verifies local routing, not network packet transport. A test coordinate also required an explicit `FIntPoint` for compilation. The final full suite above passed after both fixture corrections. No agent-driven gameplay occurred; startup file-journal/layout warnings are outside Automation test results.

Static review: public/private/ack reflected field inventories and owner-only replication remain unchanged and pass the existing privacy tests; no authoritative match/RNG is replicated; no client actor ID/raw pixels enter the target RPC; no effects are duplicated in GameState/Slate; no both-hand HUD, extra client RNG, OnlineSubsystem, new service, map, asset or config change was added. GameState's only change grants the new Automation fixture access. Generated/cache files are excluded. No protection against a malicious authority host is claimed.

### Phase 4B.2 user manual PIE validation — PASSED

The user reports that Phase 4B.2 two-player PIE gameplay validation is completed and all functionality works correctly on implementation commit `b6e1b49534e7d65025c4908a6fe88a7f1523d137`. This user-performed hands-on acceptance is separate from the agent Development Editor build and 47-test Automation result above. The deterministic checklist below is retained for regression checks. Physical two-PC LAN validation remains pending until the first Development packaged build; no Internet validation or malicious-host confidentiality is claimed.

### Retained Deterministic Manual PIE Validation Checklist

First confirm the accepted ordinary/basic-card behavior still works in two-player Listen Server + Client PIE; the retained `5751` recipe above remains available. For targeted cards, start a fresh session with **`?Seed=1294`** using the same existing seed mechanism. For in-editor PIE, launch Editor with:

```text
UnrealEditor.exe "<path-to-project>/Gomokards.uproject" "-ini:Engine:[/Script/UnrealEd.EditorEngine]:InEditorGameURLOptions=?Seed=1294"
```

Use `/Game/Maps/LocalMatch`, two players, Play As Listen Server. Do not use the development Restart button during setup because it chooses a new seed; stop/start PIE to repeat. No hand injection, cheat RPC or grant mechanism exists. The seed/opening/effects are verified through the server adapter by Automation; user hands-on Phase 4B.2 PIE validation has also passed, as recorded above.

Coordinates are zero-based `(column, row)` from the top-left. In each row play Black first, then White; the last row has only Black's move:

| Pair | Black placement | White placement | Natural reward |
| --- | --- | --- | --- |
| 1 | `(5,5)` | `(6,5)` | None |
| 2 | `(0,0)` | `(7,5)` | None |
| 3 | `(8,5)` | `(1,0)` | Black: Tactical Nuke; White: Polarity |
| 4 | `(9,9)` | `(10,9)` | None |
| 5 | `(12,12)` | `(11,9)` | None |
| 6 | `(12,9)` | — | Black: Barrier |

1. After these **11 placements**, Black owns `[Tactical Nuke, Barrier]`, White owns `[Polarity]`; each sees only the opponent's count. White is current. Select Polarity and cancel by reselecting, Escape and right-click: action count remains 11 and hands/board do not change.
2. Reselect Polarity: hover shows a 2×2 outline. Final row/column is not a valid anchor; clicking there must leave state unchanged and show rejection (reselect afterward). Apply at top-left anchor `(5,5)`: `(5,5)` becomes White, `(6,5)` becomes Black, the two empty points remain empty. Both windows agree; turn passes to Black.
3. Black selects Nuke and targets intersection `(6,5)`: the stone disappears and the forbidden mark appears in both windows. White attempts ordinary placement there: reject unchanged; then White places `(15,15)` normally.
4. Black selects Barrier and clicks the **center** between intersections `(9,9)`, `(10,9)`, `(9,10)`, `(10,10)` (board-local pixel `(300,300)` at the existing layout scale). The yellow cross preview becomes the same cyan Barrier on both windows, with all stones intact; turn passes to White.
5. White places `(16,15)` normally. Verify correct action/turn progress, own-hand consumption/public counts and continued ordinary play. Both host and remote have now used targeted cards through the same path. For current regression checks after Phase 4B.3, only Ghost/Tetris remain network-disabled when held; Confusion and Back to Basics are now enabled.

Polarity terminal wins/draws are explicitly covered by Automation; the manual pass need not construct them artificially. **Phase 4B.2 user manual PIE validation: PASSED.** Physical two-PC LAN validation remains pending until the first Development packaged build; it is not a blocker here. Phase 4B.3 Confusion/Back to Basics networking follows below; later Ghost/Tetris networking and Phase 4C Internet/session infrastructure remain unimplemented. No LAN or Internet validation is claimed. The Phase 4B.2 implementation and user manual PIE acceptance are complete; no later-phase work is included.


## Phase 4B.3 — Persistent rule-state card networking

**Implemented; agent automated validation PASSED; Phase 4B.3 user manual PIE validation: PASSED.** Continues from `e68b41dc355c8b310d07be862314f8f48c906fcd`. Phase 4B.2 user two-player PIE validation remains **PASSED** on implementation `b6e1b49534e7d65025c4908a6fe88a7f1523d137`. This section records the accepted Phase 4B.3 baseline; Phase 5A below extends availability. This phase enables exactly **Confusion and Back to Basics**, bringing network-enabled cards to eight. Ghost and Tetris remain visible when held but network-disabled. The full ten-card draw pool and probabilities are unchanged.

### Authoritative rules and the clarified Basics change

- **Confusion remains shared, not per-player.** Activation/recast leaves two future successful action events. Both players' successful placements and card plays consume the existing counter; a placement uses the opposite of the actor's assigned stone without changing their player identity or hand ownership. Selection, hovering, cancellation and rejected actions consume nothing. Recast refreshes to two instead of stacking.
- **Back to Basics disables future card play and clears active Confusion.** This explicit Phase 4B.3 user clarification supersedes Phase 3A.1's previous preservation rule. The minimal Core change clears the counter in `ResolveAction` after normal old-duration processing, so a pre-existing count of two becomes zero. No Confusion/Basics rule is implemented in PlayerController, GameState, Slate or the transport structs.
- Basics consumes itself and completes/transfers one ordinary action. It preserves every board cell and flag, Nuke removals/forbidden points, Barrier anchors, committed Polarity flips, other hand entries and RNG. No rollback, new global win scan or board cleanup is added. Further card requests reject; ordinary placement and natural blocking rewards continue, with earned cards visible but inert until restart. Existing Core tests verify those distinctions.
- Confusion is the active persistent rule counter in this slice. Ghost/Tetris mode restrictions still reject card play during their modes; this work adds no mode networking or generic effect cleanup system.

### Existing intent and projection path

No RPC signature or payload change. Reliable `ServerPlayCard(Epoch, ExpectedCompletedActions, CardId)` now allows exactly **Restock, Swap Hands, Steal, Confusion, Back to Basics**. `ServerPlayTargetedCard` still allows exactly **Nuke, Polarity, Barrier**. Wrong-RPC cards, Ghost/Tetris and malformed IDs reject before mutation. No player ID, effect count, card-lock value, result or RNG is accepted from a client.

The existing GameMode adapter validates assignment, Playing session, epoch, completed-action token and whitelist; the server derives actor identity and sends the existing action to Core for turn/ownership/terminal/card-lock validation and atomic resolution. Both host and remote use this same owning-controller path. Full-board capability handling automatically uses the extended explicit whitelist.

**No new public/private/ack fields.** Existing `ConfusionRemaining` and `bCardsDisabled` reflect committed Core state. Every accepted card updates the normal revision, public board/counts/effects and both owner-private hand projections at a matching epoch/revision. Owner-only hand replication, acknowledgement/coherence and the one-pending-intent gate are unchanged. Full match state and RNG remain server-only; public state and acknowledgements contain no hidden card identities.

The existing projected-state UI enables the two newly supported cards and displays the shared count/card lock. All card buttons become disabled after Basics. Existing selection cleanup removes stale targeting when the committed lock/revision arrives, without spending an extra action. Hover previews are presentation hints; no client predicts authoritative effects or outcomes.

### Agent validation and static review

Unreal **5.8.2 Win64 Development Editor build succeeded**. Exported `Saved/Automation/Phase4B3/index.json` reports **48 passed, 0 failed, 0 test warnings, 0 skipped/not run, 0 in progress**. All previous 47 groups remain. Existing non-target whitelist expectations are updated from three to five; only the superseded Basics/Confusion expectations change in Core tests. All other prior behavior assertions remain.

New `Gomokards.Phase4B3.PersistentAuthorityAndRecipes` covers both new cards' ownership/turn/assignment/session/epoch/action-token/terminal/card-lock rejection safety; full-state/RNG/revision and projection equality on rejection; wrong targeted-RPC use; both controller assignments; exact Core parity; shared action consumption and unchanged identities for both colors; selection/cancel/invalid placement; complete board preservation after actual Nuke/Polarity/Barrier history; Basics clearing, stale-target cleanup, disabled ordinary/targeted card requests, normal placement and restart; and both fixed-seed recipes below. Existing suites retain all-byte whitelist, RPC reflection, private/public/ack field inventory and owner-only replication checks.

Static review confirms no direct Slate-to-GameMode gameplay path, client-side authoritative effect calculation, duplicate persistent state, generic effect framework, new public card events, RNG replication or hidden-hand leakage. Runtime DTO layouts and replication conditions are unchanged; the three owner headers only gain test-fixture access. No assets, config, map references, dependencies, LAN/session services or generated/cache files are included. These agent checks are compiled tests and in-process authority/projection checks, not hands-on PIE, physical LAN or a network packet capture. No agent gameplay was performed.

### Phase 4B.3 user manual PIE validation — PASSED

The user completed two-player PIE manual validation on `ba8f1647bb7731528aaaed045bd327d574cf7898` and reports that all tested behavior passed. This is user hands-on acceptance, separate from the agent's implementation-time build and 48-test Automation result above. No additional build, Automation run or agent gameplay was performed for the documentation/review update. Physical two-PC LAN remains pending until the first Development packaged build.

### Retained Phase 4B.3 Deterministic Manual PIE Validation Checklist

Use `/Game/Maps/LocalMatch`, two players, **Play As Listen Server**, with the existing authoritative seed override. For each recipe start a fresh PIE session using its seed, for example:

```text
UnrealEditor.exe "<path-to-project>/Gomokards.uproject" "-ini:Engine:[/Script/UnrealEd.EditorEngine]:InEditorGameURLOptions=?Seed=182"
```

Use `5211` instead of `182` for recipe B. Do not press development Restart during a recipe, since it chooses a new seed. Stop/start PIE to repeat. No hand injection, cheat RPC or grant mechanism was added. Both recipes were verified by Automation through the production server adapter.

Coordinates are zero-based from the top-left. Each row is Black then White. **Recipe A uses rows 1–6 (12 placements); recipe B uses all eight rows (16 placements).**

| Pair | Black | White |
| --- | --- | --- |
| 1 | `(5,5)` | `(6,5)` |
| 2 | `(0,0)` | `(7,5)` |
| 3 | `(8,5)` | `(1,0)` |
| 4 | `(9,9)` | `(10,9)` |
| 5 | `(12,12)` | `(11,9)` |
| 6 | `(12,9)` | `(5,12)` |
| 7 | `(6,12)` | `(0,18)` |
| 8 | `(7,12)` | `(8,12)` |

**A — Shared Confusion consumption, seed 182:**

1. After 12 placements Black has `[Confusion, Restock]`; White has `[Ghost]`. Each window shows only its own identities and the opponent's hand count. Ghost remains disabled.
2. Black plays Confusion. Both windows show **2**, White to act; player identity labels do not swap. An occupied-cell click must leave board/count/turn unchanged.
3. White places `(15,15)`: a **Black** stone appears identically in both windows, count **2 → 1**, while White retains White identity.
4. Black plays Restock: own hand refreshes, opponent sees only its count; shared Confusion **1 → 0**. White places `(17,17)`: normal **White** stone. Ordinary play continues.

**B — Basics clears rules but preserves history, seed 5211:**

1. After 16 placements Black has `[Tactical Nuke, Back to Basics]`; White has `[Barrier, Confusion]`.
2. Black Nukes intersection `(6,5)`: its stone disappears and the forbidden mark agrees in both windows. White selects Barrier; cancel/reselect/Escape/right-click must spend nothing, then click the cell center between `(9,9)` and `(10,10)` to place it. Both windows retain the surrounding stones and display the same Barrier.
3. Black places `(15,15)` normally. White plays Confusion: both show **2**.
4. Black plays Back to Basics: both show **Basics on / Confusion 0**; the forbidden point, removed stone, Barrier and all other stones remain unchanged. The normal turn passes once to White.
5. White places `(8,9)`: a normal **White** stone earns a new Confusion from blocking. Black places `(17,17)`. On White's turn that Confusion is visible but disabled; clicking it changes nothing. Server-side crafted card rejection is covered automatically. Ordinary placement remains available.

Also repeat the retained Phase 4B.1 `5751` and Phase 4B.2 `1294` recipes as a regression check for Restock/Swap/Steal and Nuke/Polarity/Barrier, including targeting and continued placement. For current regression after Phase 5A, Ghost is enabled and only Tetris remains network-disabled. Automation covers retained Polarity history and terminal outcomes without requiring additional engineered manual fixtures.

**Phase 4B.3 user manual PIE validation: PASSED.** The checklist is retained for regression checks. Physical two-PC LAN validation remains pending until the first Development packaged build. Ghost/Tetris networking and Phase 4C Internet/session infrastructure remain unimplemented. No LAN/Internet validation or malicious-host confidentiality is claimed. Stop at Phase 4B.3.


## Network Architecture Review v1

**Architecture review only — 2026-10-01, source baseline `ba8f1647bb7731528aaaed045bd327d574cf7898`.** Only README changes. No source/header/config/asset/test edits, new build, Automation run or agent gameplay. Ghost/Tetris networking is **not implemented or validated**. The implementation-time 48-test result remains historical evidence; user Phase 4B.3 PIE acceptance is recorded above. Physical LAN remains pending until the first Development packaged build.

### Accepted baseline and observed source facts

The accepted ownership model remains intact: GameMode owns the sole live authoritative `FMatchState`; network-independent Core resolves gameplay; GameState holds public projections; owning PlayerController holds owner-private hand/identity and submits explicit intents; Slate reads projections. Host and remote take the same RPC/adapter route. Assignment, RNG and real-time deadlines stay server-side. Public hand counts and private card contents retain their accepted visibility.

Evidence is in [MatchNetTypes.h](Gomokards/Source/Gomokards/Public/Runtime/MatchNetTypes.h), [MatchNetTypes.cpp](Gomokards/Source/Gomokards/Private/Runtime/MatchNetTypes.cpp), [LocalMatchGameMode.cpp](Gomokards/Source/Gomokards/Private/Runtime/LocalMatchGameMode.cpp), [LocalMatchPlayerController.cpp](Gomokards/Source/Gomokards/Private/Runtime/LocalMatchPlayerController.cpp) and [SLocalMatchView.cpp](Gomokards/Source/Gomokards/Private/Presentation/SLocalMatchView.cpp). The public/private/ack DTOs contain no mode transport yet. GameMode's `Submit`, `PlaceFrom`, `CardFrom` and `TargetedCardFrom` remain the authority path; controller refresh requires matching epoch/revision across public and private views.

**Card architecture is sufficient.** [CardDefinitions.cpp](Gomokards/Source/Gomokards/Private/Cards/CardDefinitions.cpp) uses ten stable IDs and target metadata, not a type hierarchy. [ExecuteCardEffect](Gomokards/Source/Gomokards/Private/Cards/CardEffects.cpp) explicitly dispatches effects; [ResolveAction](Gomokards/Source/Gomokards/Private/Core/MatchRules.cpp) owns validation, atomic candidate commit, action completion and mode-entry ordering. Immediate, targeted, persistent-rule and mode-transition cards are useful descriptions, not new classes. Existing non-targeted/targeted whitelists are five/three cards; mode activation cards already have non-targeted Core definitions. **No CardEffect base framework, generic network Action Request or generic Status/Effect container is needed.** The ordinary RPC can later admit each mode card explicitly while preserving its existing guards.

**Existing mode boundaries are adequate:**

| Mode | Actual Core/runtime sequence |
| --- | --- |
| Ghost | Card effect enters Preparation; resolver consumes card, progresses old Confusion, completes one action and transfers. Server ticker waits five real seconds, then `BeginGhostHidden` changes phase/count without an action. Hidden accepts legal placements, writes true effective color, rewards the acting player and consumes Confusion; first five suppress line wins. Sixth placement clears Ghost and evaluates both colors globally in the same candidate commit; continuing play transfers once, terminal play does not. |
| Tetris | Resolver completes/transfers the casting action before `BeginTetris`; the opponent operates first. `SpawnOrSkipTetris` draws one shape per opportunity, chooses a legal spawn and skips impossible opportunities without writing cells. Six opportunities alternate operator; block color is opposite assigned color, independent of Confusion. Server 0.5-second gravity moves or locks; lock writes cells, clears the union of connected five-plus lines, then spawns/skips the next opportunity. `FinishTetris` clears mode before global result/no-action evaluation; ordinary current player is unchanged during the mode. |

Evidence: `MatchRules.cpp` functions `ValidateAction`, `ResolveAction`, `BeginGhostHidden`; [TetrisRules.cpp](Gomokards/Source/Gomokards/Private/Core/TetrisRules.cpp) functions `BeginTetris`, `ApplyTetrisInput`, `StepTetrisGravity`, `SpawnOrSkipTetris`, `FinishTetris`; GameMode timer functions. **Tetris movement, rotation, gravity, locks and skips never increment ordinary `CompletedActions` or consume Confusion.** Only the casting card is an ordinary action. Thus even block locks cannot use that counter as a piece identifier.

**State invariants do not require a unified phase enum.** `ValidateAction` prevents ordinary cards during Tetris, all gameplay during Ghost Preparation, and cards during Hidden; `BeginTetris` rejects any Ghost phase. Both modes cannot overlap through legal live actions. Normal Won/Draw exits clear the mode before publication. However, do not assert that every stopped result implies no mode fields: `ResolveAction` can enter `AwaitingRuleDecision` if no placement remains before the sixth Hidden placement, retaining Hidden; disconnect also retains mode fields while SessionEnded disables input. Result/session must dominate input, and Hidden redaction must remain until actual reveal/reset. An independent Basics lock plus mode is unreachable via ordinary current card play but deliberately supported by trusted fixtures; mode exit must not clear the lock. Public writable value fields permit such fixtures, not a client mutation route. These are explicit cross-field invariants, not evidence for a ModeManager or phase-enum migration.

### Review recommendation: no separate pre-refactor

**Decision A: no standalone structural refactor is required before Ghost.** Keep explicit mode fields/state machines in `FMatchState`, server runtime timers and deterministic Core transitions. The user accepted this no-pre-refactor conclusion for Phase 5A. The review below remains a historical design record; the Phase 5A and Phase 5B implementations are documented separately at the end.

There are concrete integration omissions to fix **within the respective mode phase, before enabling its network whitelist**:

1. `PollGhostPreparation`, `PollTetrisGravity` and `SubmitTetrisAt` currently only broadcast `OnMatchChanged` after mutations. They do **not** call `PublishViews`; the network Slate subscribes to controller projection refresh, not that delegate. Wire each authoritative transition to the appropriate projection publication, once, without extra action/RNG consumption.
2. `PublishViews` currently ends the session when neither an ordinary placement nor a whitelisted card is available. During Preparation or active Tetris, neither is the next permitted operation even though a server transition is pending. On a full/forbidden board this can cancel a newly scheduled mode timer. Make this runtime capability check mode-aware; do not alter Core no-legal-action rules or invent a win/draw. At Hidden entry with no legal placements, retain explicit stopped/no-action handling rather than leaving an interactive dead end. Test both zero space at entry and exhaustion before placement six; do not silently reveal early.
3. The current public converter sends true colors unconditionally, and UI eligibility knows no mode phase. Ghost needs redaction and projected input gating; Tetris needs separate operator authorization and presentation. Simply whitelisting either card would be incorrect.

These omissions affect unexposed modes, not the accepted eight-card pipeline. No broad ownership repair is indicated.

### Proposed Ghost transport, clock and UI changes — Phase 5A

**Redact on the server before replication.** Extend the display-cell encoding to distinguish Empty, visible Black, visible White and occupied-hidden, independently of Core `EStone`. Existing `Stone` byte can carry a small explicit display enum; no second true-color property is required. During Hidden map **every occupied cell** to occupied-hidden, retain Empty/forbidden and Barrier data. Current `StoneDisplayColor` and `GhostTests.VisibilityAndPool` gray all occupied stones, including pre-Ghost stones, not only new placements. Preparation keeps their colors visible; remembering prior colors or inferring new ones from public turns/Confusion cannot be undone and is not transport secrecy.

The server retains all true colors. Neither owner-private view needs hidden colors. Do not add them to acknowledgements, logs, labels, previews or reward payloads. Confusion still changes the Core placement color, but the transported cell and hover preview remain hidden/gray. Shared Confusion count and player identity stay public. Blocking reward continues to expose the accepted public count increase and only the recipient's private card identity; no opponent draw event.

Add to the coherent public snapshot only **Ghost phase, Hidden placements completed (0–6, remaining derived), and Preparation display-end time**. Cells use the revised encoding; existing session/result, counts and board metadata suffice. Preparation-to-Hidden must publish a fresh public revision **and both private revision stamps**, despite unchanged hands/actions, or the current controller coherence check will stall. Sixth placement publishes revealed cells, phase exit, turn, result and reward views as one logical commit. Transport across Actors is not atomic: retain epoch/revision matching, tolerate arrival order/coalesced intermediate revisions, and discard old epochs. On observing a newer raw public Hidden snapshot, mask stale display colors and disable incoherent input immediately even while awaiting private catch-up; never merge cached true colors back into the new hidden cells. Replace display cache on coherent reveal, without a separate reveal event/RPC. A newly joining unassigned observer must also only receive the redacted public projection.

**Reuse `ServerPlaceStone`.** Core already enforces Preparation freeze and Hidden placement/card rules. Add phase-aware projected gating to controller/Slate and clear selection, but keep independent server checks. No Ghost placement RPC, client timer transition or fake partial `FMatchState`.

**Clock-domain finding:** GameMode uses `FPlatformTime::Seconds()` and Core ticker, independent of pause/dilation. Installed UE 5.8 `GameStateBase.cpp::GetServerWorldTimeSeconds` estimates `World->GetTimeSeconds`; `World.h` specifies that this is paused and dilated/clamped. These timestamps cannot be directly subtracted. Keep the authoritative monotonic deadline. For the first no-pause, 1x network slice, publish synchronized world time plus server remaining real duration as the display deadline; derive the display locally and clamp at zero. World-time display is approximate under stalls and not a general pause/dilation solution. If that slice must support pause/dilation, use a narrow real-time display anchor/rebase instead; do not change the server's five-real-second rule. Zero displays waiting for the server phase, never permission to play. No per-frame countdown replication or new clock service.

Adapt the formatting of `GhostLabel`, `StoneDisplayColor`, `EffectLabel` to explicit display phase/cells/count and a local display duration. Their existing full-state overloads can remain for Core/local tests. The projected view must not call them with a fabricated match. Extend the current painter's two-color branch to handle occupied-hidden explicitly.

### Proposed Tetris transport and authorization — Phase 5B

Use one explicit reliable **ServerTetrisInput** on the owning controller, carrying epoch, mode activation token, block opportunity and validated absolute direction/rotate. Server maps connection/controller to assigned gameplay ID and compares it to `Players[Tetris.OperatorIndex].Id`; ordinary current player, index, stone color and Host status are not substitutes. Check Playing session, in-progress result, active mode, all tokens and enum before `SubmitTetris`. That existing runtime method has no caller/actor argument and must not become an unguarded RPC. Gravity remains server-only, never a client input.

Reuse Core movement exactly: absolute arrows, reject the direction opposite gravity, rotation without kicks; only successful gravity-direction manual movement resets the real deadline to 0.5 seconds. Perpendicular moves/rotation do not; rejected input changes no state/RNG/deadline. Only blocked **automatic** gravity locks. No client collision/result simulation.

**Smallest transport seam:** keep board/hands/result on the existing coherent snapshot and add a small companion public **Tetris pose projection on the same GameState**, not a second authority or replicated core state. The main snapshot needs mode-active state and an activation identity. Pose needs **epoch, activation identity, required board revision, pose revision, block opportunity, operator gameplay ID, shape, rotation, origin, edge and current block color**. Edge determines gravity; do not replicate both. Active state can be taken from the coherent main snapshot rather than duplicated in pose. No future shapes, RNG, spawn candidates, collision map or gravity deadline is needed by clients.

Publish full coherent main/private views on activation, lock/clear/skip progression, exit, reset and session end. Move/rotate/gravity-without-lock publishes only pose and increments its own revision; it does not advance main revision or republish hands. Render/enable controls only for pose matching epoch/activation and the currently coherent required board revision. Buffer the newest future-board pose until that board arrives; discard stale pose/old activation. When a newer main board or mode exit arrives, suppress any incompatible old overlay immediately; do not draw a previous block over its newly committed cells. This avoids both pose/board races and coupling every key to the existing public/private equality barrier. No FastArray/custom serialization is justified yet.

**Stale input and acknowledgement:** retain ordinary epoch/action token/one-pending-intent/ack unchanged for card activation and normal play. A Tetris activation can use its captured post-cast `CompletedActions` within epoch as a unique activation token (it remains fixed for that mode); pair it with `BlockNumber` to reject keys delayed into the next opportunity or a later cast. Do not increment ordinary actions for keys/locks or compare pose revision as the input token, since automatic gravity would unnecessarily invalidate legitimate input for the same piece. A small Tetris-specific result/ack, if used, references epoch/activation/block/pose revision and safe rejection reason; it must not feed the ordinary `bPending`/main-revision acknowledgement path. Reliable per-controller ordering plus bounded key repeat suffices; avoid per-key round-trip gating or a generic request sequencer. Validate stale rejects without resetting gravity, RNG or operator.

Adapt `TetrisLabel` to public operator ID/seat data and pose; reuse pure `TetrisOffsets`, `TetrisInputForKey`, `TetrisGravity` and board geometry for painting/controls. Display square committed stones while active, current shape outline/color and operator. Keep `TetrisFits`, spawn choice, gravity, line clearing, `ResolveAction` and result evaluation server/Core-only. Clear controls/overlay on mode exit, incoherence, result, disconnect and reset. No client `FMatchState` copy.

Pure server control is a reasonable first implementation for two players and 0.5-second gravity, not a latency guarantee. Measure remote key-to-authoritative-pose delay, key-repeat backlog and missed intended moves near lock on the packaged LAN build; later repeat under actual Internet RTT/loss. Persistent control frustration after fixing publication rate/input bounds would justify a separate prediction proposal. Do not add prediction, rollback or interpolation now.

### Required future transport versus optional state

| Surface | Required proposal | Not required |
| --- | --- | --- |
| Ghost public | Display-cell hidden encoding; phase; Hidden placement count; one Preparation display deadline | True hidden colors anywhere client-visible; per-player effect ownership; effect arrays; per-frame countdown |
| Tetris main public | Mode active + activation identity, alongside existing board/result/session/epoch/revision | Future shapes; duplicated gravity vector; debug spawn candidates |
| Tetris companion public | Current pose/operator/opportunity plus epoch, activation, board and pose revision correlation | RNG, client gravity timer, predicted board or second authoritative state |
| Owner-private | **No new secret gameplay fields**; keep own hand/identity/admin and coherent revision stamps | Ghost true colors; operator-only hidden shapes; opponent cards |
| Ordinary ack | Existing shape sufficient for Ghost and mode-card activation | Color/result patches, secret rewards, mode pose payloads |

### Two-player, standalone and dedicated-server review

| Assumption | Classification and implication |
| --- | --- |
| Black/White, two assigned opposite stones, `SingleOpponentIndex`, swaps/steals, alternating Tetris operators | **A — current product rule.** Core validates exactly two players; retain it. The header comment saying all two-player assumptions live in one helper is broader than the actual code. |
| `Join` two-seat cap/start threshold, first-free connection-order assignment, no reconnect/spectator seat, occupied-seat projection | **B — localized runtime policy.** IDs are mapped explicitly; connection order does not confer gameplay authority. Third connections remain unassigned; their public transport must still respect Ghost redaction. |
| `IsPresentationReady` requires two seats, public seat/hand-count rendering, two-owner publication/tests | **B — localized current projection/UI contract.** The public seat array and Core player collection are dynamic; each private view contains one identity and its hand. This is not an implicit four-player implementation. No N-player generalization now. |
| Reusing ordinary current-player or Black/Host identity for Tetris control | **C if introduced — wrong authorization layer.** No network Tetris input exists yet; prevent this in Phase 5B with the operator-ID check. No existing eight-card assumption requires correction first. |

Standalone already uses the same adapters/Core, with current-player fallback restricted to explicitly initialized `NM_Standalone` plus local controller; listen/dedicated must never use it. `PublishViews` currently switches the standalone private view to ordinary current player. Tetris's active operator differs, so standalone control eligibility must use the mode's operator, with an equally narrow local fallback; do not infer it from the current private hand or add a Host shortcut. It may retain the ordinary player's hand display while controls use operator, since cards are disabled in-mode. No separate standalone rules route is necessary. Both modes are currently disabled in the projected controller card path even though their Core/runtime functions remain available to tests.

No Phase 4B.3 gameplay depends on a viewport: GameMode resolves server-side, controller UI creation requires local controller/non-dedicated/viewport, and private replication remains owner-only. Listen Host uses ordinary intents and explicit local RepNotify refresh; its only extra permission is development restart. Dedicated mode intentionally grants no local Host restart admin; deployment-time administration is separate from gameplay. **Source-compatible direction, not dedicated validation:** only Game/Editor targets exist, and the module still unconditionally depends on Slate/SlateCore. A future Server target/build must verify UI compilation/dependency guards; no server package/headless smoke has been produced. These are deployment checks, not grounds to redesign gameplay ownership now.

### Failure modes, priorities and next-phase acceptance

**REQUIRED NOW / before implementation:** record the user PIE acceptance and this review; preserve the explicit authority/privacy invariants. Choose no standalone pre-refactor. Treat hidden-color redaction, timer publication, mode-aware capability checks and operator/piece identity as non-negotiable design constraints. No source correction is authorized in this review.

**IMPLEMENT DURING GHOST/TETRIS, before enabling each mode:**

- Ghost: prevent true colors leaking through public cells, stale cache, preview or extra payload; stamp timer transitions across both views; publish sixth-placement board/reward/result coherently; keep local countdown zero non-authoritative. Handle full-board/no-action state without cancelling valid Preparation or inventing reveal/winner. Keep stopped Hidden state redacted until a real reveal/reset.
- Tetris: reject wrong operator/stale activation or opportunity; prevent client gravity and input-triggered locking; keep timer generations/cancellation on restart, disconnect and destruction; correlate pose with committed board and remove stale overlays/input. Test coalesced/skipped opportunities and old pose/acks after exit/recast.
- Both: input/public/private/ack delivery order, reset while a timer or input is pending, local Host and remote paths, and only owner-private reward contents. Generation guards already exist; projection publication and mode-specific boundary tests still need implementation.

**LATER:** Development packaged build, two-physical-PC direct-address LAN validation; dedicated Server target/build/headless deployment checks; then Internet/session-provider work and measurements that might justify latency improvements. No LAN/Internet or malicious-listen-host confidentiality claim.

**DO NOT BUILD:** CardEffect hierarchy, generic Action/Status/Effect container, ModeManager/subgame subsystem, StateTree/GAS, generic request sequencing, four-player abstraction, FastArray/custom protocol, prediction/rollback/interpolation framework, or session services in these mode phases.

**Proposed sequence:** Phase **5A — Ghost Networking**, Phase **5B — Tetris Networking**, Development packaged build, two-PC LAN validation, later Internet/session-provider work. Ghost first reuses ordinary placement/ack flow and validates redaction plus asynchronous timer publication before adding Tetris operator and high-frequency pose transport. No technical reason to reverse that order or rename phases.

- **Phase 5A scope/acceptance:** enable Ghost via ordinary card RPC, keep Tetris disabled; redacted public cells/phase/countdown; five-real-second server Preparation and coherent Hidden/reveal; same ordinary placement RPC. Automated transport-value tests must prove all occupied Hidden cells carry no true color in public/private/ack, including Confusion placements and unassigned observers; cover wrong/stale input, Preparation freeze, six placements/rewards, all reveal results, full-board/no-action behavior, both arrival orders, reset/disconnect/timer generations and eight-card regressions. Build/full Automation, then deterministic user two-window PIE checklist for memorization/freeze/gray cells/count/reveal/results/restart. A visual gray screenshot alone is not privacy evidence. Manual user pass remains the acceptance gate unless explicitly waived.
- **Phase 5B scope/acceptance:** enable Tetris activation via ordinary RPC plus typed operator-input RPC and correlated public pose; server gravity only. Tests must cover reversed seats/White Host, every operator opportunity, stale keys from prior block/cast/epoch, all four gravity directions and opposite-input rejection/deadline invariance, rotation/collision, blocked automatic lock, clear/skip/exit/no-action results, unchanged ordinary action/Confusion on mode operations, preserved board effects, out-of-order pose/main/private/ack, lifecycle cleanup and all prior regressions. Build/full Automation, then deterministic user PIE for both operators, visual board/pose agreement, control/soft-drop timing and exit/restart. Follow with packaged two-PC LAN observation; do not claim prediction is necessary or LAN passed before observing it.


## Phase 5A — Ghost Networking

**Implemented; Phase 5A user manual PIE validation: PASSED** on `fe6645da55808f7b2c1e91879ad0baee71b1d3c6`, as reported by the user. The following records the accepted Phase 5A baseline; Phase 5B below extends Tetris availability. Continues from accepted architecture-review commit `a387d2b60373fceaddb75ce61856f026716f275e`; Phase 4B.3 user PIE acceptance remains **PASSED**. Network Architecture Review v1's no-independent-pre-refactor conclusion is accepted and followed. Exactly one existing mode/card, **Ghost**, is newly network-enabled; Tetris remains pending Phase 5B. No Core/Cards gameplay source, ten-card draw pool, assets, config or session infrastructure changed.

### Intent, lifecycle and transport privacy

The existing reliable `ServerPlayCard(Epoch, ExpectedCompletedActions, CardId)` now accepts six non-targeted cards: Restock, Swap Hands, Steal, Confusion, Back to Basics and Ghost. Targeted RPC remains exactly Nuke/Polarity/Barrier. Ghost through targeted RPC, Tetris through either, and malformed IDs reject without mutation. No new RPC or client-supplied player ID, phase, color, timer or result was added. Existing server assignment/session/epoch/token validation and Core turn/ownership/lock checks remain authoritative.

Ghost follows the unchanged Core sequence: card consumption, one completed action/normal transfer and old Confusion consumption enter Preparation; five real seconds later the server enters Hidden without a completed action; six successful ordinary placements use the same `ServerPlaceStone` path. Hidden placements use Core effective colors, rewards and Confusion progression. Placements 1–5 suppress line wins; placement 6 clears Ghost and performs existing global result evaluation, including Barrier connectivity. The final placement, possible reward, revealed board, phase exit, result, counts and turn are one committed revision, with both private hand views stamped to match. No separate reveal event or extra turn switch.

`FMatchPublicView` gains only **GhostPhase, GhostPlacementsCompleted and GhostDisplayEndServerTime**. Display cells explicitly distinguish Empty/Black/White/**HiddenOccupied**; the display enum is separate from Core `EStone`. `MakePublicView` replaces every non-empty stone with HiddenOccupied during Hidden **before replication**, including pre-existing stones and Confusion-generated stones. Empty points, forbidden flags, Barriers, public seat identities and shared effect/count information remain public. The server retains the exact true board. Public cells are a lossy projection, not a gray overlay over transported true colors.

**No owner-private or acknowledgement fields are added.** Neither carries board colors or RNG. A Hidden blocking reward gives exactly one card to the acting owner's private hand; the opponent sees only the accepted public count increase. Existing visibility through remembered Preparation colors or inference from public turns/Confusion is not erased; hostile listen-server memory inspection is outside scope. Newly unassigned clients receive the same redacted public board and no private hand, without adding spectator/reconnect support.

### Timer publication, display and session availability

The authoritative timer remains the existing weak Core ticker plus `FPlatformTime::Seconds()` five-real-second deadline/generation. At successful Preparation-to-Hidden transition, the callback now calls `PublishViews` before the retained local notification: one fresh revision and rebuilt public plus **both** private snapshots, even though hands/action count did not change. No client acknowledgement is generated for the timer transition. Reset, disconnect, EndPlay and destruction keep their existing timer cancellation/generation behavior.

Preparation publication converts current remaining authoritative real duration into a **synchronized server-world-time display endpoint**. Controller display subtracts GameState's synchronized world time and clamps at zero; no raw monotonic timestamp is transported. Countdown text is locally evaluated, without per-frame replication. Displayed zero remains frozen/waiting until the server publishes Hidden. Pause/time-dilation are not supported gameplay features in this network slice; display approximation under stalls does not change authoritative real-time rules.

The runtime capability guard now exempts Preparation because it awaits the server timer, rather than ending the session for lack of ordinary input. Hidden stays Playing while legal placements exist even though cards are restricted. Pathological no-progress behavior is explicit and unchanged in Core: if Hidden entry has zero legal points, the existing runtime guard ends the session with no invented Core winner/draw or early reveal; if a successful Hidden placement exhausts points before six, Core retains Hidden with `AwaitingRuleDecision/NoLegalAction`. Such stopped boards remain color-redacted until reset; no pass or new adjudication rule is introduced.

### Projected presentation and coherence

Slate reads public/private DTOs only. Preparation shows true visible colors, a memorization/countdown label and frozen gameplay controls; local board clicks send no intent. Server independently rejects crafted placement/non-targeted/targeted requests through existing Core errors. Hidden shows all occupied cells and the placement preview in neutral gray, remaining placement count and disabled card buttons; only ordinary current-player placement is enabled. Core still computes the real Confusion color; UI never reveals it in the Hidden preview. Unknown display bytes render transparent rather than accidentally White.

Projection updates clear stale targeted-card selection without another action. Public/private epoch/revision matching continues to gate interaction. If a newer public Hidden snapshot arrives before private catch-up, the controller immediately masks the old visible display cache and disables interaction; it does not retain a true-color cache for reconstruction. Sixth-placement reveal waits for coherent public/private data and replaces the display from the server snapshot. Existing ordinary acknowledgement buffering/coalescing remains unchanged. No fake client `FMatchState`, direct Slate-to-GameMode call or client phase/result simulation.

### Agent validation

Unreal **5.8.2 Win64 Development Editor build PASSED**. The complete `Gomokards` Automation export (`Saved/Automation/Phase5A/index.json`) was inspected: **49 passed, 0 failed, 0 test warnings, 0 skipped/not run, 0 in process**. All 48 existing tests remain, with only the expected whitelist/transport-inventory fixture updates; one Ghost integration test was added. An initial run exposed a lifecycle-test fixture lookup after simulated disconnect; the fixture was corrected, then the Editor target rebuilt and the complete suite rerun successfully.

New `Gomokards.Phase5A.GhostTransportAuthorityAndLifecycle` exercises assigned ownership and reversed seats, Waiting/Ended/terminal/Basics/stale requests, exact RPC whitelists and malformed-byte rejection, Preparation freeze/countdown-zero behavior, deterministic pre-deadline/deadline publication without sleeping, transport-level redaction of both old colors and new Confusion stones, no private board data, unassigned-client privacy, Hidden legality/counts and private rewards, first-five win suppression, sixth-placement Black/White wins/Draw/Barrier-blocked continuation, arrival-order coherence and reveal, no-progress boards, seed 182 below, restart/disconnect/EndPlay stale callbacks. Existing privacy reflection checks retain the exact private/ack inventory and owner-only replication while allowing only the three new public fields.

Static review: hidden true colors are removed in the server projection function; full `FMatchState` and RNG remain unreplicated; timer transitions publish snapshots; ordinary placement RPC is reused; no mode framework, Tetris RPC/pose/operator networking, OnlineSubsystem, packaging or session-provider work was added. Agent validation is source/reflection and in-process server/projection testing, not real remote packet capture or hands-on PIE. No agent-driven gameplay occurred. Generated build/log/report/cache files remain excluded from the commit.

### Deterministic Manual PIE Validation Checklist — user passed (retained for regression)

Use `/Game/Maps/LocalMatch`, two players, **Play As Listen Server**, with a fresh authoritative **`?Seed=182`** session. Reuse the existing in-editor launch override:

```text
UnrealEditor.exe "<path-to-project>/Gomokards.uproject" "-ini:Engine:[/Script/UnrealEd.EditorEngine]:InEditorGameURLOptions=?Seed=182"
```

Do not press development Restart during acquisition because it randomizes the seed; stop/start PIE for a fresh deterministic replay. Coordinates are zero-based from top-left, each row Black then White:

| Pair | Black | White |
| --- | --- | --- |
| 1 | `(5,5)` | `(6,5)` |
| 2 | `(0,0)` | `(7,5)` |
| 3 | `(8,5)` | `(1,0)` |
| 4 | `(9,9)` | `(10,9)` |
| 5 | `(12,12)` | `(11,9)` |
| 6 | `(12,9)` | `(5,12)` |

After 12 placements Black owns `[Confusion, Restock]`, White owns `[Ghost]`, and Black is current. Ghost is visible only in White's private hand and cannot be played on Black's turn.

1. **Activation/freeze:** Black plays Confusion (count 2); White plays Ghost (count 1, Black next). Both windows show Preparation with true board colors and roughly matching five-second countdowns. Clicking the board or cards spends no action. At displayed zero, controls remain governed by the server's phase, not local time; the automation check covers that authority distinction.
2. **Hidden:** all old stones turn gray in both windows. Hover shows gray, never Black/White; card controls remain disabled. An occupied or out-of-board click must leave action/count/turn unchanged.
3. **Six placements:** play the sequence below. Both windows should count down 6 to 0, with normal alternating ownership. First move uses the remaining Confusion: Black's authoritative stone is White, but stays gray until reveal. It gives Black exactly one **Restock** reward, producing `[Restock, Restock]`; White sees Black's count rise to 2 without a card identity. Confusion becomes 0.

| Hidden move | Player | Point | Color visible only after reveal |
| --- | --- | --- | --- |
| 1 | Black | `(8,9)` | White (Confusion) |
| 2 | White | `(15,15)` | White |
| 3 | Black | `(17,17)` | Black |
| 4 | White | `(15,17)` | White |
| 5 | Black | `(17,15)` | Black |
| 6 | White | `(16,16)` | White |

4. **Reveal/continue:** after move 6 both views reveal the complete true board with Ghost inactive, matching hand counts/result and Black to act. This recipe is nonterminal. Black can place `(18,0)` normally. Automated fixtures separately verify both winners, simultaneous Draw and Barrier-blocked reveal.
5. **Restart:** repeat the fresh seeded acquisition and activation, then use the authorized Host development restart during Preparation. Board/hands/effects reset; waiting beyond the old deadline must not reactivate Ghost. Repeat with disconnect during Preparation if checking lifecycle behavior; remaining player shows SessionEnded without a winner. A new session is required afterward.
6. **Regression:** original eight cards still work in ordinary mode. If Tetris is held, it stays visible but disabled; the retained `5751` recipe obtains it deterministically. No Tetris controls or pose networking are present.

**Phase 5A user manual PIE validation: PASSED.** The user completed the two-player Ghost checklist on `fe6645da55808f7b2c1e91879ad0baee71b1d3c6` and reported all tested behavior passed. This is user hands-on validation, separate from the agent Automation results above. Tetris availability is superseded by Phase 5B below. Physical two-PC LAN still awaits a Development packaged build; Internet/session-provider work remains later. No LAN/Internet acceptance or malicious-host secrecy is claimed.


## Phase 5B — Tetris Networking

**Implemented; Phase 5B user manual PIE validation pending.** Continues from accepted Phase 5A implementation `fe6645da55808f7b2c1e91879ad0baee71b1d3c6`. The user's Ghost two-player PIE validation is **PASSED**. Network Architecture Review v1's no-pre-refactor conclusion remains accepted. All **ten** existing cards are now network-enabled; earlier phase statements that Tetris is disabled are historical. Core/Cards rules, draw pool, assets and config are unchanged.

### Card intent and operator authority

Tetris uses existing `ServerPlayCard` alongside Restock, Swap Hands, Steal, Confusion, Back to Basics and Ghost (seven non-targeted cards). Tactical Nuke, Polarity and Barrier remain the three targeted cards. Wrong-boundary and malformed IDs reject before mutation.

One new explicit reliable Server RPC, `ServerTetrisInput(Epoch, ActivationToken, BlockNumber, EMatchTetrisInput Input)`, accepts only Up/Down/Left/Right/Rotate. The server checks assignment, Playing session, epoch, active mode, activation token, block number and **`Players[Tetris.OperatorIndex].Id`**, then invokes existing Core input validation. Neither ordinary current player nor Host status grants operator authority. The only hot-seat fallback is the existing explicitly initialized standalone/local-controller pattern; listen/dedicated sessions cannot use it. No client-supplied player/operator, pose, shape, color, gravity or timer values.

A server-owned activation serial changes on each cast and reset. Epoch rejects old-match input, activation token rejects previous casts in the same match, and BlockNumber rejects delayed keys after a lock or skipped opportunity. Mode/session gates invalidate input after exit/disconnect. Keys do not use ordinary `bPending`, spend an action, or receive ordinary action acknowledgements: the authoritative pose is their result.

### Public projection and publication

The ordinary public DTO gains only **`bTetrisActive`**, coherent with board/result/effects/turn. Existing GameState gains one separately replicated **`FMatchTetrisPose`**:

| Fields | Purpose |
| --- | --- |
| `bActive`, `Epoch`, `BoardRevision` | Whether this piece belongs to the displayed committed board |
| `ActivationToken`, `PoseSequence`, `BlockNumber` | Current cast, latest pose and current opportunity |
| `OperatorPlayerId`, `Shape`, `Rotation`, `Origin`, `SpawnEdge`, `Stone` | Current public piece and its controlling player; gravity derives from edge |

Pose contains no full board, private hand, RNG, future shape/spawn or deadline. No private-hand or acknowledgement field was added. The server retains full `FMatchState`, assignments, RNG and timer state.

- **Pose-only:** accepted translation/rotation or unblocked automatic gravity publishes just the pose and advances PoseSequence. Normal Revision, all 361 public cells and both private hand projections remain unchanged. No ordinary actions/rewards/Confusion progression. Slate invalidates board painting without rebuilding hand buttons for these notifications.
- **Full commit:** activation, automatic lock/clear/next block/skips/exit publish one normal Revision, the board/result/mode gate and both private projections at that Revision, then pose referencing the new BoardRevision. Gravity compares pre/post opportunity, active flag and committed board; the Core step's boolean alone is not treated as a lock signal.
- **Arrival order:** controller retains only the latest epoch/activation/pose sequence. It renders a piece only when public/private snapshots are coherent and pose Epoch/BoardRevision match. Future pose waits; newer board suppresses old piece. Coalesced sequences require no intermediate frames. An inactive public gate or ended session immediately suppresses stale active pose; no reconstruction or prediction.
- **Host refresh:** both public and pose publication explicitly invoke local refresh as well as RepNotify for remote clients. No separate Tetris actor, manager or mode framework.

### Preserved gameplay and rendering

Tetris card consumption, one CompletedActions increment, one ordinary turn transfer and one existing Confusion decrement happen through the unchanged common resolver. First operator is the caster's opponent. Six opportunities alternate operators; each uses the opposite of its operator's assigned stone, ignoring Confusion. Movement/lock neither changes the ordinary player nor spends Confusion/RNG/rewards beyond the existing server spawn draws.

Server retains its **0.5 real-second** gravity ticker. Absolute arrows remain absolute; the directly opposite gravity direction rejects (Top: Up; Bottom: Down; Left: Left; Right: Right). Successful gravity-direction input resets the deadline to server Now + 0.5; perpendicular movement and clockwise rotation preserve it. Rejection preserves all authoritative state, timer, Revision and PoseSequence. No wall kicks and no lock from blocked manual input. Only blocked automatic gravity locks.

Existing six-shape/four-edge preferred-clearance/fallback RNG logic is unchanged. Impossible spawns skip opportunities, alternating operator without writing/clearing cells or inventing a top-out loss. An all-skipped cast publishes only its final inactive state. Lock clears qualifying connected same-color runs of five or more, respecting Barrier connectivity; forbidden flags and unrelated stones remain, with no collapse. Sixth completion clears first, exits, evaluates the global result and performs no extra ordinary transfer. If still InProgress, ordinary play resumes for the post-card player.

Active Tetris bypasses the ordinary-capability SessionEnded guard because server gravity/operator input provides progress. After exit existing Core result/no-action semantics apply. Ordinary placement and both card RPC boundaries reject during Tetris; UI disables these interactions and clears targeting. Basics prevents activation; Ghost and Tetris do not combine.

Projected Slate rendering shows committed stones as squares while active and the current piece as colored squares with an amber border in both windows. On exit, committed stones return to round rendering. Status shows opportunity N/6, operator identity/stone, block color, edge/gravity and controls. Only the operator's local controller sends keys. Geometry uses the existing pure offset/key helpers; Slate never accesses GameMode or creates a client match state.

Restart cancels gravity, changes epoch/token and publishes inactive pose with reset state. Disconnect cancels gravity and publishes SessionEnded/inactive pose without inventing a winner. Old generations cannot commit; existing EndPlay/BeginDestroy cancellation remains intact.

### Agent validation and static review

Unreal **5.8.2 Win64 Development Editor build PASSED**. The complete exported `Saved/Automation/Phase5BFinal/index.json` was inspected: **50 passed, 0 failed, 0 test warnings, 0 skipped/not run, 0 in process**. All test entries report Success with zero warning/error counts. No agent-driven gameplay was performed.

The original **49 Automation groups are retained**; existing whitelist/reflection fixtures are updated for Tetris enablement and the one additional public pose property. New `Gomokards.Phase5B.TetrisAuthorityPoseAndLifecycle` covers card restrictions/malformed IDs; both seat mappings and operator alternation; stale epoch/cast/block keys; four-edge rejection and timer invariance; clockwise/no-kick rotation and Barrier non-collision; pose-only versus full commits; exact Core state/RNG parity over six opportunities; line clears and preserved forbidden cells; skipped/fallback spawns and global outcomes; board/private/pose arrival orders and stale/coalesced sequences; same-match recast, reset/disconnect/EndPlay; and the deterministic recipe below. Prior Ghost transport-redaction/timer/reveal tests remain included.

Static review confirms the only client mode-input path is the typed controller RPC; server-only Core/gravity/spawn/result authority, no prediction, no full snapshot on pose-only moves, matching board/pose revisions on locks, unchanged owner-only hand transport, and no Slate-to-GameMode route. Config/assets remain unchanged and existing map/GameMode references are retained. These are source/reflection and in-process Automation checks, not actual two-window gameplay, packet capture, LAN or Internet validation. Generated/cache/build/report files are excluded from the commit.

### Deterministic Manual PIE Validation Checklist — user pending

Use `/Game/Maps/LocalMatch`, **2 players / Play As Listen Server**. Begin a fresh **`?Seed=5751`** session using the existing seed mechanism:

```text
UnrealEditor.exe "<path-to-project>/Gomokards.uproject" "-ini:Engine:[/Script/UnrealEd.EditorEngine]:InEditorGameURLOptions=?Seed=5751"
```

Stop/start PIE to repeat this seed. Development Restart randomizes it, so do not use Restart during card acquisition. Follow player identities shown in each window, rather than assuming window order. Coordinates are zero-based from top-left; each pair is Black then White.

| Pair | Black | White |
| --- | --- | --- |
| 1 | `(5,5)` | `(6,5)` |
| 2 | `(0,0)` | `(7,5)` |
| 3 | `(8,5)` | `(1,0)` |
| 4 | `(9,9)` | `(10,9)` |
| 5 | `(12,12)` | `(11,9)` |
| 6 | `(12,9)` | `(5,12)` |
| 7 | `(6,12)` | `(0,18)` |
| 8 | `(7,12)` | `(8,12)` |

After 16 placements Black has `[Restock, Steal]`, White `[Swap Hands, Tetris]`. **Black places `(17,17)`**, then **White plays Tetris**. Do not use Restock or Swap for this recipe. Actions become 18, ordinary turn becomes Black, and Black operates first. Initial block: **T shape, Top edge, Down gravity, origin `(13,0)`, White block color**; gravity may already move it before the first visible frame. Subsequent shapes/edges depend on how the board develops under your movement. Automation verifies that leaving all six blocks to automatic gravity completes all six opportunities and resumes nonterminal Black play.

1. **Start/acquire/activate:** both Listen Server and Client create UI and agree on the opening, private cards/counts and turn. White's Tetris consumes exactly one action and transfers the ordinary turn once to Black.
2. **Matching presentation:** both windows show identical active shape, position, color, edge and operator. Committed stones are square; active cells have the distinct border.
3. **Operator controls:** only the currently named operator's window moves/rotates the piece. Try keys from the other window and observe no change. Over six blocks verify both Host and remote Client become operators, alternating Black/White while the ordinary current player remains Black.
4. **Absolute arrows:** for each encountered edge, its opposite-gravity key does nothing. Gravity-direction and both perpendicular keys move when space exists. Space rotates clockwise when space exists; no wall kick. All four orientations are covered automatically even if this manual run does not encounter them all.
5. **Gravity/locking:** manual input into an obstacle does not lock. Releasing input allows the blocked automatic tick to lock. A successful soft drop postpones the next automatic tick by roughly 0.5 seconds rather than immediately double-stepping; perpendicular/rotation do not restart that interval. Compare both windows.
6. **Ordinary freeze:** board clicks, card buttons and targeting cannot perform ordinary actions while active. Key movement does not increase Actions, change hand counts or consume a remaining Confusion effect. Confusion interaction is also covered automatically; no extra manual acquisition is required.
7. **Commit/exit:** both windows agree on each locked board/new piece. After six opportunities, no falling shape remains; committed stones become round, result/turn agree, with no extra ordinary action/transfer. With the hands-off recipe Black can place `(18,0)` normally. If you steer differently and reach a terminal result, verify the shared result and rejection of further ordinary actions instead.
8. **Lifecycle:** replay acquisition, activate and use authorized Host development restart while falling. Both views reset and remain free of stale pieces after the old deadline. Replay and disconnect during Tetris: remaining window enters SessionEnded without continued falling/locks or an invented winner.
9. **Regression:** repeat accepted Ghost `182` and prior card recipes as needed. All ten cards are now enabled through their correct boundaries, including Tetris in the old `5751` recipe. Line-clear/Barrier/fallback/terminal cases have automated fixtures; the user need not engineer those positions manually.

**Phase 5B user manual PIE validation: PENDING.** Agent performed no hands-on gameplay. The implementation request explicitly authorizes this focused commit/push with manual validation handed to the user. After Phase 5B manual acceptance, the next phase is a Development packaged build, followed by physical two-PC LAN validation (still pending). Internet/session-provider infrastructure remains later. Reliable server input has no prediction/interpolation; evaluate actual network feel before adding latency machinery. No packaging, LAN, Internet, reconnect/spectator protocol or malicious-host confidentiality is claimed here. Stop after Phase 5B.


## Phase 6A Packaging Readiness Audit

Continues from `854ab367c202f0fb1ff55d8e7161e26f9b8a1d8a`. Phase 5A user manual PIE remains **PASSED**; Phase 5B user manual PIE remains **PENDING**. This explicitly requested packaging audit proceeds without treating Phase 5B gameplay acceptance as complete. No gameplay, card, replication architecture, matchmaking, session provider or Dedicated Server target was added.

### Findings and fixes

- **Build:** `Gomokards.uproject` declares one Runtime module loaded at Default. Game and Editor targets already exist with UE 5.8 include order and V7 settings. Runtime dependencies are Core/CoreUObject/Engine/InputCore/EnhancedInput plus Slate/SlateCore; no UnrealEd or Editor module dependency. The enabled ModelingToolsEditorMode plugin is restricted to Editor targets. EnhancedInput supplies the configured default input classes even though gameplay uses Slate.
- **Assets/cook:** `Content/Maps/LocalMatch.umap` is the only project content asset. GameDefaultMap and EditorStartupMap point to `/Game/Maps/LocalMatch`; the configured GameMode is `/Script/Gomokards.LocalMatchGameMode`. Board/card/Ghost/Tetris presentation is C++ Slate geometry/text using runtime CoreStyle brushes/fonts, not editor textures, project card images, widget Blueprints or external pygame files. WhiteBrush is a runtime color brush; font resources come from Engine Content/Slate/Fonts. The explicit packaging command cooks LocalMatch and its dependencies. No Asset Manager or new generated asset is needed.
- **Lifecycle:** InitGame parses the host's Seed option and initializes server-owned state. PostLogin assigns seats and resets both views on the second join. Logout ends the session and cancels both timers. Reset, EndPlay and BeginDestroy cancel ticker handles and invalidate callback generations; stale mode inputs additionally fail epoch/token checks. Controller BeginPlay builds local viewport UI and removes it at EndPlay. It does not assume a replicated GameState snapshot is already available: absent/mismatched public/private state disables interaction, and GameState BeginPlay/RepNotify plus private RepNotify refresh presentation once coherent. Listen-host publication explicitly refreshes the same UI.
- **Non-editor code:** no GEditor, UnrealEd include, editor asset path or editor-only gameplay branch was found in project runtime code. Automation bodies are guarded by `WITH_DEV_AUTOMATION_TESTS`. The actual Development Game compile has `WITH_EDITOR=0` and `WITH_DEV_AUTOMATION_TESTS=1`: Development can retain registered tests, but fixtures run only when Automation is explicitly invoked; no runtime hand-injection/forced-spawn API or automatic test execution was added. This audit does not claim tests are stripped from Development.
- **Input:** local controller applies UIOnly focus to a keyboard-focusable Slate root, shows the mouse and unlocks it. Board/card clicks return focus to the root. Preview-key handling receives Escape and Tetris arrows/Space via the existing key helper; operator/coherence checks precede network submission. No editor input bindings are required. Packaged visual focus and physical keyboard/mouse behavior still require the user checklist below.
- **Environment fix:** UAT's nested child-command parsing failed on a checkout path containing an apostrophe; a project alias alone was insufficient because the default UAT log path had the same issue. A verified NTFS directory junction with a simple path plus process-local `uebp_LogFolder`/`uebp_FinalLogFolder` overrides bypassed it. A junction attempt on the engine drive was unsupported. No engine or gameplay code was patched. Prefer a normal checkout path without apostrophes for future packaging; a junction is optional. Existing build-cache path warnings after aliasing are not source errors.

### Validation and packaged contents

- **UE 5.8.2 Win64 Development Editor compile: PASSED.** UAT also compiled the non-editor **Gomokards Win64 Development** Game target successfully with MSVC 14.44 / Windows SDK 10.0.22621.0.
- **Build / Cook / Stage / Archive: PASSED**, UAT exit code 0 (`BUILD SUCCESSFUL`). Cook summary: **0 errors, 0 warnings**. Zen initially needed time to restart for staging; UAT started it successfully and completed the archive without manual service changes.
- Archive: `Gomokards/Saved/Packages/Phase6A/Windows/` (50 files, 1,091,016,318 bytes at creation, including debug symbols). Contains bootstrap `Gomokards.exe`, the Development game executable/dependencies, `.pak` and `.utoc/.ucas` containers, and prerequisite installers. Copy the entire folder. The staging manifest includes `Gomokards/Content/Maps/LocalMatch.umap`; actual pak listing confirms runtime `Roboto-Regular.ttf` and `Roboto-Bold.ttf`. CoreStyle geometry plus these engine resources support board/cards/Ghost/Tetris presentation; no custom material/texture dependency is missing from the audited view.
- **Packaged startup smoke: PASSED**, bootstrap exit code 0. A hidden, unattended **NullRHI** launch with `/Game/Maps/LocalMatch?listen?Seed=5751`, port 7777 and automatic `quit` loaded `LocalMatchGameMode`, opened the IP listener on 7777 and completed clean world/driver teardown. No gameplay input was sent. This proves cooked-map/runtime startup, not GPU rendering, focus, client joining or physical LAN success. Optional profiling DLL probes (aqProf/VTune/WinPixGpuCapturer) were unavailable but did not prevent startup; no Warning/Error log entries occurred in this smoke run.
- Evidence remains ignored under `Saved/PackagingAudit/`: `UAT/Log.txt`, staging manifests and `PackagedStartup.log`. The archived inner `Gomokards/Binaries/Win64/Gomokards.exe` SHA-256 is `E3A9F53289391E841ADD90E7ED383B74ABE35A13D5C15D1A0392E2D592C91ACE`, built from the Phase 5B source baseline above.
- No source/config/assets/tests required modification. This audit updates README and narrowly ignores generated `Build/*/FileOpenOrder/` cook-order logs; other Build resources remain trackable. The accepted Phase 5B Automation result remains **50 passed, 0 failed, 0 test warnings, 0 skipped**; Automation was not rerun for this documentation/ignore-only change. Phase 5B manual PIE and physical LAN remain pending.

### Reproduce Development packaging

Use an installed UE 5.8.2 with its supported MSVC/Windows SDK. Set the following paths for the machine; project and log paths should avoid apostrophes. The log directory must be dedicated to this run because AutomationTool clears its contents. Outputs below stay under ignored `Saved/`; do not commit Binaries/Intermediate/Saved or distribute just the executable.

```powershell
$EngineRoot = 'G:\GameDev\Unreal\UE_5.8'
$ProjectFile = Join-Path (Get-Location) 'Gomokards/Gomokards.uproject' # run from repository root
$ProjectRoot = Split-Path $ProjectFile
$env:uebp_LogFolder = Join-Path $ProjectRoot 'Saved/PackagingAudit/UAT'
$env:uebp_FinalLogFolder = $env:uebp_LogFolder
& "$EngineRoot/Engine/Build/BatchFiles/Build.bat" GomokardsEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
& "$EngineRoot/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$ProjectFile" -noP4 -platform=Win64 -clientconfig=Development -build -cook -map=/Game/Maps/LocalMatch -stage -pak -archive "-archivedirectory=$ProjectRoot/Saved/Packages/Phase6A" -prereqs -unattended -utf8output
```

### Exact direct-address LAN launch procedure

1. Copy the **entire archived Windows folder** to both PCs, using the same build. Install the bundled `Engine/Extras/Redist/en-us/vc_redist.x64.exe` if prerequisites are missing; no Unreal Editor installation is required on the second PC.
2. Put both PCs on a reachable private LAN. Find the host's LAN IPv4 address with `ipconfig`. Allow the packaged game through Windows Firewall on the private network; the default game driver uses **UDP 7777**. No router port forwarding or Internet service is part of this procedure. Do not use `127.0.0.1` on the second PC.
3. In PowerShell in the archived Windows folder, launch the host:

```powershell
.\Gomokards.exe "/Game/Maps/LocalMatch?listen?Seed=5751" -port=7777 -log -windowed -ResX=1280 -ResY=900
```

4. On the other PC, from its copied Windows folder, substitute the host's actual address:

```powershell
.\Gomokards.exe "192.168.1.10:7777" -log -windowed -ResX=1280 -ResY=900
```

The client connects to the host URL; the server supplies the map, so a client map suffix or Seed is unnecessary. The host must include `?listen`: launching the executable without it starts standalone hot-seat. Use the first local player as the host; read the displayed assigned identity instead of assuming an arbitrary window order. This uses UE's existing IP driver/direct travel, without a custom menu or session provider. For Ghost, close both processes and repeat with host `?Seed=182`. Development Restart randomizes the seed; restart the processes to replay a deterministic recipe.

### Next physical LAN / Manual Gameplay Validation Checklist

- First finish the pending Phase 5B two-window PIE checklist. Packaging success is not gameplay acceptance.
- Launch on both physical PCs without Editor. Verify startup map, board, card text/buttons and fonts render; host waits for the second player, then identities/turns synchronize.
- Click ordinary placements from each side; check wrong-turn/invalid requests, owner-only card identities/public counts, result and terminal rejection. Exercise card selection and Escape/right-click cancellation.
- Follow the Phase 5B seed 5751 recipe: test both operators' arrows/Space, absolute directions and inverse-gravity rejection, soft-drop timing, automatic lock, six opportunities, square/round display transition and normal play afterward. Compare both screens and record any lag/queued-key behavior.
- Relaunch with seed 182 for Ghost: verify preparation/freeze/countdown, gray old/new stones, private reward, six-placement reveal and continued play. Recheck the other eight cards using their accepted recipes.
- Restart on the authorized host during each timed mode; verify both views reset with no stale callback/shape. Close the client during a mode; the remaining host should reach SessionEnded (abrupt loss may wait for connection timeout), without inventing a winner. Start a new session afterward; reconnect is not implemented.
- Save both machines' game logs with observations, GPU/driver and build identity. LAN validation remains **PENDING** until the user reports the result.

Remaining risks: packaged GPU/RHI/font/focus behavior on the second machine is unverified; the template defaults to DX12/SM6 and retains its rendering settings. No graphics-setting redesign was made. Real network latency/firewall behavior and Phase 5B hands-on acceptance are pending. No Internet, malicious-host confidentiality, Shipping build or Dedicated Server support is claimed. Stop after Phase 6A.

## Demo UI Polish v0.1

This presentation pass follows the accepted Phase 5B implementation and the historical Phase 6A audit above. The three supplied images define (A) information placement, (B) portrait title/art/description hierarchy, and (C) overlapping hand/raised hover interaction only. Blue text/red sketch outlines and third-party art, borders, palette and theme are not reproduced. Core rules, ten-card pool, RNG, turn ownership, Ghost/Tetris rules and server authority are unchanged. No UMG rewrite, menus, sound, animation framework or new networking provider.

### Slate layout and card interaction

`SLocalMatchView` fits a compact 1600x940 logical composition inside the viewport: opponent backs at top, local cards below, central unchanged 19x19 board, left log and right side/status/card panel. The board's coordinate conversion, target previews, forbidden marks, barriers, Ghost gray stones and Tetris squares are retained. This is a simple fit-to-window layout, not a general responsive system; use a normal desktop window (preferably 1600x900 or larger).

`SDemoHand` centers overlapping portrait cards. Hover immediately raises one card and paints/hit-tests it above neighbors; its full title, blank art and description become visible. Disabled cards are muted but remain hover-readable. Card clicks return keyboard focus to the existing root; Escape/right-click cancellation and absolute Tetris arrows/Space remain supported. Extremely large hands necessarily expose less title area; there is no new hand cap or scrolling system.

Targeted selection remains local and leaves the card in hand. The right panel says **待选择目标** until cancellation/submission; only server acceptance consumes the card. Cancel/reselect does not publish a play. Submission clears local selection using the existing request behavior; rejection can be retried by selecting again. Right-panel priority is local targeted selection, active Ghost/Tetris, then most recent accepted public card. Instant cards remain visible as the most recent play until another accepted card or reset.

### Frozen Chinese presentation and asset seam

`DemoPresentation.cpp` is the single presentation mapping; gameplay definitions remain in their existing layer. All current player-facing gameplay labels, feedback, statuses and result text are Simplified Chinese; the brand remains `GOMOKARDS`. Historical English helper functions retained for older pure-helper tests are not called by the new UI.

| Internal ID | Display name | Description |
| --- | --- | --- |
| Restock | 补充库存 | 抽取 2 张卡牌。 |
| SwapHands | 战术换家 | 打出本牌后，与对手交换剩余手牌。 |
| Steal | 取之有道 | 随机获得对手 1 张手牌；若对手无牌，则无事发生。 |
| TacticalNuke | 战术核弹 | 清空并永久封锁一个目标位置。 |
| Polarity | 两极反转 | 翻转所选 2×2 区域内所有棋子的颜色。 |
| Confusion | 定位混淆 | 接下来 2 次成功行动共享混淆次数：落子颜色反转，出牌也会消耗 1 次。 |
| Barrier | 阴阳屏障 | 在所选区域放置屏障，阻断穿过该区域的棋子连线。 |
| BackToBasics | 回归基本功 | 清除定位混淆，并使双方本局后续无法再出牌。 |
| Ghost | 幽灵棋子 | 记忆棋盘 5 秒后隐藏所有棋子颜色；完成 6 次成功落子后恢复并结算胜负。 |
| Tetris | 俄罗斯方块 | 双方交替操控共 6 个方块；方块颜色与操作者相反，锁定后若形成 5 子及以上同色直线则消除。 |

Each card has a distinct `/Game/UI/Cards/T_CardArt_<InternalID>_Placeholder` Texture2D; the eleventh asset is `T_CardBack_Placeholder`. All are intentionally identical neutral blank 4x4 art, with embedded source data, no text or illustration baked in. Replace each asset at its stable path with illustration/background art later; Slate continues drawing name/description dynamically. `FDemoCardArt` retains textures for the UI lifetime. Runtime loads Unreal assets, never external PNG files. `DefaultGame.ini` narrowly always-cooks `/Game/UI/Cards` because native string references alone do not guarantee cook discovery. No Asset Manager was added. Cook configuration/readability were inspected; this task does **not** claim a new successful package.

### Public display, log and privacy

The only new public network data is the last accepted played-card ID, acting player and completed-action serial. GameMode sets these after successful Core resolution and clears them on reset; rejected requests, local selection/cancellation, draws and mode ticks cannot publish a false play. This is a small presentation field addition, not an event stream or gameplay change.

Opponent backs use public `HandCount` only, with no identity or hover-detail source. Own exact cards use the existing owner-only private view. `FDemoGameLog` retains at most 12 local text entries, consumes only coherent public/private snapshots, clears on epoch reset and never writes a disk log. It reports placements, accepted public cards, public hand-count increases, Ghost/Tetris transitions and results. Exact newly acquired names can appear only from the local owner's hand delta; another player's count increase never exposes an acquired ID. It does not log each key/gravity step. Replication can coalesce snapshots: the log intentionally omits unverifiable intermediate actions/acquisition identities and is not guaranteed complete history. A swap/steal may show newly received own cards; public count changes describe the actual observed net increase.

Static review: no full `FMatchState` reaches Slate; widgets have no GameMode gameplay access and calculate no authoritative card outcome. Core/card files, RPC permissions and ownership are unchanged. Ghost true colors remain transport-redacted; future Tetris shapes/RNG remain server-only. Played identities are public only after acceptance. Host confidentiality against a malicious host is not claimed.

The right panel uses assigned 黑方/白方 identities, with a presentation-only black circle blinking every 0.5 seconds beside the current side (the current operator during Tetris). Inactive effects are hidden. Active 定位混淆/回归基本功, Ghost countdown/remaining placements and Tetris block/operator/gravity are Chinese. A central 黑方获胜/白方获胜/平局 overlay displays terminal results. Existing authorized host restart is labeled **重新开始**; remote clients do not gain restart permission.

### Fonts and validation

A small Slate composite font uses Engine `Roboto-Regular.ttf` with Engine `DroidSansFallback.ttf` as CJK fallback. It does not depend on Windows-installed fonts or a download. Static cmap verification covered all **226 distinct Chinese characters** in the request and current C++ source, with **0 missing glyphs**. The earlier Phase 6A staging manifest includes the Engine fallback font; the new Demo's packaged rendering still needs future packaging/user validation.

**UE 5.8.2 Win64 Development Editor compile: PASSED.** The final exported full-suite Automation report contains **53 passed, 0 failed, 0 test warnings, 0 skipped/not run** (50 existing + 3 Demo groups). Report totals and each test state were inspected, not just the process exit code. The first run found two test-adaptation issues (an English feedback expectation and reading texture render dimensions under NullRHI); both were corrected and the complete suite rerun successfully. Existing 50 Automation groups remain; three focused Demo groups cover frozen mappings/embedded assets, identity/count-only log paths/display priority, and accepted/rejected public play plus local targeting. Older assertions are translated where needed; test-local symbols were made unique where unity compilation exposed collisions. No gameplay assertion was removed.

Visual startup inspection was attempted with the external desktop capture tool, but the fresh graphical process remained at the splash screen while ShaderCompileWorker processes were compiling. The agent stopped that inspection process without sending gameplay input. No completed gameplay-screen, card-hover or rendered-CJK visual pass is claimed; static font coverage and NullRHI Automation are not substitutes for the manual checks below.

**Phase 5B automated validation: PASSED.** **Phase 5B user manual PIE: PENDING.** **Demo UI Polish v0.1 user manual validation: PENDING.** The agent does not perform hands-on gameplay. The current task explicitly authorizes this focused commit/push while handing manual acceptance to the user. The earlier Phase 6A package is a historical pre-UI archive, not acceptance of this UI. The next package must wait for Demo UI and Phase 5B manual acceptance; physical two-PC LAN remains **PENDING**, and Internet/session-provider work remains later.

### Manual UI Validation Checklist

Use two-player Listen Server + Client PIE, preferably in separate normal desktop windows. Compare both views.

1. Verify top opponent hand, bottom own hand, large central board, left 对局记录 and right side/status/card panel. No blue/red sketch annotation; gameplay information and glyphs are readable Chinese.
2. Earn cards with an existing deterministic recipe. Opponent shows one back per public card count and no hover identity; own cards overlap, remain distinguishable, and show the exact frozen name/blank art/description when raised.
3. Hover raises the correct card above neighbors. Repeat while it is unplayable during opponent turn, Ghost, Tetris and 回归基本功: it remains readable but cannot play.
4. Select 战术核弹/两极反转/阴阳屏障: card stays in hand, right panel says 待选择目标, correct board preview appears. Escape, right-click and reselect cancel with no consumption/action. Invalid target does not publish a card; accepted target consumes once and appears publicly.
5. Check recent instant cards, persistent active Ghost/Tetris display and the bounded log. Opponent acquisitions show count only; your own acquired identity may appear locally. Actually played cards can be named to both.
6. Check 黑方/白方 identity, blinking indicator/turn changes, Chinese 定位混淆 remaining count and 回归基本功 disabled state. Inactive effects should not clutter the panel.
7. Repeat accepted Ghost seed 182 recipe: visible preparation/countdown/freeze, all-gray hidden stones including new ones/no true-color preview, remaining placements and coherent reveal. Existing Ghost gameplay acceptance is not a claim that this new UI was manually accepted.
8. Perform the pending Tetris checklist below through the new UI: same pose in both windows, correct operator/block/gravity and arrows/Space focus.
9. Verify win/draw overlay and terminal rejection. Only authorized Host gets working 重新开始; restart resets both UI views/log/selection. Disconnect shows ended session without an invented winner.

### Pending Phase 5B Tetris manual PIE through this UI

Set two players, Play As Listen Server, map `/Game/Maps/LocalMatch`, **Seed 5751**. For a fresh Editor launch, use:

```powershell
& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" "$ProjectFile" "-ini:Engine:[/Script/UnrealEd.EditorEngine]:InEditorGameURLOptions=?Seed=5751"
```

Stop/start PIE for a deterministic replay; **重新开始 randomizes the seed**, so do not use it during acquisition. Coordinates below are zero-based from top-left; perform Black then White in each row.

| Pair | Black | White |
| --- | --- | --- |
| 1 | (5,5) | (6,5) |
| 2 | (0,0) | (7,5) |
| 3 | (8,5) | (1,0) |
| 4 | (9,9) | (10,9) |
| 5 | (12,12) | (11,9) |
| 6 | (12,9) | (5,12) |
| 7 | (6,12) | (0,18) |
| 8 | (7,12) | (8,12) |

After 16 placements Black owns 补充库存/取之有道; White owns 战术换家/俄罗斯方块. Black places `(17,17)`, then White plays 俄罗斯方块. This is ordinary action 18; the first operator is Black, first piece is a white T spawned Top with Down gravity at `(13,0)` (it may already have fallen when viewed).

- Both windows show identical committed squares and active piece; right panel shows block number, actual operator and gravity. Only the operator can control it. Operators alternate over six spawn opportunities.
- Absolute arrows remain absolute. For Top/Down reject Up; Bottom/Up reject Down; Left/Right reject Left; Right/Left reject Right. Gravity/perpendicular directions work where space exists; Space rotates clockwise. Rejecting opposite input changes nothing. Replay as needed to inspect orientations.
- A successful soft drop resets the next gravity step to 0.5 seconds; perpendicular movement/rotation do not. A blocked manual input does not lock; only blocked automatic gravity locks. Compare both views.
- Ordinary board/card/target actions stay frozen; movement does not consume ordinary actions, hand cards or Confusion. Do not expect per-key log spam.
- After six opportunities, no active piece remains, committed stones return round and both views agree on normal turn/result. In the hands-off recipe Black can continue at `(18,0)`; if steering leads to terminal state, verify shared result and input rejection instead.
- Replay, restart on Host during falling, and wait past the old deadline: both views stay reset with no stale piece. Replay and disconnect during Tetris: remaining view ends the session without further falling/locks or an invented winner.

No physical LAN test is requested in this phase. Stop after Demo UI v0.1.

### Sync and Development Editor build

Close the running Gomokards Editor/game before linking. In the repository root:

```powershell
git switch ue-migration
git pull --ff-only origin ue-migration
$EngineRoot = 'G:\GameDev\Unreal\UE_5.8' # adjust to the installed UE 5.8.2
$ProjectFile = Join-Path (Get-Location) 'Gomokards/Gomokards.uproject'
& "$EngineRoot/Engine/Build/BatchFiles/Build.bat" GomokardsEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
```

This change adds **three .cpp files, two .h files and eleven binary .uasset textures**. Regenerate IDE project files if needed to show the new source files; UnrealBuildTool discovers them without that step. Do not routinely clear `Intermediate`/`Binaries`; a normal build/UHT handles these changes. Use the configured development drive for temporary files, caches and build logs. No new packaging or physical LAN execution is part of this UI task.

## Demo UI Polish v0.1.1 — final pre-packaging visual pass

The user manually inspected and tested v0.1 at `da06beb536cdac84876ce4208fc2ea568750d8ea` and reported normal gameplay/network interaction, including Phase 5B Tetris PIE. This acceptance supersedes the older pending notices above:

- **Phase 5B user manual PIE validation: PASSED (user-performed).**
- **Demo UI Polish v0.1 functional/manual interaction validation: PASSED (user-performed).**
- **Demo UI v0.1.1 visual user acceptance: PENDING.**

This small pass uses the original composition sketch and the user's actual v0.1 screenshot. Only presentation dimensions, text layout/visibility and routine feedback changed. No Core/gameplay/card rules, network state, replicated fields, RPCs, authority, privacy or assets changed. No new source files, frameworks or animation systems were added.

### Visual adjustments

- A 1920x1112 logical fit-to-window canvas and 700-unit board row replace 1600x940/540. At a typical 16:9 viewport, the displayed board is approximately **10% larger**, while its 19x19 drawing coordinates, hit testing, previews and centered result overlay remain unchanged. Outer padding falls from 16 to 12; panel gaps shrink. The brand moves into the log heading to free the redundant header row.
- Opponent backs become **96x119**, exactly 50% of the local **192x238** card dimensions, with 62-unit overlapping steps. They are centered above the board, use the existing back texture and still have no identity or hover-detail path.
- Local cards become wider (176 to 192 logical units), exposed steps increase from 136 to 154, title font increases from 14 to 18 and descriptions from 11 to 12 with wider wrapping. Normal/raised offsets are 36/4. Overall viewport fitting means this prioritizes title readability rather than increasing the entire local card's physical height; the raised card remains entirely within its 244-unit hand area, below the board. Existing hover, hit-test ordering, disabled inspection and click actions are preserved.
- Removed the top-right ordinary identity/turn line. The right panel is the single identity/turn location. Waiting/synchronization/session-ended/rule-decision messages remain available there; the turn circle grows from 12 to 18 logical units and retains its visibility-toggle blink.
- Right panel order remains player/turn, special status, current/recent card, restart. The card fits a **288x357** box, approximately **38% wider / 27% taller** on a typical 16:9 screen after canvas scaling (roughly 30–40% enlargement). Pending target > active Ghost/Tetris > recent accepted public play priority is unchanged. Status fonts compensate for the new canvas scale; the enlarged card leaves room for Tetris/Ghost text and Host restart.
- The log explicitly scrolls vertically, wraps text within the panel and scrolls to the newest entry when its text changes. There is no horizontal scroll widget. Up to 12 entries remain internally; longer histories can be read by vertical scrolling, and new entries return the view to the bottom. Owner/public log privacy and snapshot logic are unchanged.
- Accepted actions clear routine bottom feedback, including the duplicate success/reward notification already represented by board/hand/log changes. Empty feedback collapses. Actionable errors, target instructions, waiting messages and cancellation feedback remain. Frozen Chinese card strings, composite font and all eleven placeholder assets are untouched.

### Validation and next step

UE 5.8.2 Win64 Development Editor compilation **PASSED**. The exported full Gomokards Automation report was inspected: **53 passed, 0 failed, 0 test warnings, 0 skipped/not run**. All 53 existing groups were preserved without assertion changes; no pixel-value test suite was added. A graphical startup and external screenshot were attempted, but the process remained at the splash/default-engine-material shader compilation stage. The inspection process was stopped without gameplay input. The agent therefore does not claim a completed in-game visual check; the user checklist below remains required.

Manual visual checklist (user):

1. At a normal 16:9 size, verify the larger central board and tighter spacing; no card overlaps the board or clips the necessary controls.
2. Verify portrait opponent backs, count-only privacy, identifiable own titles and a fully readable raised card, including unavailable cards.
3. Verify no duplicate top-right turn text, a clearer blinking circle, readable special-mode text and a larger current/recent card with unchanged selection priority.
4. Read/scroll the log: text wraps, no horizontal scrollbar, newest entries appear at the bottom, private acquisition identity remains private.
5. Verify ordinary success leaves no persistent bottom message, while waiting/errors/target instructions remain visible. Check target cancellation, win/draw alignment and Host-only restart as a visual regression.

Stop UI development after this pass. **Packaging is next only after the user accepts v0.1.1 visually.** No package was built in this pass. Physical two-machine LAN remains **PENDING**; Internet/session-provider work remains later. Existing sync/Development Editor commands above apply; no source regeneration or routine Intermediate/Binaries deletion is required.

## Demo v0.1.2 — Exit Control

Demo UI v0.1.1 visual acceptance and Phase 5B manual PIE are **PASSED (user-performed)**. The first v0.1.1 Development package was built and startup-smoke-tested. These accepted results supersede the older pending notices above; physical two-machine LAN remains **PENDING**.

The existing Slate view now provides a visible top-left **退出游戏** button. **Escape always exits directly**, including targeting, Ghost, Tetris, terminal, waiting and SessionEnded states. This explicitly supersedes all historical Escape-to-cancel instructions. Right-click and re-clicking the selected targeted card still cancel selection; the targeting hint reflects this change. The button occupies the existing top row, with symmetric hand padding to prevent overlap; board geometry and vertical position are unchanged.

Both exit routes use the same local Slate callback, whose runtime default calls `UKismetSystemLibrary::QuitGame` for the local controller. Standard Unreal quit closes the packaged application and requests viewport/PIE closure in the Editor rather than quitting the Editor application. Exit is application control, not a gameplay action: no RPC, replicated field, Core action, card consumption, RNG or match-state mutation was added. Existing disconnect lifecycle remains responsible for departures; Exit is not surrender and assigns no winner. No new source files, assets, config, dependencies or networking features were added.

**UE 5.8.2 Win64 Development Editor compile: PASSED.** The final exported complete Gomokards Automation report contains **53 passed, 0 failed, 0 test warnings, 0 skipped/not run, 0 in progress**. All existing groups remain. The former Escape-cancels assertion now checks local exit dispatch while preserving selection and complete authoritative state/revision; the button handler shares that test callback, avoiding termination of the test process. Right-click/reselect cancellation and existing gameplay/network assertions remain passing. Static review confirms Escape is handled before all mode/state checks and existing card/board focus, Tetris arrows and Space routing are retained.

**Packaged Demo v0.1.2 status at this source commit: pending post-push packaging.** Per the requested sequence, the clean pushed commit is the package input; Build/Cook/Stage/Pak/Archive, graphical startup/exit smoke and final ZIP hashes are reported with the external deliverable after packaging, without a second source commit. Recipient Share/ZIP must omit only `.pdb` debug symbols, preserving runtime files, LocalMatch, all eleven placeholder textures and Chinese/Slate resources. All controllable build, report, archive and share outputs use the configured development drive outside version control. No Internet/session-provider work or physical LAN validation is included.

### Manual Gameplay Validation Checklist

User acceptance of the new Exit controls remains pending; agent startup/exit smoke is not gameplay acceptance. In separate short runs:

1. Verify **退出游戏** is visible top-left without overlapping opponent cards; click it to close the packaged game. In PIE, verify the play session ends while the Editor remains open.
2. Verify Escape closes immediately after normal board/card/hover interaction and during targeting, Ghost Preparation/Hidden, Tetris, match end, WaitingForPlayers and SessionEnded; it must never merely cancel selection.
3. Before exiting, verify right-click and reselect still cancel targeting without consuming a card/action, and Tetris arrows/Space retain focus and behavior.

The existing sync/Development Editor command applies. This focused commit and subsequent package are explicitly requested before this user recheck; stop after the v0.1.2 deliverable.

## Singleplayer Pivot — SP0 Architecture & Rules Impact Review

审查日期：2026-10-09。审查源码为 `ue-migration` 的 `b975b6310128a4113d5a118acfc5f1d280586623`，即用户接受的已打包 Demo v0.1.2 多人实现基线，必须保留为重要回退/参照版本。本节是**只读源码审查与设计文档**，仅修改 README；单人模式、AI、新卡、Run 系统均未实现。此前 v0.1.2 小节的“提交时等待打包”是历史时点，打包随后已完成；这不构成物理双机 LAN 或 Internet 验证。

**结论：需要少量前置接口拆分，不需要重写，也没有发现必须重建 Core 的结构性障碍。** 保留现有多人路径，以并列单人运行入口组合现有值状态和棋盘机制；SP1 是下一个建议实施检查点。以下 A 是用户已接受的产品决定，B 是待实施的架构建议，不能把建议字段名、缺省算法或待决选项当作冻结规则。历史多人规则仍然有效，单人改动不得反向覆盖它们。

### A. 已接受的产品与规则决定

- 产品转向单人 roguelike/deckbuilder：玩家跨战斗构筑牌组与棋诀；AI/Boss 使用棋力策略、主动能力、被动词条，**不持有玩家式牌组或假手牌**。遭遇规则独立于 Boss 词条。
- 每回合一个主行动：落子、出牌、抽牌三选一；满手时的 Replace 是 Draw 的一种处理，不是第四种基础行动。接受后结束本方回合；拒绝/取消不消耗回合。AI 主动能力未来通常替代落子，被动默认不占主行动；SP1 AI 只落子。
- 原型起手 3、手牌上限 5、初始牌组约 6–8 张；普通 Draw 抽 1 并结束回合。普通已打出的牌进弃牌堆；抽牌堆空时将弃牌堆洗回；Exhaust 只持续本场战斗，不删除 Run 卡牌。满手 Draw 需选一张弃掉，再抽 1 并结束回合。
- 单人普通封堵**不自动获得卡牌**；未来棋诀“防守反击”才在成功封堵后从玩家牌堆抽 1。多人仍保持封堵后从旧十卡池随机生成一张。
- SP1 只验证一场 19×19、正常五子连线的完整单人战斗；建议评估的最小卡池是两极反转、阴阳屏障、战术核弹、补充库存。无 Boss 能力、棋诀、Run 地图、商店、金币、奖励、存档或新卡。

单人角色分类（所有旧 `ECardId` 与 PvP 实现保留）：

| 旧卡 | 单人角色 | 已接受的单人方向 / 与旧版的差异 | SP1 |
| --- | --- | --- | --- |
| 两极反转 Polarity | A：玩家/Boss 共享效果 | 同一 2×2 翻色机制 | 建议纳入 |
| 阴阳屏障 Barrier | A：玩家/Boss 共享效果 | 同一四角连接阻断机制 | 建议纳入 |
| 战术核弹 TacticalNuke | A：玩家/Boss 共享效果 | 清空一个交点，本场永久禁用该点 | 建议纳入 |
| 定位混淆 Confusion | D：移出单人 | 不删除旧共享行动计数机制 | 排除 |
| 补充库存 Restock | B：仅玩家 | 从玩家牌堆抽 3，受上限 5 限制；溢出未定。旧版是随机生成 2 | 建议纳入 |
| 战术换家 SwapHands | D：移出单人 | 不删除旧交换手牌实现 | 排除 |
| 取之有道 Steal | B：仅玩家 | 随机获得当前 Boss 一个明确可偷取词条的玩家版本，非转移 Boss 运行对象 | 后续 |
| 回归基本功 BackToBasics | B：仅玩家 | 本场压制玩家卡牌/棋诀、Boss 主动/被动；保留棋盘历史、五子规则与遭遇规则；兼容战斗 Exhaust | 后续 |
| 幽灵棋子 Ghost | C：仅 Boss 词条/被动 | 复用准备/隐藏/揭示；AI 不得默认偷看真色 | 后续 |
| 俄罗斯方块 Tetris | B：仅玩家 | 玩家操纵双方颜色、双方同色 5+ 连线均消除；系统决定颜色/形状；高影响战斗 Exhaust；约四次机会是方向，精确队列未冻结 | 后续 |

移花接木、乾坤挪移、玉石俱焚、封穴、弃车保帅、破阵是后续候选，不属于 SP1；瓮中捉鳖、蓄势待发继续搁置。SP0 不为这些候选预建框架。

### B. 架构建议与源码证据（未实施）

#### 1. 当前可复用架构与复用矩阵

以下路径相对 `Gomokards/Source/Gomokards/`，函数/类型名对应本次检查的实际源码。

| 源码/机制 | 复用结论 | 具体边界 |
| --- | --- | --- |
| `Public/Core/MatchState.h`、`Private/Core/MatchState.cpp`：`FCell/FBoard` | 原样复用 | 361 格、`Y*19+X`、永久禁点、Barrier anchors；唯一权威棋盘，不能新增第二份可写 Board |
| `Private/Core/MatchRules.cpp`：`HasWinningLine/EvaluateBoardResult` | SP1 原样复用 | Barrier 截断连接；至少五连；双颜色同时成线判和，胜者按棋色而非行动者。未来六连遭遇不能靠改全局 `WinLength` 实现 |
| 同文件 `HasSuccessfulBlock` | 原样复用检测 | 原有八射线封堵判据刻意忽略 Barrier/禁点历史，不能顺手“修正”为新判据 |
| `FMatchState/FMatchResult` | 组合复用值状态 | 保留唯一 Board/Players/CurrentPlayerIndex/CompletedActions/Result；整个 PvP resolver 并非通用单人内核 |
| `ValidateAction/ResolveAction` | 小接口拆分 | 抽出双方共用的身份/落点检查及落子机制；保留 PvP 手续，单人另有牌堆与主行动事务 |
| `Private/Cards/CardEffects.cpp` 的 Nuke/Polarity/Barrier 分支 | 小型提取后共享 | 从依赖双手牌的分派函数中提取校验与棋盘效果；不复制三套变更算法 |
| `CardDefinitions/DrawCard` | 旧目录保持多人专用 | 十卡均匀有放回抽取不是牌堆；单人用很小的独立目录/定义表，不覆盖旧 Restock/Steal 等语义 |
| `TetrisOffsets/TetrisGravity/TetrisTranslation/TetrisFits/BestTetrisSpawnAtEdge/ChooseTetrisSpawn/ClearTetrisLines` | 几何/候选生成/清线原样复用 | `ChooseTetrisSpawn` 已显式接收 RNG；Barrier 影响连线，不参与物理碰撞 |
| `BeginTetris/SpawnOrSkipTetris/StepTetrisGravity/FinishTetris` | 后续拆分模式编排 | 六次机会、换操作者、反色、取下一块、结束合法行动检查存在硬耦合；不能把整套函数直接用于单人 |
| Ghost 值状态、`BeginGhostHidden`、隐藏投影 | 复用机制，后续改入口 | 真色仍在唯一权威板；Boss 触发不走玩家持牌入口；AI 获取信息另设窄边界 |
| `Private/Presentation/MatchPresentation.cpp`：`FBoardLayout` | SP1 几何原样复用 | 交点/2×2/格心命中、边界、Barrier 十字；当前 `TargetAt` 依旧卡 ID 找 target domain，SP1 三张目标牌域相同 |
| `SLocalMatchView.cpp`、`SDemoCards.cpp`、`DemoPresentation.cpp` | 绘制复用，接线适配 | 棋盘/手牌并不是已解耦组件，需剥离具体网络 Controller/投影依赖；中文字形、贴图、卡框继续用 |
| `ALocalMatchGameMode/GameState/PlayerController`、`MatchNetTypes`、网络测试 | 冻结保留 | RPC、owner-only 手牌、Epoch/Revision、Ghost/Tetris transport 和会话生命周期继续服务多人；不承载新牌堆 |
| 玩家四牌堆、单人请求/解析器、AI 策略、本地视图 | 单人特有新增 | 小型 C++ 值状态与显式调用；不建立通用技能/事件/命令框架 |

`FPlayerState` 的两个条目并不天然等于“两名真人”：ID、分配棋色与两方轮换仍适合人类对 AI。真正的冲突是 `Hand` 的所有权与旧抽卡/出牌规则，而不是棋盘必须重写。

#### 2. 当前 ResolveAction 的精确耦合

实际顺序如下：

1. `ValidateAction` 拒绝终局、Tetris、Ghost 准备；检查恰好两名有效异色玩家和当前行动身份。落子校验范围/占用/禁点；出牌检查旧 `bCardsDisabled`、Ghost Hidden、旧目录、`Actor.Hand.Contains` 及目标域。
2. 复制整个 `FMatchState` 为 Candidate。落子使用当前 Confusion 决定实际棋色；非 Hidden 时检查该落点胜负；计算封堵并**立刻向 Actor.Hand 添加 `DrawCard(Candidate.Random)`**。即使该落子胜利，封堵奖励仍执行。
3. 出牌先从旧手牌移除第一张相同 ID，再调用 `ExecuteCardEffect`。后者进入 switch **之前**就取出双方 Hand 引用；不是无玩家/无持牌条件下可直接调用的 Boss 效果 API。
4. 消耗进入行动前的共享 Confusion 计数，重施刷新为 2，Basics 清零；Polarity 才进行整板胜负扫描。Nuke/Barrier 不额外扫描；重复 Barrier/空点 Nuke/空区域 Polarity 等既有合法无变化操作仍可消费行动。
5. Hidden 成功落子累计到 6 才揭示并全板裁决。完成行动计数加一；仅未终局才换当前玩家。Tetris 在普通行动换人后启动；末尾的 `HasLegalAction` 只认识可落交点和当前旧手牌，不能用于单人 Draw/Replace。
6. 最后提交 Candidate，失败不提交，包含 RNG 回滚。`FActionResult.bBlockingReward` 当前同时承担“检测成功封堵”和“已授予奖励”的含义；网络 Ack/UI 已依赖奖励语义，不能全局改名义或直接拿它代表单人抽牌。

`ALocalMatchGameMode::PublishViews` 还存在一个基于空位与网络卡白名单的会话可行动检查。直接复用该 GameMode 即使绕过 Core 手牌，也可能错误进入 SessionEnded。`NM_Standalone` 当前实际是同机轮流控制两方，并不会自动产生 AI。

#### 3. 最小前置接口：复用机制，分开行动经济

建议仅做以下明确拆分，随 SP1 首批实现，不单开全库清理：

- **落子/封堵检测边界**：把既有双方身份、合法交点校验和“放实际棋色、计算该点连线、报告是否封堵”的机制提为窄函数。返回小型值结果（例如 `FPlacementOutcome` 的 `bSuccessfulBlock` 与棋色/落点/胜线事实），本身不抽卡、不改手牌、不转移回合。是否延迟胜负由现有 Ghost 调用方继续决定，不能在 helper 内擅自揭示 Hidden。
- **共享棋盘效果边界**：把 Nuke/Polarity/Barrier 的目标域校验与现有 Board 变更提为少数显式函数，可操作事务 Candidate 的 `FBoard`。只做那三个效果，不做注册系统。两条玩法入口都用它们；它们不认识手牌、Boss 对象、网络或牌堆。Polarity 之后的全板裁决仍调用唯一 `EvaluateBoardResult`，Nuke/Barrier 保留既有裁决时点。
- **行动经济边界**：旧 `ResolveAction` 保持原 API/外部行为，使用上述 helper 后自己执行旧封堵奖励、旧消费、Confusion/Ghost/Tetris 生命周期与旧可行动检查。新单人解析器使用同一机制但自己执行玩家牌堆事务和单人可行动查询。不在整个旧调用链插入 `bSingleplayer`，不先调用旧 resolver 再删奖励/恢复 RNG/纠正回合。

单人建议单独的 `FSingleplayerActionRequest`，三个 Kind 为 Place、PlayCard、Draw；Draw 可带待替换卡实例，PlayCard 带卡实例和规范化目标。保留旧 `EActionType` 和网络请求原样，不为 Draw 添加多人 RPC。共用棋盘坐标/目标值类型即可，不强迫嵌套一个无法表达牌堆实例的 `FActionRequest`。

同一个单人解析器有一个明确的成功完成点：`CompletedActions` 加一，未终局才按现有两方关系换人；Place/Play/Draw/Replace、将来的 AI 主动能力都走这里。无必要建立通用 CompleteAction 策略类；身份、落点、连线、效果算法不能复制，少量玩法专属的提交顺序由各自协调器明确表达。旧模式的计数/换人顺序及所有测试必须继续成立。

#### 4. 推荐状态所有权、不变量与生命周期

推荐 SP1 的 `FSingleplayerBattleState` **包含一个** `FMatchState Match`、一个 `FPlayerDeckState`、人类/AI 身份映射以及显式牌堆/AI RNG。不立即加入空的 EnemyAbilityState、RunState 或万能 BattleModifiers。`FMatchState` 作为可复用棋盘/两方状态容器保留，不宣称它的全部字段已经与 PvP 解耦。

| 状态/数据 | 所有者与真值 | 变更边界与数据流 | 不变量、重置、测试缝 |
| --- | --- | --- | --- |
| Board、棋色、当前方、结果、行动数 | Battle.Match，且只有这一份 | 单人解析器 Candidate → 共享机制 → 一次提交 → 派生只读视图 | 不再建立 Battle.Board/Result/Turn 副本；整场 Reset；值状态相等测试包含全部字段 |
| DrawPile/Hand/DiscardPile/ExhaustPile | Battle.PlayerDeck | 解析器移动卡实例；UI 只提交实例/目标 | 四区实例互斥、总数守恒、Hand≤5；每场重建；重复卡与跨洗牌测试 |
| 旧 Players[i].Hand、旧模式字段 | 保留在 Match 中给 PvP 使用 | SP1 不写、不镜像单人 Hand | SP1 双方旧 Hand 为空、Confusion=0、Ghost=None、Tetris inactive、旧 cards-disabled=false；用断言/测试防串线 |
| AI 身份与当前策略配置 | Battle 的只读敌方定义/身份；实际当前方仍为 Match | 本地 runtime 请求纯策略 → 返回 intent → 同一解析器 | AI 没有 Controller 假玩家/手牌；只在其回合行动；固定快照/种子测试 |
| 敌方主动/被动运行状态（后续） | Battle 内小型 EnemyTraitState；静态定义单独只读 | 解析器处理使用、冷却、触发 | 战斗结束丢弃；不可被 UI/Steal 直接共享引用；能力资格/冷却测试 |
| 棋诀、压制状态（后续） | Battle 的有效棋诀副本/压制位；永久所有权未来属于 Run | 明确行动结果 → 显式调用有效棋诀 → 同一事务 | Basics 只压制本场执行，不删除所有权；重置恢复；触发一次/压制测试 |
| 遭遇定义（后续） | 战斗初始化输入的独立只读 EncounterRules | 解析器裁决读取；不附着在 Boss 可偷/可压制词条上 | Basics 不能清掉六连等遭遇约束；下一场重选；对照测试 |
| 时序/待执行 AI 回调 | `ASingleplayerBattleGameMode` 的调度元数据 | 捕获战斗 generation/行动 serial，执行前重新验证 | Init/reset/EndPlay 失效旧回调；不作为第二份轮次状态；过期回调测试 |
| 选择、hover、Replace 候选、日志 | 本地 Controller/Slate | 输入意图 → 解析器；读取提交后的本地视图 | 不可写 Board/Deck/RNG；reset 清空；取消与焦点测试 |
| 未来 Run 牌组/金币/棋诀/路线 | 后续 Run owner，SP1 不实现 | Run 定义快照 → Battle 实例；战后显式结果/奖励命令回到 Run | Battle discard/Exhaust/Steal 不得直接修改永久牌组；以后另测跨战斗边界 |

牌堆与棋盘在同一个 Battle Candidate 内原子提交；四牌堆外置不是建立第二份手牌真值。未来如很多共用函数真的只需要 Board/参与者，可再评估提取更小内核；SP1 不先搬迁所有 Match 字段。

#### 5. 一回合的数据流、牌堆事务与 RNG

建议流程：本地输入（含战斗 generation、预期行动 serial）→ 验证终局/当前方/卡实例/目标/替换资格 → 复制 Battle（包括各 RNG）→ 执行落子、共享效果或牌堆操作 → 汇总小型行动事实 → 单次完成/裁决/换人 → 提交 → 更新本地视图 → 若仍进行且轮到 AI，调度一次 AI 决策。玩家胜利或平局后不再额外给 AI 一次行动。

牌堆表示建议为四个小数组，卡实例只需本场稳定 `InstanceId` 与定义 ID；允许多张同类卡，每张初始化时获得不同实例号。定义只存目标域、效果身份、抽牌数量、是否 Exhaust 等 SP1 真正需要的数据；普通 C++ 常量表足够，不需要 DataAsset/AssetManager/Deck 子系统。SP1 没有 Exhaust 卡也可以保留空 ExhaustPile 以统一守恒；不提供未获批准的 Exhaust 玩法。

- 建场：从固定牌组清单复制实例，按明确种子洗牌、抽起手 3；不调用旧 `DrawCard`。洗牌只重排已有实例，普通抽牌只移动栈顶，不能每抽一张重新随机创建卡。
- 出牌：先验证属于本人 Hand 和合法目标；在 Candidate 内移走精确实例、执行效果，再按已确认的时点进入 Discard/Exhaust。若处理中有临时“正在结算的卡”，它只是本次事务的局部值，不是永久第五牌堆；提交时所有实例回归四区之一。Restock 本身何时可被这次洗牌再抽到必须确认，不能无意沿用旧“先删除”的行为。
- Draw/Replace：不足 5 张时 Draw 不接受多余替换参数；5 张时必须指明确实在 Hand 的实例。建议 UI 用同一个抽牌按钮进入本地选替换模式，再提交一个完整请求；取消无事务。顺序按需求为弃所选牌→必要时弃牌堆搬入抽牌堆并清空弃牌堆→洗牌→抽 1→完成行动。被替换牌是否可在此次回洗中立即抽回，在下方作为规则确认项列出。
- 洗牌不能 `append` 后遗留 Discard 副本；普通 Draw 不碰 Exhaust。抽牌量/手牌上限在核心事务检查，不能只由按钮禁用保证。Restock 溢出与无可抽牌处理属于产品规则，下面的推荐不能视为实现决定。
- 单人合法行动查询按当前方分别检查：人类的合法落子、可用四卡、Draw/Replace；AI 只检查合法落子。不调用旧 `HasLegalAction`，也不将牌堆 Hand 填回旧 Hand 来欺骗它。双方同时成线沿用现有 Draw；“没有合法行动”并不自动等于和棋。

**RNG 建议：少量显式 stream，不建框架。** 保留 `Match.Random` 给旧 Core/未来棋盘模式，SP1 的共享三种棋盘效果不消费它；新增 `DeckRandom` 与 `AIRandom`，未来确有随机词条再加 `TraitRandom`。初始化可直接传入固定种子元组，普通运行由一个 battle seed 用固定、文档化的盐派生，避免依赖平台不稳定哈希。已有 PvP `Match.Random` 调用次数/顺序不变。

AI tie-break 在 Candidate 的 AIRandom 副本上计算；若请求被拒绝/已过期，不提交该副本。不得先让策略修改 live RNG 再验证动作。排序与候选遍历稳定（例如行优先），只在确需随机平局选择时采样。测试应保证改变洗牌种子不改变同一棋盘的 AI tie-break 流；合法抽牌不扰动未来 AI 流；拒绝/取消保持所有 stream 不变。SP1 开始可用确定性平局优先，随机平局不是必需的新系统。

#### 6. AI 与运行入口：并列小路径，不伪装网络玩家

建议 `ASingleplayerBattleGameMode` 持有 Battle 并接收一个本地人类 Controller 的意图；AI 是普通 C++ 决策函数/小对象，由该 owner 调用。它接收只读策略输入，返回落点，不能拿可写 Board、Deck、GameMode 或 UObject 引用；最终仍走相同的单人解析器合法性/回合检查。无需 AIController、行为树、导航、感知系统或多线程搜索。

SP1 最小策略建议：先枚举所有合法交点，试出自己立即获胜的位置，再试对手立即获胜的位置并优先封堵，余下用简单连通结构评分/稳定平局排序。模拟只在临时棋盘中完成，连线检查调用现有 `HasWinningLine` 并尊重 Barrier/禁点。多个无法同时封堵的威胁允许简单 AI 失误；不是保证不输的棋力指标。更深威胁搜索和随机 tie-break 可后置，不引入 ML、ONNX、LLM、强化学习或 MCTS 架构。

每次提交后由 owner 检查权威当前方，只挂一个待执行 AI 回调（可以下一 tick 执行，无需人为延迟框架）；回调携带 generation/预期 CompletedActions，重置、结束或离开 world 后不得落子。AI 异常返回非法点时应无变化、报告诊断，不强行写板或无限递归重试。程序须区分“AI 找不到点”与“棋盘已终局”，没有合法落点的规则见待决项。

并列入口应通过单独的开发启动/地图 GameMode override 等最小显式方式选择，不把既有 LocalMatch 默认配置改成单人，也不把 Run 字段塞进 `ALocalMatchGameMode`。单人 Controller 只负责局部意图、焦点和视图订阅，不发旧 gameplay RPC；AI 不是第二个远程 Controller。不需要新增单人 GameState 复制层。Battle 完成初始化之后才发布 ready 视图；reset 清本地选牌、日志和 pending intent；EndPlay 解绑委托并取消待执行回调，必要时 BeginDestroy 做幂等兜底。

#### 7. 共享效果与未来能力边界

**Boss Polarity 不等于 Boss 出一张牌。** 玩家入口验证本方 Hand 实例、消耗/归堆；Boss 入口未来验证该能力定义、资格、冷却和主行动限制；二者再调用同一个经过目标校验的棋盘效果函数。效果函数不得以“内部可信”为由省掉范围检查；Boss 未经授权的能力不能通过直接调用效果绕过协调器。效果返回值不能自行改变轮次。SP1 只实现前三种 Board 效果的小拆分，不提前建立 Boss ability registry。

**封堵奖励迁移**：共享落子结果报告 `bSuccessfulBlock`；PvP 协调器继续在当前时点给 Actor.Hand 加一张旧随机卡，并保持 `bBlockingReward` Ack、中奖概率、胜利落子奖励、RNG 次数不变。SP1 协调器只观察事实，零奖励、零额外 RNG。未来“防守反击”在已成功行动事实后由小型 battle resolver 显式调用一次，从**有限玩家牌堆**抽 1，而非随机生成 ECardId。重复射线只给一次事实；拒绝请求没有事实。启用判定读 Battle 有效棋诀/压制位；无事件总线。未来触发时点、满手抽牌处理、终局封堵是否触发棋诀需在加入它时明确，不影响 SP1 的“无奖励”。

**回归基本功**：旧 `bCardsDisabled` 不足以表达 Boss/棋诀/遭遇边界，而且旧 `CanPlayCards` 还绑定 Ghost/Tetris 状态。最小建议是后续 Battle 增加一个本场不可逆的 `bTricksSuppressed`，通过四个明确的资格判断控制玩家卡、玩家棋诀、Boss 主动、Boss 被动。因为当前要求四类一起关闭，不必先存四个可独立组合的状态位；如后来存在独立压制需求再增加。遭遇规则永远不读此压制位，基础合法落子/胜负仍有效。压制是持久的资格约束，词条对象不被销毁，偷来的玩家效果也必须受相应资格约束。

Basics 还需要在同一事务中停止这些来源的**持续效果/挂起回调**，不能只禁止下次施放，却让旧被动继续 tick；每种实际存在的持续能力明确做清理，不能靠遍历通用标签猜测。既有 Nuke 禁点、Barrier、翻色、移除棋子属于 Board 历史，不回滚；本场 Exhaust 归堆照常。SP1 不实现该卡，所以它与未来 Ghost/Tetris 激活模式的压制/揭示/终止顺序、是否仍允许无意义 Draw 等细节留后续确认，不用为此先建状态框架。

**取之有道**：最小建议为只读 BossTraitDefinition 的可选 PlayerCopyDefinitionId 映射，仅有映射的词条进入候选池。解析器在候选事务内按稳定候选序列用 TraitRandom 选择；创建明确的玩家临时卡或棋诀实例，不复制 Boss 的对象、冷却、timer、指针或权限。原 Boss 词条默认保留。玩家复制物的持续时间、重复获取叠加、没有候选时是否消耗、满手时去向、是否 Exhaust 都必须在该卡上线前定案；没有可用映射的 Boss 词条不需参与。SP1 不需要映射表或新接口实现。

#### 8. Tetris 与 Ghost：可复用，但不能直接更改旧模式

**Tetris 精确边界**：`FTetrisState::BlockLimit=6` 是常量；`SpawnOrSkipTetris` 每块用 `Match.Random` 抽形状、把块色设为操作者反色，跳过时换操作者；`StepTetrisGravity` 锁定/清线后也换操作者；`BeginTetris` 从普通换人后的 CurrentPlayerIndex 开始；`FinishTetris` 使用旧手牌版 `HasLegalAction`。因此将 BlockLimit 改成 4、固定 OperatorIndex 或改统一投影会破坏 PvP，不能作为单人实现捷径。

后续到单人 Tetris 阶段，只把“姿态校验/移动/旋转/重力推进/锁定写板/清线”与“选下一块、序列次数、颜色、操作者、结束回合”分离为明确调用。复用现有 offsets、spawn 搜索、碰撞、四边引力、禁反向输入、无 kick 旋转、Barrier-aware 双色同时清线，不复制整个引擎。PvP 调用方保持六机会/交替/反色/现有 RNG；SP 调用方提供系统队列、固定人类控制身份及自己的结束策略。场上姿态只有一份，队列只描述尚未激活的块，不复制当前 Board。

计时器的单调时间 deadline、generation、0.5 秒软降重置和“只有阻塞的自动重力才锁定”机制可复用；现有 runtime 同时处理网络 Pose/Revision，不能整类继承后删网络分支。SP1 没有 Tetris，不现在移动 timer 代码。约四次机会是否正好四次、堵塞出生是否算次数、精确形状/颜色/边缘策略、预告长度、预生成对 RNG 的影响、模式结束把回合交给谁均后续确认。保留旧 `FMatchTetrisPose` 的“只发送当前块”协议，单人预告用本地视图，不向多人新增未来队列。

`ClearTetrisLines` 当前对整板先收集所有满足 `HasWinningLine` 的格，再同时清除，包含双方棋色、保留禁点/Barrier 且不塌落；这正是所需的共用机制。未来若遭遇要求六连获胜，不能把 Tetris 的既定 5+ 消线阈值意外一起改成六：两种规则需在实际加入遭遇时显式分开，不改当前全局常量。

**Ghost 精确边界**：目前牌启动 Preparation，runtime 五秒真实时间后调用 `BeginGhostHidden`；真色一直留在 Board，Hidden 时投影只给 HiddenOccupied；六次成功落子后揭示/整板裁决。旧手牌限制、封堵真色奖励、计数和网络遮罩保留。Boss-only 启动未来通过能力入口使用相同准备/隐藏/揭示机制，而不是塞一张 Ghost 到 AI.Hand。

建议从 SP1 开始让策略接口接受小型 `FAIPolicyView`（SP1 普通模式可见完整棋色），不是 `const FSingleplayerBattleState&`；只读完整 Battle 仍会泄露隐藏真色。以后 Ghost 由 owner 生成按允许信息遮罩的快照：可见占用/禁点/Barrier，但无隐藏颜色、玩家 Hand/牌堆顺序/全局 RNG。仅当 Boss 定义明确允许时增加信息能力；不能因本地权威同进程就默认全知。记忆与隐藏时如何评分留 Ghost 阶段，不建 belief 系统；即使允许人类记忆准备期，也不能悄悄给 AI 一份持续更新的真色板。测试未来应证明两份可观察快照相同而隐藏真色不同的局面，在相同策略状态/种子下给出相同决策。

#### 9. Slate 复用与多人保全

源码显示，`SMatchBoard` 定义在 `SLocalMatchView.cpp` 内，直接持有 `SLocalMatchView` 并读取 `FMatchPublicView`/网络卡白名单；`SDemoHand` 直接持有 `ALocalMatchPlayerController`，从 owner-only Hand/count 投影取数并发旧请求；`FTargetSelection` 同样检查旧 `FPlayerState.Hand`。这些都不能“不改接线就复用到单人”。

SP1 只需小型展示接口拆分：绘制用只读 Board display/选中目标/可用性值，输入用明确 delegate；保留旧 Controller 的适配接线，再由单人本地视图喂同一棋盘/手牌绘制。采用少量 Slate attributes/delegates 即可，无需统一 ViewModel 框架、全 UI 重写或迁移 UMG。`FBoardLayout` 命中/坐标与牌框、字体、贴图、重叠/hover 算法保留。SP1 三张目标卡可以继续复用现有 ID 的相同目标域；以后新增目标形状才把 domain 作为直接输入，不提前建任意形状框架。

`SDemoCard` 本身较轻，但文本从 `DemoCard(Id)` 的固定 PvP 定义取出；Restock 单人“抽 3”不能把旧定义的“抽 2”全局改掉。建议让卡片接受小型展示数据（名称/说明/贴图/可用性），由两个目录分别提供；同一个美术资源可以共享。日志不复用依赖 Epoch/公私投影差分的 `FDemoGameLog::Update` 作为权威事件推断：单人可直接把已提交的简短行动结果交给有界本地日志，保留布局而无需事件流。

单人只增加一侧玩家手牌、抽牌/满手替换、牌堆/弃牌/Exhaust 数量、AI 身份与回合/结果。AI 区域不再展示假对手手牌；SP1 不造空词条面板或预告框。Esc/退出按钮继续是本地应用控制，右键/重选取消选牌；满手替换取消也不消费行动，且 Esc 仍退出。绘制、hover、焦点不访问权威 RNG。

多人保全策略：以完整 SHA `b975b6310128a4113d5a118acfc5f1d280586623` 及已交付 v0.1.2 包作为可复现参照；本次不强制新建 tag/分支、不改写历史。GameMode、GameState、公私投影、RPC、网络测试和模式生命周期均保留。后续共享 helper/绘制组件的行为保持型提取允许旧调用点改接线，但不能改外部语义/协议；并列路径不等于复制整套 Core。每次提取跑完整旧套件，不能通过删测试或改变期待奖励/卡池来让单人通过。

#### 10. 失败模式与阶段分类

| 风险 | 级别 / 阶段 | 明确防线与验证 |
| --- | --- | --- |
| Battle 与 Match 各持一份 Board/Turn/Result | 高，REQUIRED FOR SP1 | 只组合 Match；UI 全部派生，拒绝/重置比较完整 Battle |
| 把玩家 Deck.Hand 同步到旧 Hand，或给 AI 假牌 | 高，REQUIRED FOR SP1 | 旧双方 Hand 在 SP1 始终为空；共享 Board helper 无持牌前置条件 |
| 调旧 resolver 后删掉奖励，导致 RNG/结束判断已污染 | 高，REQUIRED FOR SP1 | 检测与奖励分离；SP1 封堵卡数/RNG 无变化，PvP 原样加一 |
| `if Singleplayer` 扩散至 Core/卡/UI/runtime | 高，REQUIRED FOR SP1 | 两个显式协调入口，少量共用机制；不改网络状态表达单人规则 |
| Draw/Replace 洗牌复制卡、拒绝仍消费 RNG | 高，REQUIRED FOR SP1 | 整个 Battle 候选事务；按实例 ID 检查四区互斥和守恒；跨洗牌/过期请求测试 |
| Draw 随机流改变 AI 决策，UI 多刷新导致不同结果 | 中高，REQUIRED FOR SP1 | Deck/AIRandom 分开，AI 决策在候选事务中消费；UI 不持 RNG |
| AI 直接写 Board、同一回合重复调度、reset 后旧回调落子 | 高，REQUIRED FOR SP1 | 只返回 intent；统一解析器；generation/行动 serial 校验和取消 |
| 满盘误判和棋、终局后 AI 多走一步 | 高，REQUIRED FOR SP1 | 独立单人可行动查询；终局先停止；确认无法落子规则，不套旧 SessionEnded |
| Boss 绕过能力/目标校验 | 高，REQUIRED LATER | Boss 资格与共享 Board 校验分层；非法能力/坐标完整无变化 |
| Basics 清遭遇/棋盘，或只禁新施放却留下持续被动 | 高，REQUIRED LATER | 分离压制与 EncounterRules；各持续能力显式清理；保留历史测试 |
| Steal 共享 Boss 内部状态或临时牌写进永久 Run | 高，REQUIRED LATER | 显式玩家复制定义，新建 Battle 实例；Run 仅接受未来明确奖励命令 |
| 为 SP 改六块限制/换人/队列而破坏 PvP | 高，REQUIRED LATER | 保留旧编排与测试；只提取共用运动/锁定机制，SP 序列另管 |
| Ghost AI 读到权威隐藏真色 | 高，REQUIRED LATER；输入边界 SP1 保留 | 受限策略快照；禁止直接拿 Battle/Board 真值对象；可观察等价性测试 |
| 抽 3 的展示仍来自 PvP 抽 2，或把 PvP 一并改掉 | 中，REQUIRED FOR SP1 | 独立小目录/展示输入；双目录回归 |

范围归类：

- **REQUIRED FOR SP1**：确认下列阻塞规则；共享落子/三 Board 效果窄拆分；Battle+四牌堆+事务+独立行动查询；并列 runtime/本地视图；最小合法落子 AI；Draw/Replace 与四卡子集；旧完整测试回归。
- **REQUIRED LATER**：Boss 主动/被动、棋诀显式触发、Basics 压制及持续效果清理、Steal 玩家复制语义、Ghost 信息规则、SP Tetris 队列编排、遭遇胜负阈值、Run 与 Battle 生命周期分离。只在各自真实功能进入范围时实现。
- **NICE TO HAVE**：固定种子录入便利、可选 AI 思考延迟、更强威胁评分、调试行动追踪、目录 DataAsset 化；不是 SP1 正确性的前提。
- **DO NOT BUILD**：GAS、ECS、通用 modifier/event bus/技能图/状态机/AI/牌堆框架、完整 AI 搜索基础设施、ML/LLM/ONNX、假手牌、第二权威棋盘、为单人改造多人复制模型、现在建设商店/存档/Run 地图或 UMG 重写。

未来六张新卡只暴露应保留的边界：多目标/实例与棋色校验、完整 Candidate 原子提交、棋盘变更后明确胜负时点、临时禁点与永久 Nuke 禁点分别表示、按成功行动推进期限、效果后调用同一有限牌堆抽牌。现在 `FCell.bForbidden` 无法表示来源/剩余时间，封穴上线时需旁侧临时禁点计时记录，不能到期把已有永久禁点清掉。相邻是四邻/八邻、Barrier 是否影响移动/交换或“不同连线”判定、破阵交叉线的去重、临时 3 行动起止时点均后续再定；不要为这些问题现在扩建 targeting/effect 系统。

#### 11. SP1 最小实施顺序（仅设计）

1. 定案第 13 节真正阻塞的规则，固定一个可复现的牌组/初始身份/种子 fixture；保留多人基线与既有 53 组测试。
2. 先做落子/封堵事实和三张 Board 卡效果的行为保持型提取；旧入口调用新 helper，旧奖励/RNG/裁决顺序不变。用原用例及直接 helper 对照验证，不先移动所有 Core 文件。
3. 实现纯值 Battle、四牌堆、三种请求及原子解析器；先测试 Draw/Replace/Restock、实例守恒、无封堵奖励、非法/过期请求完整无变化。再接胜负与单人合法行动查询。
4. 实现只有合法落子的小型 AI 与受限只读输入；接入同一解析器和单次调度。先用固定局面验证赢一步、防一步与轮次，不提高搜索深度。
5. 增加并列 GameMode/本地 Controller 入口与必要 Slate 接线，复用绘制、目标命中、卡片/字体，提供足够完成一局的 UI，不造 roguelike 外壳。
6. 编译并跑完整 Gomokards 自动化（原 53 组保留，加 SP 测试）；静态检查权限/状态/RNG/资产引用，输出用户手动单局清单。用户验证后再按约定收尾，不把本次 SP0 当作实施授权。

#### 12. SP1 明确验收标准（未来验证，不是本次结果）

| 类别 | 可观察/可断言的通过条件 |
| --- | --- |
| 状态与初始化 | 同一固定牌组、身份、种子得到同一完整 Battle；一个权威 Board/CurrentPlayer/Result；起手恰好 3；双方旧 Hand 均为空；AI 无 Deck/假手牌 |
| 主行动 | 合法 Place/Play/Draw/满手 Replace 分别只加一次 CompletedActions、只换一次当前方；已终局不再换人；非法/取消/重复 serial 不改任何状态或 stream |
| 四牌堆 | Hand 始终≤5；重复同类牌仍有唯一实例；初始、打牌、Replace、跨多轮洗牌后实例全集完全一致且各实例恰好属于一区；无增牌/丢牌；Exhaust 不参与洗牌 |
| Draw/Replace | 低于上限只抽 1；满手必须选当前有效实例；错误实例/旧选择/缺少替换请求不弃牌、不洗牌、不消费 RNG；成功原子弃 1 抽 1，无可见中间状态；溢出/回抽遵循已定规则 |
| Restock | 只消费/归堆所选一张实例；尝试抽 3，实际入手/溢出/RNG/本牌是否可回抽与确认规则一致；不从十卡随机池生成牌 |
| AI | 玩家非终局行动后恰好一个 AI 回合，只提交一个合法落点；固定局面与种子可复现；立即胜利/单点防败 fixture 正确；多个必败威胁不要求完美；无合法点处理遵守明确规则 |
| AI 生命周期 | pending AI 不阻止状态检查；重置/结束/离开后旧任务不能写新战斗；玩家胜利后没有 AI 落子；没有在 AI 策略/UI 中写 live Board |
| 封堵对照 | 同一个封堵局面 SP1 报告成功封堵但牌堆/旧 Hand/奖励 RNG 不变；PvP 仍按旧规则恰好生成 1 张卡，含多射线与胜利封堵 |
| 棋盘效果 | Nuke 清点+永久禁点、Polarity 完整 2×2 翻色/空点不变/双胜和棋、Barrier 六条局部连接/重复部署语义与旧基线一致；目标越界原子拒绝，实际棋色归属正确 |
| 终局/可行动 | 两方正常五子胜利和双胜和棋可结束一局；终局行动拒绝；单人牌堆合法动作不被旧 HasLegalAction/SessionEnded 漏判；满盘与 AI 无可落点 fixture 有已确认结果，不自行补 pass/胜者 |
| RNG | 全 Battle 拒绝前后相等（含所有 stream 的初始/当前状态）；同棋盘改变 DeckRandom 不改变独立 AIRandom 决策序列；展示刷新/hover 不采样 |
| 回归 | 原 53 个 Gomokards Automation 组及旧 PvP 卡池、手牌、Confusion、Ghost、六机会 Tetris、网络隐私/权限测试保留并通过；新测试单独记录数量，不把 SP 规则改进旧期待值 |
| 可玩 UI | 一个人能完成落子/四卡/抽牌/满手替换/取消/AI 回合/结果/重开；牌堆数量可信、卡牌说明正确，AI 区域无假手牌；Esc/退出正常，未引入网络依赖 |

SP1 实施完成时给用户的手动验收清单应覆盖：确认起手 3 与牌堆数量；轮流测试落子、四种牌与 Draw 的整回合成本；抽到 5 后 Replace/取消；洗牌不丢卡；封堵不赠牌；AI 合法响应一次；胜负/重开/退出。代理负责代码、编译、自动化和静态/编辑器级验证，实际玩法由用户执行；SP0 不请求现在试玩。

#### 13. 仅以下规则阻塞 SP1 定案

| 待确认项 | 为什么确实影响首个原型 | 最小建议（尚未接受） |
| --- | --- | --- |
| 玩家棋色与先手 | 初始化、第一轮 AI 调度、UI/fixture 都依赖它 | 玩家执黑、黑先；SP1 不做选边 UI |
| 精确 6–8 张固定牌组及起手生成 | 实例总数、洗牌、平衡与验收需要确定输入 | 8 张，Polarity/Barrier/Nuke/Restock 各 2；建场洗牌后抽 3，测试固定种子 |
| Restock 满手溢出与抽牌不足 | “抽 3”与 Hand≤5 必须有一致的可见结果/RNG 行为 | 本牌先离手；只抽可容纳的至多 3，额外额度不取牌不弃牌；可用堆彻底无牌时停止。不同设计如抽满再弃溢出会消耗不同牌/RNG，必须明确选择 |
| 打出的 Restock 与被替换牌何时可参与此次回洗 | 抽牌堆恰好耗尽时可能立刻抽回同一张，影响守恒/期望结果 | 建议 Restock 完成效果后才进弃牌，避免本次回抽；Replace 按先弃再抽，允许在需要回洗时抽回所弃卡。确认这个时点即可，UI 无需额外架构决策 |
| 当前方没有合法主行动，尤其 AI 没有合法落点 | AI 不持牌，满盘/禁点可使其无处落子，而人类可能仍能出 Nuke/Draw；现有 AwaitingRuleDecision 不能冒充完整玩法 | 必须在和棋/跳过/其他明确规则中选择；SP0 不替用户决定。未决定前应保留明确未定状态，不能宣称完整 SP1 验收 |

Replace 必须原子、取消零变更已经是要求，不再询问；具体按钮位置/交互控件由实施时最小复用决定，不作为阻塞问题。未来棋诀满手奖励、Steal 期限/Exhaust/复制载体、Basics 与持续模式/Draw 的关系、Tetris 精确四块/预告/结束回合、新卡相邻/连线/临时期限定义、Ghost AI 记忆规则、遭遇六连如何配置，都可等 SP3 或相应功能真正进入范围时再讨论，不阻塞上述 SP1 子集。

#### 14. 本次验证与停止点

本次读取并追踪了 `MatchState/MatchRules`、卡定义/效果、TetrisRules、GameMode timer/Submit/Join/Leave/PublishViews、GameState 与公私投影、Controller 的输入/一致性/退出路径、Slate 棋盘/手牌/文案，以及相关 Core/卡牌/Ghost/Tetris/网络测试断言。现行规则以最新澄清与源码为准，例如 Basics 清 Confusion，不能误用早期 README 的历史保留规则。

**本次没有编译、没有运行 Automation、没有打开/操作游戏、没有重新打包。** 上次 **53 passed / 0 failed / 0 test warnings / 0 skipped** 是 `b975b63` 的已有验证基线，本次不冒充重跑结果。仅 README 追加本审查；没有源码、配置、资产、地图、测试或多人实现修改。最终建议为“少量前置接口拆分 + 并列单人 runtime”，不是零拆分硬套，也不是重写；SP1 等规则确认与新的实施授权后再开始。SP0 到此停止。


## 单人转向 SP0.5 — 多人旧代码隔离与干净基线

### 产品线与提交边界

- 多人源基线：`b975b6310128a4113d5a118acfc5f1d280586623`；本地和远端分支 `multiplayer-v0.1.2`、带注释标签 `demo-v0.1.2-multiplayer` 均指向该提交。删除源码前已核对远端分支与标签解引用 SHA。
- 活动分支：`singleplayer-roguelike`，从 SP0 文档提交 `48d10fd3b7503054effa0dcb62df035f112c030c` 创建。
- `ue-migration` 保留，不重写历史、不强制推送。多人产品通过 Git 保留，不再作为单人运行入口。
- 本阶段是源码重组，不是 SP1。没有实现牌堆、弃牌/消耗区、抽牌、换牌、Restock 抽三张、AI、Boss、单人回合循环、Run、商店、金币、棋诀或新卡。没有打包。

### 工程实现决定（不是新增玩法）

**运行代码与测试边界：** 删除 `Gomokards` 模块中的 `LocalMatchGameMode`、`LocalMatchPlayerController`、`LocalMatchGameState`、`MatchNetTypes` 四组头文件/实现，以及网络界面 `SLocalMatchView`。RPC、复制 DTO、私有手牌传输、座位与会话管理、Epoch/Revision、网络确认、网络 Ghost 遮蔽和 Tetris 姿态同步均退出活动源码。UE 自带网络代码未作裁剪。

新增 `GomokardsTests` 模块，类型为 `Editor`，在 `PostEngineInit` 加载，仅 Editor target 引用。旧卡牌事务、十张牌抽取池、六块 Tetris 轮换与旧提示文字移到其私有 `Legacy/` 目录，作为共享规则的回归夹具，不是活动单人玩法。原 `WITH_DEV_AUTOMATION_TESTS` 在 Development Game 中也可能开启，故仅用该宏不能保证产品隔离；独立 Editor 模块提供实际编译边界。Game 模块不依赖此模块，测试模块依赖共享 Core，规则实现没有复制成“单人版本”。

**保留的共享 Core：** `FBoard`/`FCell`、19×19 几何、黑白双方身份与索引、禁区、Barrier 连通性、胜负检查、成功阻挡判定、确定性状态/RNG、Ghost 显示状态转换、Tetris 形状/旋转/碰撞/四边出生选择/重力/同时消行。两侧棋盘逻辑不等于网络。

**三个小接口边界：**

1. `TryPlaceStone(FBoard&, Coordinate, Stone)` 验证并以候选副本提交落子，返回错误、`bSuccessfulBlock`、`bWinningLine`；不改手牌、随机数、行动次数、回合或模式。旧回归夹具调用同一接口后自行应用旧奖励，Game target 中没有旧随机发牌策略。阻挡检测只是一项规则信息，未实现防守反击。
2. `ApplyTacticalNuke`、`ApplyPolarity`、`ApplyBarrier` 只接收棋盘和目标，先验证再修改，不要求伪造手牌/玩家或 PlayCard 请求。旧卡牌夹具委托这些唯一实现；未来卡牌或 Boss 的事务验证需在调用者完成。
3. `StepTetrisPiece` 只返回 Rejected/Moved/Locked。锁定时写入棋子、调用现有消行逻辑并结束当前块，不选择下一块或轮换操作者；旧六块轮换留在测试夹具。`ApplyTetrisInput` 的绝对方向、禁止逆重力、旋转与碰撞规则保持。新运行时和计时器仍待未来阶段定义。

**资源状态：** 暂留 `FPlayerState.Hand` 及历史效果字段以保持已有确定性状态比较、组合回归夹具和 Tetris/Ghost 规则测试；它们不是 SP 手牌/牌堆接口。活动运行代码没有旧抽牌、偷牌、换手牌或出牌事务入口，只有 Editor 私有夹具写入旧 Hand。SP1 必须显式拥有自己的战斗资源状态，不得把旧 Hand 当作 DrawPile/DiscardPile 系统。本阶段未提前创建 FRunState 或任何新资源容器。

**卡牌定义：** 保留稳定枚举 ID；运行模块只保留三个棋盘效果的目标域元数据，不提供玩家可玩目录或抽牌池。十张旧牌的贴图引用是历史素材目录，不代表单人玩家牌池。单人角色边界记录为：两极反转/阴阳屏障/战术核弹是玩家与 Boss 共享效果；补充库存/取之有道（未来偷 Boss 特性）/回归基本功/俄罗斯方块为玩家牌；定位混淆与战术换家退出单人目录；幽灵棋子为 Boss 能力。以上新语义尚未实现。

**表现层：** 从旧界面抽出 `SMatchBoard`，保留棋盘、棋子、禁区、Barrier、目标预览、Ghost 灰色显示与 Tetris 几何绘制。`SDemoCard`/`SDemoHand` 保留竖版卡面、中文字体、贴图占位接口、重叠/悬停及焦点回送；接收只读显示属性和点击/取消委托，不读取 Controller、复制快照或手牌权威状态。手牌点击返回槽位索引，避免重复卡牌 ID 混淆。移除对方隐藏手牌、远程身份、网络记录与确认反馈。完整单人界面尚未组合；键盘映射 helper 保留，Escape/游戏退出与屏幕焦点的应用层绑定留给后续真实界面。

**地图/启动：** `LocalMatch.umap` 无被移除项目类的 GameMode 覆盖；不改二进制资产。`GlobalDefaultGameMode` 暂指向引擎 `GameModeBase`，默认地图不变。不为对称性新建空项目 GameMode。当前启动只是空架构基线，不应期待旧 Demo UI、可玩对局或 LAN 入口；测试加载原地图核对资产及覆盖引用。

### 测试迁移审计

以下网络组从活动分支删除，完整旧测试继续存在于冻结分支。表中的 Phase 1/2/3 引用现在都位于 Editor 专用 `Gomokards.LegacyFixtures.*`，不是单人产品验收规则。

| 删除的旧组 | 共享断言去向；不迁移部分 |
|---|---|
| Phase4A.ProjectionSeparation | 棋盘/身份状态保留于 Phase1.MatchAndReset、Phase3A.ConfusionIdentityAndReward；复制公开/私有投影不迁移 |
| Phase4A.ReflectedPrivacyAndRpcContract | 仅反射、复制条件及 RPC 合同，不属于共享规则，不迁移 |
| Phase4A.AuthorityLifecycleAndPresentation | Phase1.InvalidActionsAtomic/BlockingMatrix/WinMatrixAndTerminal 保留拒绝、奖励夹具与胜负；共享点击迁到 Shared.PresentationIntentCallbacks；座位、断连、网络重启及版本协调不迁移 |
| Phase4B1.CardRpcAndWhitelist | 旧牌池覆盖在 Phase1.DeterminismAndPool、Phase3A.PoolAndDeterminism；RPC 白名单与反射不迁移 |
| Phase4B1.CardAuthorityPrivacyAndCoherence | Phase1.CardTransactions、InvalidActionsAtomic 保留旧手牌事务/顺序/RNG 原子性；私有传输和确认不迁移 |
| Phase4B2.TargetRpcAndGeometry | 全 361 点 Nuke、18×18 Polarity/Barrier、边界及十字中心断言提取到 Shared.TargetGeometry；RPC 合同不迁移 |
| Phase4B2.AuthorityOutcomesAndLocalTargeting | Phase1.NukeTransactions、Phase3A.PolarityAndDraw/BarrierTopology/BarrierWinningLines、Phase2.TargetingIntent 保留效果、胜负、取消与原子性；Shared.BoardEffectsWithoutHand 直接检查无手牌接口；网络状态不迁移 |
| Phase4B3.PersistentAuthorityAndRecipes | Phase3A.ConfusionLifetime/ConfusionIdentityAndReward/BackToBasics/ReachableManualSetups 已覆盖共享计数、身份不变及不回滚棋盘历史；网络配方发布和私有视图不迁移 |
| Phase5A.GhostTransportAuthorityAndLifecycle | Phase3B 的 7 个 Core 组保留隐藏落子计数、胜负延后、真颜色/奖励、持久化元数据与重置；另将网络夹具独有的“隐藏阶段最后合法位置用尽，不提前显色/虚构胜负”断言迁入 HiddenCountingAndRestrictions；遮蔽传输、会话结束和计时回调不迁移 |
| Phase5B.TetrisAuthorityPoseAndLifecycle | Phase3C 的 10 个 Core 组保留四向移动拒绝原子性、碰撞、Barrier/禁区、出生、消行、轮换夹具和 RNG；新增 Shared.TetrisPhysicalStep 验证纯物理接口；姿态排序、token 和网络计时器不迁移 |

另删除 `Phase2.RuntimeOwner`、`Phase3B.RuntimeTimerLifecycle`、`Phase3C.RuntimeGravityDeadlineAndLifecycle`：其所属运行对象和计时器已删除；状态重置/拒绝/模式转换保留在 Core 夹具中，不能把旧计时器测试通过冒充新单人运行时验证。

旧 DemoUI 三组处理：`ChineseCardsAndCookableAssets` 的 11 个贴图、嵌入源与中文字体检查迁到 `Shared.FontArtAndStartupAssets`，旧十牌中文描述和对手身份/手牌计数不迁移；`LogPrivacyAndDisplayPriority` 随复制日志删除；`AcceptedPublicCardAndLocalTargeting` 的公开卡牌发布测试删除，本地意图/取消由 Phase2.TargetingIntent 和新显示回调测试覆盖。

保留 37 个历史 Core/表现夹具组，新建 6 个共享组；所有测试仅在 Editor 模块编译。旧整套 53 组减去 10 个网络组、3 个运行时组、3 个 DemoUI 组，再增加 6 组，共 43 组。当前完整测试清单：

```text
Gomokards.LegacyFixtures.Phase1.BlockingMatrix
Gomokards.LegacyFixtures.Phase1.CardTransactions
Gomokards.LegacyFixtures.Phase1.DeterminismAndPool
Gomokards.LegacyFixtures.Phase1.InvalidActionsAtomic
Gomokards.LegacyFixtures.Phase1.MatchAndReset
Gomokards.LegacyFixtures.Phase1.NukeTransactions
Gomokards.LegacyFixtures.Phase1.RepeatedBlocking
Gomokards.LegacyFixtures.Phase1.UnresolvedNoLegalAction
Gomokards.LegacyFixtures.Phase1.WinMatrixAndTerminal
Gomokards.LegacyFixtures.Phase2.BoardCoordinates
Gomokards.LegacyFixtures.Phase2.TargetingIntent
Gomokards.LegacyFixtures.Phase3A.BackToBasics
Gomokards.LegacyFixtures.Phase3A.BarrierTopology
Gomokards.LegacyFixtures.Phase3A.BarrierWinningLines
Gomokards.LegacyFixtures.Phase3A.ConfusionIdentityAndReward
Gomokards.LegacyFixtures.Phase3A.ConfusionLifetime
Gomokards.LegacyFixtures.Phase3A.PolarityAndDraw
Gomokards.LegacyFixtures.Phase3A.PoolAndDeterminism
Gomokards.LegacyFixtures.Phase3A.PresentationDomains
Gomokards.LegacyFixtures.Phase3A.ReachableManualSetups
Gomokards.LegacyFixtures.Phase3B.ActivationAndPreparation
Gomokards.LegacyFixtures.Phase3B.HiddenCountingAndRestrictions
Gomokards.LegacyFixtures.Phase3B.PersistentEffectsAndReset
Gomokards.LegacyFixtures.Phase3B.TrueColorRewardsAndConfusion
Gomokards.LegacyFixtures.Phase3B.VisibilityAndPool
Gomokards.LegacyFixtures.Phase3B.WinSuppressionAndReveal
Gomokards.LegacyFixtures.Phase3B.WinningHiddenRewards
Gomokards.LegacyFixtures.Phase3C.AbsoluteMovementAndCollision
Gomokards.LegacyFixtures.Phase3C.ActivationAndEffects
Gomokards.LegacyFixtures.Phase3C.CrowdedFallbackAndSkippedBlocks
Gomokards.LegacyFixtures.Phase3C.FinalClearAdjudicationAndReset
Gomokards.LegacyFixtures.Phase3C.FourEdgeSpawnAndClearance
Gomokards.LegacyFixtures.Phase3C.GravityAndLocks
Gomokards.LegacyFixtures.Phase3C.PoolShapeSamplingAndPresentation
Gomokards.LegacyFixtures.Phase3C.ShapesAndRotation
Gomokards.LegacyFixtures.Phase3C.SimultaneousConnectedLineClear
Gomokards.LegacyFixtures.Phase3C.SixOperatorsAndDeterminism
Gomokards.Shared.BoardEffectsWithoutHand
Gomokards.Shared.FontArtAndStartupAssets
Gomokards.Shared.PlacementWithoutReward
Gomokards.Shared.PresentationIntentCallbacks
Gomokards.Shared.TargetGeometry
Gomokards.Shared.TetrisPhysicalStep
```

### SP1 冻结产品规则（仅记录，尚未实现）

1. 玩家为黑方，先手。
2. 原型初始牌组恰为 8 张：两极反转×2、阴阳屏障×2、战术核弹×2、补充库存×2；不是最终平衡。
3. 起手 3 张，上限 5 张。
4. 玩家每回合只接受一次主行动：落子、出牌、抽牌/满手换牌；成功行动结束玩家回合。
5. 未满 5 张时，Draw 抽 1 张并结束回合。
6. 满 5/5 时 Draw 变 Replace：选择恰好一张牌，事务内先移出手牌，抽一张替换牌；抽牌事务结束后才将旧牌放入弃牌堆，然后结束回合。正在替换的牌不能参加本次洗牌并立即被抽回。无效或取消不得改状态。
7. 普通非 Exhaust 卡先从手中移出，完整结算效果及内部抽牌/洗牌后，才进入弃牌堆；Restock 不能在自己的效果中洗回并抽回自身。
8. 单人 Restock 抽 3 张，但只抽至手牌上限 5；装不下的牌不抽、不丢弃、不烧毁，留在正常牌序中。
9. 只有仍需抽牌且 DrawPile 为空时，才将可用 DiscardPile 确定性洗回。排除正在结算的出牌、替换牌和 ExhaustPile；RNG 由权威状态拥有。
10. Exhaust 表示本场战斗移除，之后下一场归还；SP1 四种基础牌无须 Exhaust 行为。本阶段没有实现这些容器。
11. 成功阻挡仍可检测，单人基线不自动发牌；防守反击留待未来。
12. 尚无胜方且 AI 无合法落子位置时，SP1 判 Draw；不引入跳过回合。本阶段没有实现 AI 或该对局循环。

### 验证与限制

- 2026-10-09，UE 5.8.2，Win64 Development Editor：**通过**。构建前确认编辑器未运行，核验路径后删除本项目 `Intermediate` 和 `Binaries`；重新编译全部项目运行/Editor 测试源码。首轮新增 UI 测试的 Slate 坐标构造出现 C2665 类型歧义，改为显式 `FVector2D` 后修复；最终 Editor 构建成功。
- Win64 Development Game：**通过**，生成 `Gomokards.exe`；未 Cook、Stage 或 Package。
- 完整 `Automation RunTests Gomokards`：**43 通过、0 失败、0 测试警告、0 跳过/未运行、0 进行中**；其中历史 Core 夹具 37 组、共享接口 6 组。启动地图、贴图、字体、显示委托、无手牌棋盘效果和物理锁定接口均通过。不是人工玩法验收。
- 测试日志之外的 Editor 初始化日志有 **1 条布局版本兼容警告**（`UnrealEd_Layout_v1.5/v1.6`），不是测试失败；未修改布局配置入库。日志没有 Error 记录。最终 Editor/Game 编译日志没有 C++ warning/error。
- 源码/配置搜索指定项目网络符号：**0 残留**。扩大搜索 network/replication/server/client/RPC/epoch/session 后仅命中 Editor 测试的一条历史迁移说明；`LocalMatch` 仅为保留地图资源名，均非网络实现。
- 实际 Game 链接响应文件 `Gomokards.exe.rsp` 只有 10 个项目运行源文件对应的对象：模块入口、CardDefinitions、MatchState、MatchRules、BoardEffects、TetrisRules、MatchPresentation、DemoPresentation、SDemoCards、SMatchBoard；没有 GomokardsTests、Legacy、原网络 Runtime 对象或测试对象。Editor 测试模块未进入 Game target。此结论不是包体大小估算，未裁剪引擎/插件网络能力。
- 暂存差异检查通过：没有 Binaries/Intermediate/Saved/DDC、日志、缓存或二进制资产修改。所有本次工程产物、验证日志与报告位于 G 盘；本机报告为 `G:\GameDev\Logs\SP05\Automation\index.json`。

当前是可构建的共享规则/绘制基线，不是可玩的单人版本。旧多人完整功能请切换冻结分支；不宣称单人玩法、LAN、Internet 或恶意 Host 保密验证。没有运行手动 gameplay，没有打包。本阶段按请求无需手工验收门槛；后续 gameplay 阶段仍由用户执行手动清单。

### 本机切换与干净 Editor 构建

先保存并关闭 Unreal Editor。以下命令假定采用当前 G 盘开发目录；clean 只删除该项目的两个生成目录，若工作区有未提交内容，先自行保存，勿强制切换。

```powershell
. G:\GameDev\Tools\Set-DevEnvironment.ps1
Set-Location -LiteralPath 'G:\GameDev\Projects\Gomokards-mini-game'
git fetch origin
git switch singleplayer-roguelike
git pull --ff-only origin singleplayer-roguelike

$spProjectRoot = (Resolve-Path -LiteralPath '.\Gomokards').Path
foreach ($spDir in @('Intermediate','Binaries')) {
    $spPath = Join-Path $spProjectRoot $spDir
    if (Test-Path -LiteralPath $spPath) {
        $spResolved = (Resolve-Path -LiteralPath $spPath).Path
        if ($spResolved -ne ($spProjectRoot + '\' + $spDir)) { throw 'Clean path mismatch' }
        Remove-Item -LiteralPath $spResolved -Recurse -Force
    }
}
& 'G:\GameDev\Unreal\UE_5.8\Engine\Build\BatchFiles\Build.bat' GomokardsEditor Win64 Development '-Project=G:\GameDev\Projects\Gomokards-mini-game\Gomokards\Gomokards.uproject' -WaitMutex -NoHotReloadFromIDE
```
