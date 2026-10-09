#include "Singleplayer/SingleplayerBattle.h"
#include "Singleplayer/SingleplayerAI.h"
#include "Singleplayer/SingleplayerPresentation.h"
#include "Singleplayer/SingleplayerBattleGameMode.h"
#include "Singleplayer/SingleplayerBattlePlayerController.h"
#include "Core/BoardEffects.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace Gomokards;
namespace
{
constexpr EAutomationTestFlags SPFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
FSingleplayerCard Card(int32 Instance)
{
    const ECardId Types[] = {ECardId::Polarity, ECardId::Barrier, ECardId::TacticalNuke, ECardId::Restock};
    return {Instance, Types[Instance / 2]};
}
FSingleplayerBattleState Fixture(std::initializer_list<int32> Hand, std::initializer_list<int32> Draw,
    std::initializer_list<int32> Discard = {}, std::initializer_list<int32> Exhaust = {})
{
    FSingleplayerBattleState State(71);
    State.PlayerDeck = {};
    for (int32 Id : Hand) { State.PlayerDeck.Hand.Add(Card(Id)); }
    for (int32 Id : Draw) { State.PlayerDeck.DrawPile.Add(Card(Id)); }
    for (int32 Id : Discard) { State.PlayerDeck.DiscardPile.Add(Card(Id)); }
    for (int32 Id : Exhaust) { State.PlayerDeck.ExhaustPile.Add(Card(Id)); }
    return State;
}
bool Conserved(const FSingleplayerBattleState& State)
{
    TSet<int32> Seen;
    for (const auto* Pile : {&State.PlayerDeck.Hand, &State.PlayerDeck.DrawPile, &State.PlayerDeck.DiscardPile, &State.PlayerDeck.ExhaustPile})
    for (const auto& Entry : *Pile)
    {
        if (Entry.InstanceId < 0 || Entry.InstanceId >= 8 || Seen.Contains(Entry.InstanceId) || !(Entry == Card(Entry.InstanceId))) { return false; }
        Seen.Add(Entry.InstanceId);
    }
    return Seen.Num() == 8 && State.PlayerDeck.Hand.Num() <= 5;
}
bool LegacyInactive(const FSingleplayerBattleState& State)
{
    const auto& Match = State.Match;
    return Match.Players.Num() == 2 && Match.Players[0].Hand.IsEmpty() && Match.Players[1].Hand.IsEmpty()
        && Match.ConfusionActionsRemaining == 0 && !Match.bCardsDisabled && Match.GhostPhase == EGhostPhase::None
        && Match.GhostPlacementsCompleted == 0 && Match.Tetris == FTetrisState{};
}
FSingleplayerActionRequest Request(const FSingleplayerBattleState& State, ESingleplayerActionType Type, int32 Instance = INDEX_NONE,
    TOptional<FIntPoint> Target = {})
{ return {Type, State.Match.CompletedActions, Instance, Target}; }
void CheckRejected(FAutomationTestBase& Test, FSingleplayerBattleState& State, const FSingleplayerActionRequest& Action)
{
    const auto Before = State;
    Test.TestFalse(TEXT("Request rejected"), ResolvePlayerAction(State, Action).IsAccepted());
    Test.TestTrue(TEXT("Full state, piles, both RNG states, counters unchanged"), State == Before);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInitialization, "Gomokards.Singleplayer.Initialization", SPFlags)
bool FSPInitialization::RunTest(const FString&)
{
    FSingleplayerBattleState State(17);
    TestTrue(TEXT("Player black, AI white, player first"), State.Match.Players[0].AssignedStone == EStone::Black
        && State.Match.Players[1].AssignedStone == EStone::White && State.Match.CurrentPlayerIndex == 0);
    TestTrue(TEXT("Empty board, in progress, zero actions"), State.Match.Board == FBoard{} && State.Match.Result == FMatchResult{} && State.Match.CompletedActions == 0);
    TestEqual(TEXT("Starting hand"), State.PlayerDeck.Hand.Num(), 3);
    TestEqual(TEXT("Initial draw pile"), State.PlayerDeck.DrawPile.Num(), 5);
    TestTrue(TEXT("Exact eight-card multiset with distinct instances"), Conserved(State));
    TestTrue(TEXT("Legacy resources and modes inactive"), LegacyInactive(State));
    TestTrue(TEXT("Same seed reproduces complete state"), State == FSingleplayerBattleState(17));
    bool bDifferent = false;
    for (int32 Seed = 18; Seed < 30; ++Seed) { bDifferent |= !(State.PlayerDeck == FSingleplayerBattleState(Seed).PlayerDeck); }
    TestTrue(TEXT("Other seeds can change shuffle"), bDifferent);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPDraw, "Gomokards.Singleplayer.DrawAtomicity", SPFlags)
bool FSPDraw::RunTest(const FString&)
{
    FSingleplayerBattleState State(3);
    const auto Before = State;
    const auto Result = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::Draw));
    TestTrue(TEXT("Draw one, one action, AI turn"), Result.IsAccepted() && Result.CardsDrawn == 1
        && State.PlayerDeck.Hand.Num() == 4 && State.Match.CompletedActions == 1 && State.Match.CurrentPlayerIndex == 1);
    TestTrue(TEXT("Drawing from existing pile consumes no RNG or board state"), State.DeckRandom.GetCurrentSeed() == Before.DeckRandom.GetCurrentSeed() && State.Match.Board == Before.Match.Board);
    TestTrue(TEXT("Conservation and old fields"), Conserved(State) && LegacyInactive(State));
    CheckRejected(*this, State, Request(State, ESingleplayerActionType::Draw));
    State = Fixture({0,1,2}, {}, {}, {3,4,5,6,7});
    CheckRejected(*this, State, Request(State, ESingleplayerActionType::Draw));
    State = Fixture({0,1,2,3,4}, {5,6,7});
    CheckRejected(*this, State, Request(State, ESingleplayerActionType::Draw));
    State = FSingleplayerBattleState(3);
    auto Stale = Request(State, ESingleplayerActionType::Draw); Stale.ExpectedActions = 9;
    CheckRejected(*this, State, Stale);
    CheckRejected(*this, State, Request(State, ESingleplayerActionType::Draw, 0));
    CheckRejected(*this, State, Request(State, static_cast<ESingleplayerActionType>(255)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPReplace, "Gomokards.Singleplayer.ReplaceAtomicity", SPFlags)
bool FSPReplace::RunTest(const FString&)
{
    auto State = Fixture({0,1,2,3,4}, {}, {5,6}, {7});
    const auto Result = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::Replace, 0));
    TestTrue(TEXT("Accepted replacement has five cards, one action and AI turn"), Result.IsAccepted() && Result.CardsDrawn == 1
        && State.PlayerDeck.Hand.Num() == 5 && State.Match.CompletedActions == 1 && State.Match.CurrentPlayerIndex == 1);
    TestTrue(TEXT("Exact selected instance discarded after reshuffle; same-type other copy remains"), State.PlayerDeck.DiscardPile == TArray<FSingleplayerCard>{Card(0)}
        && State.PlayerDeck.Hand.Contains(Card(1)) && !State.PlayerDeck.Hand.Contains(Card(0)));
    TestTrue(TEXT("No lost or duplicated instances; Exhaust excluded"), Conserved(State) && State.PlayerDeck.ExhaustPile == TArray<FSingleplayerCard>{Card(7)});
    State = Fixture({0,1,2,3,4}, {}, {}, {5,6,7});
    CheckRejected(*this, State, Request(State, ESingleplayerActionType::Replace, 0));
    CheckRejected(*this, State, Request(State, ESingleplayerActionType::Replace, 99));
    State = Fixture({0,1,2}, {3,4,5,6,7});
    CheckRejected(*this, State, Request(State, ESingleplayerActionType::Replace, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPReshuffle, "Gomokards.Singleplayer.ReshuffleConservation", SPFlags)
bool FSPReshuffle::RunTest(const FString&)
{
    auto State = Fixture({0,1,2}, {}, {3,4,5,6}, {7});
    auto Twin = State;
    TestTrue(TEXT("Deterministic discard reshuffle"), ResolvePlayerAction(State, Request(State, ESingleplayerActionType::Draw)).IsAccepted()
        && ResolvePlayerAction(Twin, Request(Twin, ESingleplayerActionType::Draw)).IsAccepted() && State == Twin);
    TestTrue(TEXT("Discard moved, Exhaust unchanged"), State.PlayerDeck.DiscardPile.IsEmpty() && State.PlayerDeck.DrawPile.Num() == 3
        && State.PlayerDeck.ExhaustPile == TArray<FSingleplayerCard>{Card(7)} && Conserved(State));
    State = Fixture({0,1,2,3,6}, {}, {4,5}, {7});
    CheckRejected(*this, State, Request(State, ESingleplayerActionType::PlayCard, 0, FIntPoint(18,18)));
    // Repeated explicit transactions cross multiple reshuffles without manufacturing cards.
    for (int32 I = 0; I < 24; ++I)
    {
        TestTrue(TEXT("Replace transaction conserved"), ResolvePlayerAction(State, Request(State, ESingleplayerActionType::Replace, State.PlayerDeck.Hand[0].InstanceId)).IsAccepted() && Conserved(State));
        TestTrue(TEXT("Independent explicit AI placement"), ResolveAIPlacement(State, FIntPoint((I%4)*4, (I/4)*3), State.Match.CompletedActions).IsAccepted());
        TestTrue(TEXT("Legacy fields inactive"), LegacyInactive(State));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPRestock, "Gomokards.Singleplayer.RestockTimingAndCap", SPFlags)
bool FSPRestock::RunTest(const FString&)
{
    auto State = Fixture({0,1,6}, {2,3,4,5,7});
    const auto Result = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlayCard, 6));
    TestTrue(TEXT("Three before play becomes five after drawing three"), Result.IsAccepted() && Result.CardsDrawn == 3 && State.PlayerDeck.Hand.Num() == 5);
    TestTrue(TEXT("Remainder stays in order; card discarded last"), State.PlayerDeck.DrawPile == TArray<FSingleplayerCard>{Card(2),Card(3)}
        && State.PlayerDeck.DiscardPile == TArray<FSingleplayerCard>{Card(6)} && Conserved(State));
    State = Fixture({0,1,2,3,6}, {4,5,7});
    const auto Before = State;
    const auto AtCap = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlayCard, 6));
    TestTrue(TEXT("Five before play draws only one, ends once"), AtCap.IsAccepted() && AtCap.CardsDrawn == 1 && State.Match.CompletedActions == 1
        && State.Match.CurrentPlayerIndex == 1 && State.PlayerDeck.Hand.Num() == 5);
    TestTrue(TEXT("Unneeded cards neither drawn nor shuffled"), State.PlayerDeck.DrawPile == TArray<FSingleplayerCard>{Card(4),Card(5)}
        && State.DeckRandom.GetCurrentSeed() == Before.DeckRandom.GetCurrentSeed());
    State = Fixture({0,6}, {}, {1,2,3,4,5}, {7});
    const auto Reshuffle = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlayCard, 6));
    TestTrue(TEXT("Restock excluded from own reshuffle"), Reshuffle.IsAccepted() && Reshuffle.CardsDrawn == 3 && !State.PlayerDeck.Hand.Contains(Card(6))
        && State.PlayerDeck.DiscardPile == TArray<FSingleplayerCard>{Card(6)} && Conserved(State));
    State = Fixture({6}, {}, {0}, {1,2,3,4,5,7});
    TestEqual(TEXT("Insufficient drawable cards stop at one"), ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlayCard, 6)).CardsDrawn, 1);
    TestTrue(TEXT("Cannot cycle resolving Restock"), Conserved(State) && State.PlayerDeck.Hand == TArray<FSingleplayerCard>{Card(0)});
    State = Fixture({6}, {}, {}, {0,1,2,3,4,5,7});
    TestTrue(TEXT("Restock may validly draw zero"), ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlayCard, 6)).IsAccepted() && State.PlayerDeck.Hand.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlacement, "Gomokards.Singleplayer.PlacementNoRewardAndTerminal", SPFlags)
