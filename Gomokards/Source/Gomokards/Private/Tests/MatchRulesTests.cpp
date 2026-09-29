#include "Core/MatchRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Gomokards
{
namespace
{
constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
FPlayerId Current(const FMatchState& State) { return State.Players[State.CurrentPlayerIndex].Id; }
FActionResult Place(FMatchState& State, FIntPoint P) { return ResolveAction(State, FActionRequest::Place(Current(State), P)); }
FActionResult Play(FMatchState& State, ECardId Id, TOptional<FIntPoint> P = {})
{ return ResolveAction(State, FActionRequest::Play(Current(State), Id, P)); }
void Put(FMatchState& State, FIntPoint P, EStone Stone) { State.Board.At(P).Stone = Stone; }
void RejectUnchanged(FAutomationTestBase& Test, FMatchState& State, const FActionRequest& Request, EActionError Error)
{
    const FMatchState Before = State;
    const auto Result = ResolveAction(State, Request);
    Test.TestTrue(TEXT("Expected rejection reason"), Result.Error == Error);
    Test.TestFalse(TEXT("Rejected request gives no reward"), Result.bBlockingReward);
    Test.TestTrue(TEXT("Every authoritative field and RNG preserved"), State == Before);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchBasics, "Gomokards.Phase1.MatchAndReset", Flags)
bool FMatchBasics::RunTest(const FString& Parameters)
{
    FMatchState State(73);
    TestEqual(TEXT("Exactly two players"), State.Players.Num(), 2);
    TestTrue(TEXT("Black first"), State.Players[State.CurrentPlayerIndex].AssignedStone == EStone::Black);
    TestTrue(TEXT("Empty hands"), State.Players[0].Hand.IsEmpty() && State.Players[1].Hand.IsEmpty());
    TestTrue(TEXT("No terminal result"), State.Result.Status == EMatchStatus::InProgress);
    for (int32 I = 0; I < FBoard::Size * FBoard::Size; ++I)
    {
        TestTrue(TEXT("Empty clean cell"), State.Board.Cells[I] == FCell{});
        TestEqual(TEXT("Index round trip"), FBoard::ToIndex(FBoard::ToCoordinate(I)), I);
    }
    TestTrue(TEXT("Placement accepted"), Place(State, {4, 4}).IsAccepted());
    TestTrue(TEXT("Black stored"), State.Board.At({4, 4}).Stone == EStone::Black);
    TestEqual(TEXT("Exactly one completion"), State.CompletedActions, uint64(1));
    TestEqual(TEXT("Other player"), State.CurrentPlayerIndex, 1);
    TestTrue(TEXT("No ordinary draw"), State.Players[0].Hand.IsEmpty());
    State.Players[1].Hand = {ECardId::TacticalNuke};
    Play(State, ECardId::TacticalNuke, FIntPoint(4, 4));
    State.Random.RandRange(0, 50);
    State.Result = {EMatchStatus::Won, EStone::White};
    State.Reset(73);
    TestTrue(TEXT("Reset reconstructs entire initial state and RNG"), State == FMatchState(73));
    State.Reset();
    TestTrue(TEXT("Default reset is deterministic"), State == FMatchState());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInvalidActions, "Gomokards.Phase1.InvalidActionsAtomic", Flags)
bool FInvalidActions::RunTest(const FString& Parameters)
{
    FMatchState State(11);
    State.Players[0].Hand = {ECardId::TacticalNuke, ECardId::Restock, ECardId::Ghost};
    Put(State, {2, 2}, EStone::White);
    State.Board.At({3, 3}).bForbidden = true;
    RejectUnchanged(*this, State, FActionRequest::Place(1, {0, 0}), EActionError::WrongPlayer);
    RejectUnchanged(*this, State, FActionRequest::Place(99, {0, 0}), EActionError::WrongPlayer);
    for (FIntPoint P : {FIntPoint(-1, 0), FIntPoint(19, 0), FIntPoint(0, -1), FIntPoint(0, 19)})
    { RejectUnchanged(*this, State, FActionRequest::Place(0, P), EActionError::InvalidCoordinate); }
    RejectUnchanged(*this, State, FActionRequest::Place(0, {2, 2}), EActionError::Occupied);
    RejectUnchanged(*this, State, FActionRequest::Place(0, {3, 3}), EActionError::Forbidden);
    RejectUnchanged(*this, State, FActionRequest::Play(0, ECardId::Steal), EActionError::CardNotOwned);
    for (ECardId Id : {ECardId::Invalid, ECardId::Ghost, ECardId::Tetris, ECardId::FastDuel, ECardId::Undo, ECardId::Joker, static_cast<ECardId>(255)})
    { RejectUnchanged(*this, State, FActionRequest::Play(0, Id), EActionError::UnsupportedCard); }
    RejectUnchanged(*this, State, FActionRequest::Play(0, ECardId::TacticalNuke), EActionError::InvalidTarget);
    RejectUnchanged(*this, State, FActionRequest::Play(0, ECardId::TacticalNuke, FIntPoint(19, 4)), EActionError::InvalidTarget);
    RejectUnchanged(*this, State, FActionRequest::Play(0, ECardId::Restock, FIntPoint(0, 0)), EActionError::InvalidTarget);
    auto Request = FActionRequest::Place(0, {0, 0});
    Request.Type = static_cast<EActionType>(255);
    RejectUnchanged(*this, State, Request, EActionError::UnsupportedAction);
    // Target selection can validate a request then discard it, without a cancel command or match mutation.
    const FMatchState Before = State;
    const auto Uncommitted = FActionRequest::Play(0, ECardId::TacticalNuke, FIntPoint(2, 2));
    TestTrue(TEXT("Preview validation succeeds"), ValidateAction(State, Uncommitted) == EActionError::None);
    TestTrue(TEXT("Uncommitted/cancelled selection preserves all state"), State == Before);
    State.Players.Add({2, EStone::Black, {}});
    RejectUnchanged(*this, State, FActionRequest::Place(0, {0, 0}), EActionError::InvalidState);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlockingMatrix, "Gomokards.Phase1.BlockingMatrix", Flags)
bool FBlockingMatrix::RunTest(const FString& Parameters)
{
    const FIntPoint Rays[] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,-1},{1,-1},{-1,1}};
    const FIntPoint Origin(9, 9);
    for (EStone Stone : {EStone::Black, EStone::White})
    {
        for (FIntPoint Ray : Rays)
        {
            for (int32 RunLength : {1, 3})
            {
                FMatchState State(21);
                State.CurrentPlayerIndex = Stone == EStone::Black ? 0 : 1;
                const int32 Actor = State.CurrentPlayerIndex;
                for (int32 I = 1; I <= RunLength; ++I) { Put(State, Origin + Ray * I, OppositeStone(Stone)); }
                Put(State, Origin + Ray * (RunLength + 1), Stone);
                TestTrue(TEXT("Every ray/color/run rewards"), Place(State, Origin).bBlockingReward);
                TestEqual(TEXT("Exactly one card"), State.Players[Actor].Hand.Num(), 1);
                TestEqual(TEXT("Complete once"), State.CompletedActions, uint64(1));
            }
            FMatchState Open;
            Open.CurrentPlayerIndex = Stone == EStone::Black ? 0 : 1;
            Put(Open, Origin + Ray, OppositeStone(Stone));
            Open.Board.At(Origin + Ray * 2).bForbidden = true;
            const int32 Seed = Open.Random.GetCurrentSeed();
            TestFalse(TEXT("Empty forbidden far end remains open"), Place(Open, Origin).bBlockingReward);
            TestEqual(TEXT("No reward uses no RNG"), Open.Random.GetCurrentSeed(), Seed);
            TestTrue(TEXT("No reward means no card"), Open.Players[0].Hand.IsEmpty() && Open.Players[1].Hand.IsEmpty());
            FMatchState Gap;
            Put(Gap, Origin + Ray * 2, OppositeStone(Stone));
            Put(Gap, Origin + Ray * 3, Stone);
            TestFalse(TEXT("Adjacent gap fails"), HasSuccessfulBlock(Gap.Board, Origin, Stone));
            FMatchState Edge;
            Edge.CurrentPlayerIndex = Stone == EStone::Black ? 0 : 1;
            const FIntPoint Start(Ray.X > 0 ? 17 : Ray.X < 0 ? 1 : 9, Ray.Y > 0 ? 17 : Ray.Y < 0 ? 1 : 9);
            Put(Edge, Start + Ray, OppositeStone(Stone));
            TestTrue(TEXT("Board edge closes far end"), Place(Edge, Start).bBlockingReward);
        }
    }
    FMatchState Multi;
    Put(Multi, {10,9}, EStone::White); Put(Multi, {11,9}, EStone::Black);
    Put(Multi, {9,8}, EStone::White); Put(Multi, {9,7}, EStone::Black);
    TestTrue(TEXT("Several rays reward"), Place(Multi, Origin).bBlockingReward);
    TestEqual(TEXT("Several rays only one card"), Multi.Players[0].Hand.Num(), 1);
    TestFalse(TEXT("Predicate out of bounds"), HasSuccessfulBlock(Multi.Board, {-1,0}, EStone::Black));
    // The predicate deliberately does not demand a stone at its origin.
    Multi.Board.At(Origin).Stone = EStone::Empty;
    TestTrue(TEXT("Legality remains caller responsibility"), HasSuccessfulBlock(Multi.Board, Origin, EStone::Black));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRepeatedBlocking, "Gomokards.Phase1.RepeatedBlocking", Flags)