bool FSPPlacement::RunTest(const FString&)
{
    FSingleplayerBattleState State(17);
    State.Match.Board.At({1,0}).Stone = EStone::White;
    State.Match.Board.At({2,0}).Stone = EStone::Black;
    const auto Before = State;
    auto Result = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlaceStone, INDEX_NONE, FIntPoint(0,0)));
    TestTrue(TEXT("Black placement reports successful block"), Result.IsAccepted() && Result.bSuccessfulBlock && State.Match.Board.At({0,0}).Stone == EStone::Black);
    TestTrue(TEXT("No blocking reward, no deck or RNG changes"), State.PlayerDeck == Before.PlayerDeck
        && State.DeckRandom.GetCurrentSeed() == Before.DeckRandom.GetCurrentSeed() && State.Match.Random.GetCurrentSeed() == Before.Match.Random.GetCurrentSeed());
    TestTrue(TEXT("Only one completed action"), State.Match.CompletedActions == 1 && State.Match.CurrentPlayerIndex == 1 && LegacyInactive(State));
    State = Before;
    State.Match.Board.At({3,3}).bForbidden = true;
    for (FIntPoint Point : {FIntPoint(1,0), FIntPoint(3,3), FIntPoint(-1,0), FIntPoint(19,0)})
    { CheckRejected(*this, State, Request(State, ESingleplayerActionType::PlaceStone, INDEX_NONE, Point)); }
    State = FSingleplayerBattleState(17);
    for (int32 X=0; X<4; ++X) { State.Match.Board.At({X,0}).Stone = EStone::Black; }
    State.Match.Board.At({4,1}).Stone = EStone::White;
    State.Match.Board.At({4,2}).Stone = EStone::Black;
    const auto DeckBeforeWin = State.PlayerDeck;
    Result = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlaceStone, INDEX_NONE, FIntPoint(4,0)));
    TestTrue(TEXT("Winning block still gives no reward and never transfers turn"), Result.IsAccepted() && Result.bSuccessfulBlock
        && Result.Result.Status == EMatchStatus::Won && Result.Result.WinningStone == EStone::Black
        && State.Match.CurrentPlayerIndex == 0 && State.PlayerDeck == DeckBeforeWin);
    const auto Terminal = State;
    TestFalse(TEXT("AI cannot move after terminal player action"), ResolveAIPlacement(State, FIntPoint(8,8), State.Match.CompletedActions).IsAccepted());
    TestTrue(TEXT("Terminal AI rejection atomic"), State == Terminal);
    for (auto Type : {ESingleplayerActionType::PlaceStone, ESingleplayerActionType::Draw, ESingleplayerActionType::Replace, ESingleplayerActionType::PlayCard})
    { CheckRejected(*this, State, Request(State, Type)); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPBoardCards, "Gomokards.Singleplayer.BoardCardsAndPolarityResults", SPFlags)
bool FSPBoardCards::RunTest(const FString&)
{
    for (int32 Instance : {0,2,4})
    {
        auto State = Fixture({0,1,2,3,4}, {5,6,7});
        State.Match.Board.At({4,4}).Stone = EStone::Black;
        State.Match.Board.At({5,5}).Stone = EStone::White;
        const FIntPoint Target(4,4);
        auto Expected = State.Match.Board;
        if (Instance == 0) { ApplyPolarity(Expected, Target); }
        if (Instance == 2) { ApplyBarrier(Expected, Target); }
        if (Instance == 4) { ApplyTacticalNuke(Expected, Target); }
        const auto Result = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlayCard, Instance, Target));
        TestTrue(TEXT("Effect matches shared primitive exactly"), Result.IsAccepted() && State.Match.Board == Expected);
        TestTrue(TEXT("Only selected instance consumed, discarded after resolution"), State.PlayerDeck.DiscardPile == TArray<FSingleplayerCard>{Card(Instance)}
            && !State.PlayerDeck.Hand.Contains(Card(Instance)) && Conserved(State) && State.Match.CompletedActions == 1);
        State = Fixture({0,1,2,3,4}, {5,6,7});
        CheckRejected(*this, State, Request(State, ESingleplayerActionType::PlayCard, Instance, FIntPoint(19,19)));
        if (Instance != 4) { CheckRejected(*this, State, Request(State, ESingleplayerActionType::PlayCard, Instance, FIntPoint(18,18))); }
        CheckRejected(*this, State, Request(State, ESingleplayerActionType::PlayCard, 999, Target));
        CheckRejected(*this, State, Request(State, ESingleplayerActionType::PlayCard, Instance));
    }
    for (EStone Winner : {EStone::Black, EStone::White, EStone::Empty})
    {
        auto State = Fixture({0,1,2}, {3,4,5,6,7});
        for (int32 X=0; X<5; ++X)
        {
            if (Winner == EStone::Black || Winner == EStone::Empty) { State.Match.Board.At({X,4}).Stone = X == 4 ? EStone::White : EStone::Black; }
            if (Winner == EStone::White || Winner == EStone::Empty) { State.Match.Board.At({X,5}).Stone = X == 4 ? EStone::Black : EStone::White; }
        }
        const auto Result = ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlayCard, 0, FIntPoint(4,4)));
        TestTrue(TEXT("Polarity global winner or simultaneous draw"), Result.IsAccepted() && Result.Result.Status == (Winner == EStone::Empty ? EMatchStatus::Draw : EMatchStatus::Won)
            && Result.Result.WinningStone == Winner && State.Match.CurrentPlayerIndex == 0);
        TestTrue(TEXT("Terminal card still discarded"), State.PlayerDeck.DiscardPile.Contains(Card(0)) && Conserved(State));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPAI, "Gomokards.Singleplayer.AIPolicyAndTurnSequence", SPFlags)