bool FRepeatedBlocking::RunTest(const FString& Parameters)
{
    FMatchState State(12);
    // Two separate placements, separated by an opponent action; no match-wide reward deduplication.
    Put(State, {6,5}, EStone::White); Put(State, {7,5}, EStone::Black);
    Put(State, {6,9}, EStone::White); Put(State, {7,9}, EStone::Black);
    TestTrue(TEXT("First block"), Place(State, {5,5}).bBlockingReward);
    TestTrue(TEXT("Opponent ordinary action"), Place(State, {0,0}).IsAccepted());
    TestTrue(TEXT("Later qualifying action"), Place(State, {5,9}).bBlockingReward);
    TestEqual(TEXT("Both rewards retained"), State.Players[0].Hand.Num(), 2);
    TestEqual(TEXT("Three completed actions"), State.CompletedActions, uint64(3));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWinMatrix, "Gomokards.Phase1.WinMatrixAndTerminal", Flags)
bool FWinMatrix::RunTest(const FString& Parameters)
{
    for (FIntPoint Direction : {FIntPoint(1,0), FIntPoint(0,1), FIntPoint(1,1), FIntPoint(1,-1)})
    {
        for (EStone Stone : {EStone::Black, EStone::White})
        {
            for (int32 Length : {5, 6})
            {
                FMatchState State;
                // Non-default player IDs and reversed stone assignments prove identity separation.
                State.Players[0].Id = 42; State.Players[1].Id = 77;
                State.Players[0].AssignedStone = Stone; State.Players[1].AssignedStone = OppositeStone(Stone);
                const FIntPoint Start(5,9);
                for (int32 I = 0; I < Length; ++I)
                { if (I != 2) { Put(State, Start + Direction * I, Stone); } }
                TestTrue(TEXT("Winning placement accepted"), Place(State, Start + Direction * 2).IsAccepted());
                TestTrue(TEXT("Winner is actual stone identity"), State.Result.WinningStone == Stone);
                TestTrue(TEXT("Terminal won"), State.Result.Status == EMatchStatus::Won);
                TestEqual(TEXT("No transfer after win"), State.CurrentPlayerIndex, 0);
                TestEqual(TEXT("Winning action completed once"), State.CompletedActions, uint64(1));
                RejectUnchanged(*this, State, FActionRequest::Place(42, {0,0}), EActionError::MatchStopped);
                State.Players[0].Hand.Add(ECardId::Restock);
                RejectUnchanged(*this, State, FActionRequest::Play(42, ECardId::Restock), EActionError::MatchStopped);
            }
        }
        FBoard Broken;
        for (int32 I : {0,1,3,4,5}) { Broken.At(FIntPoint(5,9) + Direction * I).Stone = EStone::Black; }
        TestFalse(TEXT("Broken sequence is not five"), HasWinningLine(Broken, {5,9}, EStone::Black));
    }
    FMatchState Both;
    for (int32 X = 5; X < 9; ++X) { Put(Both, {X,9}, EStone::Black); }
    Put(Both, {9,10}, EStone::White); Put(Both, {9,11}, EStone::Black);
    TestTrue(TEXT("Winning placement also rewards"), Place(Both, {9,9}).bBlockingReward);
    TestTrue(TEXT("Black wins"), Both.Result.WinningStone == EStone::Black);
    TestEqual(TEXT("One reward after win"), Both.Players[0].Hand.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardTransactions, "Gomokards.Phase1.CardTransactions", Flags)
bool FCardTransactions::RunTest(const FString& Parameters)
{
    FMatchState Restock(123);
    Restock.Players[0].Hand = {ECardId::Steal, ECardId::Restock};
    FRandomStream Expected = Restock.Random;
    const auto Pool = GetPlayableCards();
    const ECardId First = Pool[Expected.RandRange(0, Pool.Num()-1)].Id;
    const ECardId Second = Pool[Expected.RandRange(0, Pool.Num()-1)].Id;
    TestFalse(TEXT("Restock is not blocking reward"), Play(Restock, ECardId::Restock).bBlockingReward);
    TestTrue(TEXT("Consume then append exactly two draws"), Restock.Players[0].Hand == TArray<ECardId>{ECardId::Steal, First, Second});
    TestEqual(TEXT("Exactly two RNG draws"), Restock.Random.GetCurrentSeed(), Expected.GetCurrentSeed());
    TestEqual(TEXT("Restock completes once"), Restock.CompletedActions, uint64(1));
    TestEqual(TEXT("Restock transfers once"), Restock.CurrentPlayerIndex, 1);

    FMatchState SwapState(45);
    SwapState.Players[0].Hand = {ECardId::SwapHands, ECardId::Restock};
    SwapState.Players[1].Hand = {ECardId::Steal, ECardId::TacticalNuke};
    TestTrue(TEXT("Swap accepted"), Play(SwapState, ECardId::SwapHands).IsAccepted());
    TestTrue(TEXT("Actor gets other hand"), SwapState.Players[0].Hand == TArray<ECardId>{ECardId::Steal, ECardId::TacticalNuke});
    TestTrue(TEXT("Consumed swap never transferred"), SwapState.Players[1].Hand == TArray<ECardId>{ECardId::Restock});
    TestEqual(TEXT("Swap completes once"), SwapState.CompletedActions, uint64(1));
    TestEqual(TEXT("Swap no RNG"), SwapState.Random.GetCurrentSeed(), 45);

    FMatchState Steal(678);
    Steal.Players[0].Hand = {ECardId::Steal};
    Steal.Players[1].Hand = {ECardId::Restock, ECardId::TacticalNuke, ECardId::Restock};
    Expected = Steal.Random;
    TArray<ECardId> Remaining = Steal.Players[1].Hand;
    const ECardId Chosen = Remaining[Expected.RandRange(0, Remaining.Num()-1)];
    Remaining.RemoveAt(Remaining.Find(Chosen));
    TestTrue(TEXT("Steal accepted"), Play(Steal, ECardId::Steal).IsAccepted());
    TestTrue(TEXT("Deterministic transfer"), Steal.Players[0].Hand == TArray<ECardId>{Chosen});
    TestTrue(TEXT("One matching entry removed, order preserved"), Steal.Players[1].Hand == Remaining);
    TestEqual(TEXT("One random choice"), Steal.Random.GetCurrentSeed(), Expected.GetCurrentSeed());
    TestEqual(TEXT("Steal completes once"), Steal.CompletedActions, uint64(1));
    FMatchState Empty(89);
    Empty.Players[0].Hand = {ECardId::Steal};
    TestTrue(TEXT("Empty opponent is valid"), Play(Empty, ECardId::Steal).IsAccepted());
    TestTrue(TEXT("Steal consumed"), Empty.Players[0].Hand.IsEmpty());
    TestEqual(TEXT("Empty steal still advances"), Empty.CurrentPlayerIndex, 1);
    TestEqual(TEXT("Empty steal no random choice"), Empty.Random.GetCurrentSeed(), 89);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNukeTransactions, "Gomokards.Phase1.NukeTransactions", Flags)
bool FNukeTransactions::RunTest(const FString& Parameters)
{
    for (EStone Existing : {EStone::Empty, EStone::Black, EStone::White})
    {
        for (bool Forbidden : {false, true})
        {
            FMatchState State(4);
            State.Players[0].Hand = {ECardId::TacticalNuke};
            State.Board.At({5,5}) = {Existing, Forbidden};
            Put(State, {6,5}, EStone::White); Put(State, {7,5}, EStone::Black);
            const auto Result = Play(State, ECardId::TacticalNuke, FIntPoint(5,5));
            TestTrue(TEXT("Occupied/empty/already forbidden target valid"), Result.IsAccepted());
            TestFalse(TEXT("Nuke never rewards even beside enclosed run"), Result.bBlockingReward);
            TestTrue(TEXT("Cleared and forbidden"), State.Board.At({5,5}) == FCell{EStone::Empty, true});
            TestTrue(TEXT("Only played card removed"), State.Players[0].Hand.IsEmpty());
            TestEqual(TEXT("Nuke completes once"), State.CompletedActions, uint64(1));
            TestEqual(TEXT("Nuke no RNG"), State.Random.GetCurrentSeed(), 4);
            RejectUnchanged(*this, State, FActionRequest::Place(Current(State), {5,5}), EActionError::Forbidden);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeterminism, "Gomokards.Phase1.DeterminismAndPool", Flags)
bool FDeterminism::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Exactly eight playable definitions (Phase 3A pool)"), GetPlayableCards().Num(), 8);
    for (int32 Seed = 0; Seed < 32; ++Seed)
    {
        FMatchState A(Seed), B(Seed);
        A.Players[0].Hand = B.Players[0].Hand = {ECardId::Restock};
        Play(A, ECardId::Restock); Play(B, ECardId::Restock);
        Place(A, {0,0}); Place(B, {0,0});
        TestTrue(TEXT("Fixed seed/actions reproduce full state"), A == B);
        for (const auto& Player : A.Players)
        { for (ECardId Card : Player.Hand) { TestNotNull(TEXT("No unresolved card generated"), FindCardDefinition(Card)); } }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoLegalAction, "Gomokards.Phase1.UnresolvedNoLegalAction", Flags)
bool FNoLegalAction::RunTest(const FString& Parameters)
{
    FMatchState State;
    // Four-period pattern has no five-line in any direction.
    for (int32 Y=0; Y<FBoard::Size; ++Y)
    { for (int32 X=0; X<FBoard::Size; ++X) { Put(State, {X,Y}, (X+2*Y)%4 < 2 ? EStone::Black : EStone::White); } }
    State.Players[0].Hand = {ECardId::Steal};
    TestTrue(TEXT("Last available action accepted"), Play(State, ECardId::Steal).IsAccepted());
    TestTrue(TEXT("Explicit unresolved result, no invented draw"), State.Result.Status == EMatchStatus::AwaitingRuleDecision);
    TestTrue(TEXT("Reason retained"), State.Result.Decision == EDecisionReason::NoLegalAction);
    TestTrue(TEXT("No invented winner"), State.Result.WinningStone == EStone::Empty);
    RejectUnchanged(*this, State, FActionRequest::Place(Current(State), {0,0}), EActionError::MatchStopped);
    State.Reset();
    TestTrue(TEXT("Decision state resets"), State == FMatchState());
    return true;
}
}
#endif