bool FSPAI::RunTest(const FString&)
{
    FBoard Board;
    for (int32 X=0; X<4; ++X) { Board.At({X,5}).Stone = EStone::White; Board.At({X,10}).Stone = EStone::Black; }
    TestTrue(TEXT("Immediate White win outranks defense"), ChooseSingleplayerMove(Board).GetValue() == FIntPoint(4,5));
    ApplyBarrier(Board, {1,5});
    TestTrue(TEXT("Barrier breaks White threat; blocks Black instead"), ChooseSingleplayerMove(Board).GetValue() == FIntPoint(4,10));
    FSingleplayerBattleState State(17);
    TestTrue(TEXT("Player center opening accepted"), ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlaceStone, INDEX_NONE, FIntPoint(9,9))).IsAccepted());
    const auto Before = State;
    const auto Intent = ChooseSingleplayerMove(State.Match.Board);
    TestTrue(TEXT("Policy produces intent without touching any battle data"), Intent.IsSet() && State == Before);
    TestTrue(TEXT("AI resolves using authority"), ResolveAIPlacement(State, Intent, State.Match.CompletedActions).IsAccepted());
    TestTrue(TEXT("Full round has two actions and player turn, no automatic draw"), State.Match.CurrentPlayerIndex == 0 && State.Match.CompletedActions == 2
        && State.PlayerDeck == Before.PlayerDeck && State.Match.Board.At(Intent.GetValue()).Stone == EStone::White && LegacyInactive(State));
    const auto After = State;
    TestFalse(TEXT("Duplicate AI rejected"), ResolveAIPlacement(State, Intent, 1).IsAccepted());
    TestTrue(TEXT("Duplicate AI atomic"), State == After);
    State = FSingleplayerBattleState(17); State.Match.CurrentPlayerIndex = 1;
    for (int32 X=0; X<4; ++X) { State.Match.Board.At({X,0}).Stone = EStone::White; }
    TestTrue(TEXT("Authoritative AI win"), ResolveAIPlacement(State, ChooseSingleplayerMove(State.Match.Board), 0).IsAccepted()
        && State.Match.Result == FMatchResult{EMatchStatus::Won,EStone::White} && State.Match.CurrentPlayerIndex == 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPAIDeterminism, "Gomokards.Singleplayer.AIDeterminismAndNoPlacement", SPFlags)
bool FSPAIDeterminism::RunTest(const FString&)
{
    FSingleplayerBattleState A(11), B(88);
    A.Match.Board.At({9,9}).bForbidden = true;
    A.Match.Board.At({9,8}).Stone = EStone::Black;
    B.Match.Board = A.Match.Board;
    const auto Before = A;
    const auto Move = ChooseSingleplayerMove(A.Match.Board);
    TestTrue(TEXT("Same observable board independent of deck seed"), Move == ChooseSingleplayerMove(B.Match.Board) && Move == ChooseSingleplayerMove(A.Match.Board));
    TestTrue(TEXT("Legal deterministic move, no hidden RNG sampling"), Move.IsSet() && A.Match.Board.At(Move.GetValue()).Stone == EStone::Empty
        && !A.Match.Board.At(Move.GetValue()).bForbidden && A == Before);
    A.Match.CurrentPlayerIndex = 1;
    auto Snapshot = A;
    TestFalse(TEXT("Missing intent with legal board rejected"), ResolveAIPlacement(A, {}, 0).IsAccepted());
    TestFalse(TEXT("Stale AI action rejected"), ResolveAIPlacement(A, Move, 99).IsAccepted());
    TestFalse(TEXT("Forbidden AI placement rejected"), ResolveAIPlacement(A, FIntPoint(9,9), 0).IsAccepted());
    TestTrue(TEXT("AI rejections preserve everything"), A == Snapshot);
    for (auto& Cell : A.Match.Board.Cells) { Cell = {EStone::Empty, true}; }
    TestFalse(TEXT("No legal coordinate"), ChooseSingleplayerMove(A.Match.Board).IsSet());
    const auto NoMove = ResolveAIPlacement(A, {}, 0);
    TestTrue(TEXT("No AI placement ends Draw without fake action"), NoMove.IsAccepted() && A.Match.Result.Status == EMatchStatus::Draw && A.Match.CompletedActions == 0);
    B.Match.Board = A.Match.Board;
    TestTrue(TEXT("Full/forbidden board still allows human cards or draw"), HasPlayerMainAction(B));
    B.PlayerDeck = {}; TestFalse(TEXT("Query exposes truly empty no-action fixture"), HasPlayerMainAction(B));
    // Existing winner must not be replaced by a no-placement draw in defensive fixture handling.
    A = FSingleplayerBattleState(17); A.Match.CurrentPlayerIndex = 1;
    for (auto& Cell : A.Match.Board.Cells) { Cell.bForbidden = true; }
    for (int32 X=0; X<5; ++X) { A.Match.Board.At({X,0}) = {EStone::Black,false}; }
    TestTrue(TEXT("Existing winner preserved"), ResolveAIPlacement(A, {}, 0).IsAccepted() && A.Match.Result.WinningStone == EStone::Black);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPSelection, "Gomokards.Singleplayer.SelectionAndDisplayBoundary", SPFlags)
bool FSPSelection::RunTest(const FString&)
{
    auto State = Fixture({0,1,6}, {2,3,4,5,7});
    const auto Before = State;
    const auto View = MakeSingleplayerView(State, 7);
    FSingleplayerSelection Selection;
    TestFalse(TEXT("Target selection only"), Selection.HandClick(View, 0).IsSet());
    TestTrue(TEXT("Exact instance selected, no authority mutation"), Selection.TargetInstance == 0 && State == Before);
    TestTrue(TEXT("Preview click carries correct instance and action serial"), Selection.BoardClick(View, {4,4}).GetValue().CardInstanceId == 0);
    Selection.Clear();
    TestTrue(TEXT("Cancellation spends no action"), State == Before && Selection.TargetInstance == INDEX_NONE);
    TestTrue(TEXT("Same type second card has distinct identity"), !Selection.HandClick(View, 1).IsSet() && Selection.TargetInstance == 1);
    Selection.HandClick(View, 1); TestEqual(TEXT("Reclick cancels"), Selection.TargetInstance, INDEX_NONE);
    TestTrue(TEXT("Below cap produces Draw intent"), !View.IsReplace() && Selection.Draw(View).GetValue().Type == ESingleplayerActionType::Draw);
    State = Fixture({0,1,2,3,6}, {4,5,7});
    const auto FullBefore = State;
    const auto FullView = MakeSingleplayerView(State, 7);
    TestTrue(TEXT("At cap enters local Replace mode"), FullView.IsReplace() && !Selection.Draw(FullView).IsSet() && Selection.bReplacing && State == FullBefore);
    TestFalse(TEXT("Board suppressed while choosing replacement"), Selection.BoardClick(FullView, {4,4}).IsSet());
    TestTrue(TEXT("One hand entry becomes replacement request"), Selection.HandClick(FullView, 1).GetValue().Type == ESingleplayerActionType::Replace);
    Selection.Clear(); TestTrue(TEXT("Replace cancellation is zero mutation"), State == FullBefore && !Selection.bReplacing);
    TestTrue(TEXT("Escape is Exit, not cancellation"), IsSingleplayerExitKey(EKeys::Escape) && !IsSingleplayerExitKey(EKeys::RightMouseButton));
    int32 CatalogCount = 0;
    for (int32 Id=0; Id<=int32(ECardId::Joker); ++Id) { CatalogCount += SingleplayerCardPresentation(static_cast<ECardId>(Id)).bEnabled ? 1 : 0; }
    TestEqual(TEXT("Only four obtainable/displayed catalog cards"), CatalogCount, 4);
    TestTrue(TEXT("Restock SP text"), SingleplayerCardPresentation(ECardId::Restock).Description == TEXT("抽取至多 3 张牌，手牌最多 5 张。"));
    State.Match.Players[1].Hand.Add(ECardId::Ghost);
    TestTrue(TEXT("Presentation hand comes only from SP deck, ignores legacy opponent hand"), MakeSingleplayerView(State,7).Hand == FullView.Hand);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPRestart, "Gomokards.Singleplayer.RestartAndRuntimeLifecycle", SPFlags)
bool FSPRestart::RunTest(const FString&)
{
    auto Dirty = Fixture({0,1}, {2}, {3,4}, {5,6,7});
    Dirty.Match.Board.At({4,4}) = {EStone::Black,true};
    Dirty.Match.Board.Barriers.Add({3,3});
    Dirty.Match.Result = {EMatchStatus::Won,EStone::Black};
    Dirty.Match.CompletedActions = 73;
    Dirty.Reset(17);
    TestTrue(TEXT("Value reset clears result, board history, piles and counters"), Dirty == FSingleplayerBattleState(17));
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    auto* Mode = World->SpawnActor<ASingleplayerBattleGameMode>();
    if (!TestNotNull(TEXT("Explicit singleplayer owner"), Mode)) { World->DestroyWorld(false); return false; }
    FString Error;
    Mode->InitGame(TEXT("LocalMatch"), TEXT("?Seed=17"), Error);
    TestTrue(TEXT("Seed launch uses exact seed; restart reconstructs complete battle"), Error.IsEmpty() && Mode->GetBattle() == FSingleplayerBattleState(17));
    TestTrue(TEXT("Startup uses local controller"), Mode->PlayerControllerClass == ASingleplayerBattlePlayerController::StaticClass());
    auto* Controller = World->SpawnActor<ASingleplayerBattlePlayerController>();
    if (!TestNotNull(TEXT("Local controller class constructible"), Controller)) { World->DestroyWorld(false); return false; }
    Controller->BindBattle(Mode);
    int32 TargetSlot = Controller->GetPresentation().Hand.IndexOfByPredicate([](const FSingleplayerCard& Entry){ return BoardEffectTarget(Entry.Id) != ECardTarget::None; });
    Controller->ClickHand(TargetSlot);
    TestTrue(TEXT("Controller targeting stays local"), Controller->GetSelection().TargetInstance != INDEX_NONE && Mode->GetBattle() == FSingleplayerBattleState(17));
    Mode->RestartBattle();
    TestEqual(TEXT("Restart notification clears actual controller targeting"), Controller->GetSelection().TargetInstance, INDEX_NONE);
    int32 Notifications = 0;
    Mode->OnBattleChanged.AddLambda([&]{ ++Notifications; });
    auto View = Mode->GetPresentation();
    TestTrue(TEXT("Runtime accepts player opening"), Mode->SubmitPlayer({ESingleplayerActionType::PlaceStone, View.Actions, INDEX_NONE, FIntPoint(9,9)}, View.Generation).IsAccepted());
    const auto PendingView = Mode->GetPresentation();
    TestTrue(TEXT("AI pending leaves exactly one completed player action"), PendingView.Actions == 1 && PendingView.CurrentSide == 1);
    TestTrue(TEXT("Deterministic callback seam invokes real policy and resolver"), Mode->ExecutePendingAI(PendingView.Generation, PendingView.Actions));
    const auto After = Mode->GetBattle();
    TestTrue(TEXT("Exactly one AI placement and two notifications"), After.Match.CompletedActions == 2 && Notifications == 2);
    TestFalse(TEXT("Callback cannot execute twice"), Mode->ExecutePendingAI(PendingView.Generation, PendingView.Actions));
    TestTrue(TEXT("Duplicate changes nothing"), Mode->GetBattle() == After);
    View = Mode->GetPresentation();
    Mode->SubmitPlayer({ESingleplayerActionType::Draw, View.Actions}, View.Generation);
    const auto OldPending = Mode->GetPresentation();
    Mode->RestartBattle();
    TestTrue(TEXT("Restart clears all board, piles, counters and terminal state"), Mode->GetBattle() == FSingleplayerBattleState(17));
    View = Mode->GetPresentation();
    TestFalse(TEXT("Old player command rejected even if action serial repeats"), Mode->SubmitPlayer({ESingleplayerActionType::Draw,0},OldPending.Generation).IsAccepted());
    Mode->SubmitPlayer({ESingleplayerActionType::Draw,View.Actions},View.Generation);
    const auto NewPending = Mode->GetPresentation();
    const auto BeforeStale = Mode->GetBattle();
    TestFalse(TEXT("Old AI generation cannot consume new pending callback"), Mode->ExecutePendingAI(OldPending.Generation,NewPending.Actions));
    TestTrue(TEXT("Stale callback atomic"), Mode->GetBattle() == BeforeStale);
    TestTrue(TEXT("New pending AI still executes"), Mode->ExecutePendingAI(NewPending.Generation,NewPending.Actions));
    Controller->ClickDraw();
    auto FullPending = Mode->GetPresentation();
    Mode->ExecutePendingAI(FullPending.Generation, FullPending.Actions);
    const auto FullBefore = Mode->GetBattle();
    Controller->ClickDraw();
    TestTrue(TEXT("Actual controller enters Replace without changing battle"), Controller->GetSelection().bReplacing && Mode->GetBattle() == FullBefore);
    Controller->CancelSelection();
    TestTrue(TEXT("Actual controller cancellation atomic"), !Controller->GetSelection().bReplacing && Mode->GetBattle() == FullBefore);
    Controller->ClickDraw();
    Mode->RestartBattle();
    TestTrue(TEXT("Restart clears actual controller Replace and refreshes hand"), !Controller->GetSelection().bReplacing && Controller->GetPresentation().Hand.Num() == 3);
    View = Mode->GetPresentation();
    Mode->SubmitPlayer({ESingleplayerActionType::Draw,0},View.Generation);
    const auto Ending = Mode->GetPresentation();
    const auto BeforeEnd = Mode->GetBattle();
    Mode->EndPlay(EEndPlayReason::RemovedFromWorld);
    TestFalse(TEXT("EndPlay invalidates pending AI"), Mode->ExecutePendingAI(Ending.Generation,Ending.Actions));
    TestTrue(TEXT("No post-EndPlay mutation"), Mode->GetBattle() == BeforeEnd);
    Controller->EndPlay(EEndPlayReason::RemovedFromWorld);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPManualRecipe, "Gomokards.Singleplayer.ManualSeedRecipe", SPFlags)
bool FSPManualRecipe::RunTest(const FString&)
{
    // Find a reproducible starting hand with Restock and two distinct targeted cards.
    int32 ChosenSeed = INDEX_NONE;
    for (int32 Seed=0; Seed<100; ++Seed)
    {
        const FSingleplayerBattleState Candidate(Seed);
        TSet<ECardId> Types;
        for (const auto& Entry : Candidate.PlayerDeck.Hand) { Types.Add(Entry.Id); }
        if (Types.Num() == 3 && Types.Contains(ECardId::Restock)) { ChosenSeed = Seed; break; }
    }
    if (!TestTrue(TEXT("Useful deterministic seed exists"), ChosenSeed != INDEX_NONE)) { return false; }
    FSingleplayerBattleState State(ChosenSeed);
    TArray<FString> Names;
    for (const auto& Entry : State.PlayerDeck.Hand) { Names.Add(SingleplayerCardPresentation(Entry.Id).Name); }
    TestTrue(TEXT("Manual recipe starts 3/5/0/0"), State.PlayerDeck.Hand.Num()==3 && State.PlayerDeck.DrawPile.Num()==5
        && State.PlayerDeck.DiscardPile.IsEmpty() && State.PlayerDeck.ExhaustPile.IsEmpty());
    ResolvePlayerAction(State, Request(State, ESingleplayerActionType::PlaceStone, INDEX_NONE, FIntPoint(9,9)));
    const auto Move = ChooseSingleplayerMove(State.Match.Board);
    TestTrue(TEXT("Opening AI has legal deterministic response"), Move.IsSet());
    AddInfo(FString::Printf(TEXT("Manual recipe: ?Seed=%d; hand left-to-right=%s; after Black (10,10), AI (%d,%d), 1-based."),
        ChosenSeed,*FString::Join(Names,TEXT(" / ")),Move.GetValue().X+1,Move.GetValue().Y+1));
    return true;
}
#endif
