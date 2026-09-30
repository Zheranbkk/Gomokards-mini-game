#include "Core/MatchRules.h"
#include "Presentation/MatchPresentation.h"
#include "Runtime/LocalMatchGameMode.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace Gomokards;
namespace
{
constexpr EAutomationTestFlags GhostFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
FPlayerId GhostActor(const FMatchState& S) { return S.Players[S.CurrentPlayerIndex].Id; }
FActionResult GhostPlace(FMatchState& S,FIntPoint P) { return ResolveAction(S,FActionRequest::Place(GhostActor(S),P)); }
void StartHidden(FMatchState& S)
{
    S.Players[S.CurrentPlayerIndex].Hand.Add(ECardId::Ghost);
    ResolveAction(S,FActionRequest::Play(GhostActor(S),ECardId::Ghost));
    BeginGhostHidden(S);
}
void GhostReject(FAutomationTestBase& Test,FMatchState& S,const FActionRequest& R,EActionError Error)
{
    const FMatchState Before=S;
    const auto Result=ResolveAction(S,R);
    Test.TestTrue(TEXT("Specific rejection without reward"),Result.Error==Error && !Result.bBlockingReward);
    Test.TestTrue(TEXT("Rejection preserves ALL state, phase, counts, turns and RNG"),S==Before);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostActivation,"Gomokards.Phase3B.ActivationAndPreparation",GhostFlags)
bool FGhostActivation::RunTest(const FString& Parameters)
{
    FMatchState S(22);
    S.Players[0].Hand={ECardId::Ghost,ECardId::Restock}; S.Players[1].Hand={ECardId::TacticalNuke};
    S.ConfusionActionsRemaining=2;
    S.Board.At({2,2}).Stone=EStone::Black; S.Board.At({3,2}).Stone=EStone::White;
    S.Board.At({4,4}).bForbidden=true; S.Board.Barriers.Add({2,2});
    const FBoard Board=S.Board;
    const auto Cast=ResolveAction(S,FActionRequest::Play(0,ECardId::Ghost));
    TestTrue(TEXT("Ghost consumes exactly once, never rewards"),Cast.IsAccepted() && !Cast.bBlockingReward && S.Players[0].Hand==TArray<ECardId>{ECardId::Restock});
    TestTrue(TEXT("Cast completes and transfers once then prepares"),S.CompletedActions==1 && S.CurrentPlayerIndex==1 && S.GhostPhase==EGhostPhase::Preparation && S.GhostPlacementsCompleted==0);
    TestTrue(TEXT("Cast uses normal old Confusion lifetime and leaves board true"),S.ConfusionActionsRemaining==1 && S.Board==Board && S.Random.GetCurrentSeed()==22);
    TestFalse(TEXT("Preparation has no card access"),CanPlayCards(S));
    for (FIntPoint P : {FIntPoint(0,0),FIntPoint(2,2),FIntPoint(4,4),FIntPoint(-1,0)})
    { GhostReject(*this,S,FActionRequest::Place(1,P),EActionError::GhostPreparation); }
    GhostReject(*this,S,FActionRequest::Play(1,ECardId::TacticalNuke,FIntPoint(0,0)),EActionError::GhostPreparation);
    FTargetSelection Target;
    TestFalse(TEXT("No targeting in Preparation"),Target.Toggle(S,ECardId::TacticalNuke));
    const FMatchState Before=S;
    TestTrue(TEXT("Deterministic transition without sleeping"),BeginGhostHidden(S));
    FMatchState Expected=Before; Expected.GhostPhase=EGhostPhase::Hidden;
    TestTrue(TEXT("Transition changes only Ghost phase/count initialization"),S==Expected);
    TestFalse(TEXT("Transition is one-shot"),BeginGhostHidden(S));
    TestTrue(TEXT("Duplicate transition atomic"),S==Expected);
    S.Reset(22); TestTrue(TEXT("Reset clears Ghost fully"),S==FMatchState(22));
    TestFalse(TEXT("Stale logical transition cannot change new normal match"),BeginGhostHidden(S));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostCounting,"Gomokards.Phase3B.HiddenCountingAndRestrictions",GhostFlags)
bool FGhostCounting::RunTest(const FString& Parameters)
{
    FMatchState S(14); S.Board.At({18,18}).bForbidden=true;
    StartHidden(S);
    S.Players[1].Hand={ECardId::TacticalNuke};
    GhostReject(*this,S,FActionRequest::Place(1,{-1,0}),EActionError::InvalidCoordinate);
    GhostReject(*this,S,FActionRequest::Place(1,{18,18}),EActionError::Forbidden);
    GhostReject(*this,S,FActionRequest::Play(1,ECardId::TacticalNuke,FIntPoint(0,0)),EActionError::GhostCardsRestricted);
    FTargetSelection Selection; TestFalse(TEXT("No hidden card targeting"),Selection.Toggle(S,ECardId::TacticalNuke));
    for (int32 I=0; I<6; ++I)
    {
        const auto R=GhostPlace(S,{I*2,5});
        TestTrue(TEXT("Legal hidden non-blocking placement succeeds without draw"),R.IsAccepted() && !R.bBlockingReward);
        TestEqual(TEXT("Exactly one completed action per placement"),S.CompletedActions,uint64(I+2));
        TestEqual(TEXT("One normal transfer per continuing placement"),S.CurrentPlayerIndex,(I%2));
        if (I<5)
        {
            TestTrue(TEXT("Hidden persists through five"),S.GhostPhase==EGhostPhase::Hidden && S.GhostPlacementsCompleted==I+1);
            GhostReject(*this,S,FActionRequest::Place(GhostActor(S),{I*2,5}),EActionError::Occupied);
        }
    }
    TestTrue(TEXT("Six reveals and clears local mode counter"),S.GhostPhase==EGhostPhase::None && S.GhostPlacementsCompleted==0 && S.Result.Status==EMatchStatus::InProgress);
    TestTrue(TEXT("Cards return after nonterminal reveal"),CanPlayCards(S));
    TestTrue(TEXT("Retained Nuke can now be played"),ResolveAction(S,FActionRequest::Play(1,ECardId::TacticalNuke,FIntPoint(0,0))).IsAccepted());
    TestEqual(TEXT("No unexpected RNG draw"),S.Random.GetCurrentSeed(),14);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostRewards,"Gomokards.Phase3B.TrueColorRewardsAndConfusion",GhostFlags)
bool FGhostRewards::RunTest(const FString& Parameters)
{
    for (bool Confused : {false,true})
    for (bool MultiRay : {false,true})
    {
        FMatchState S(123);
        S.ConfusionActionsRemaining=Confused ? 2 : 0;
        StartHidden(S); // White to act; cast consumed one old Confusion action if present.
        const EStone Actual=Confused ? EStone::Black : EStone::White;
        S.Board.At({6,5}).Stone=OppositeStone(Actual); S.Board.At({7,5}).Stone=Actual;
        if (MultiRay) { S.Board.At({5,6}).Stone=OppositeStone(Actual); S.Board.At({5,7}).Stone=Actual; }
        S.Board.Barriers.Add({5,5}); // Blocking predicate still ignores Barrier metadata.
        GhostReject(*this,S,FActionRequest::Place(1,{6,5}),EActionError::Occupied);
        const auto Result=GhostPlace(S,{5,5});
        TestTrue(TEXT("True-color single/multiple rays reward exactly one card to actor"),Result.bBlockingReward && S.Players[1].Hand.Num()==1 && S.Players[0].Hand.IsEmpty());
        TestTrue(TEXT("Stores true effective color, not gray"),S.Board.At({5,5}).Stone==Actual && S.Players[1].AssignedStone==EStone::White);
        TestEqual(TEXT("Confusion cast/first hidden placement exhaust exactly two"),S.ConfusionActionsRemaining,0);
        GhostPlace(S,{10,10}); // Return to card owner.
        GhostReject(*this,S,FActionRequest::Play(1,S.Players[1].Hand[0]),EActionError::GhostCardsRestricted);
        TestTrue(TEXT("Black and White have identical gray rendering"),StoneDisplayColor(S,EStone::Black)==StoneDisplayColor(S,EStone::White));
    }
    FMatchState S;
    StartHidden(S);
    S.ConfusionActionsRemaining=1;
    const FString Label=EffectLabel(S);
    TestTrue(TEXT("Confusion remains indicated without actual color leak"),Label.Contains(TEXT("1 successful")) && Label.Contains(TEXT("color hidden")) && !Label.Contains(TEXT("White")) && !Label.Contains(TEXT("Black")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostReveal,"Gomokards.Phase3B.WinSuppressionAndReveal",GhostFlags)
bool FGhostReveal::RunTest(const FString& Parameters)
{
    for (int32 Outcome=0; Outcome<4; ++Outcome)
    {
        FMatchState S;
        for (int32 X=0; X<4; ++X)
        {
            if (Outcome&1) { S.Board.At({X,2}).Stone=EStone::Black; }
            if (Outcome&2) { S.Board.At({X,5}).Stone=EStone::White; }
        }
        StartHidden(S);
        const FIntPoint Moves[]={{4,5},{4,2},{10,10},{12,10},{14,10},{16,10}};
        for (int32 I=0; I<6; ++I)
        {
            TestTrue(TEXT("Every hidden placement accepted even with existing winning lines"),GhostPlace(S,Moves[I]).IsAccepted());
            if (I<5) { TestTrue(TEXT("First five never terminate on lines"),S.Result.Status==EMatchStatus::InProgress && S.GhostPhase==EGhostPhase::Hidden); }
        }
        const EMatchStatus Expected=Outcome==0 ? EMatchStatus::InProgress : Outcome==3 ? EMatchStatus::Draw : EMatchStatus::Won;
        TestTrue(TEXT("Reveal handles neither, black, white and both"),S.Result.Status==Expected);
        TestTrue(TEXT("Winning identity/no-winner correct"),S.Result.WinningStone==(Outcome==1 ? EStone::Black : Outcome==2 ? EStone::White : EStone::Empty));
        TestTrue(TEXT("Exactly seven actions, no extra reveal transfer"),S.CompletedActions==7 && S.CurrentPlayerIndex==(Outcome==0 ? 1 : 0));
        TestTrue(TEXT("Reveal restores colors, no snapshot rewrite"),S.GhostPhase==EGhostPhase::None && StoneDisplayColor(S,EStone::Black)!=StoneDisplayColor(S,EStone::White));
        if (Outcome!=0) { GhostReject(*this,S,FActionRequest::Place(GhostActor(S),{18,18}),EActionError::MatchStopped); }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostWinningReward,"Gomokards.Phase3B.WinningHiddenRewards",GhostFlags)
bool FGhostWinningReward::RunTest(const FString& Parameters)
{
    for (bool Sixth : {false,true})
    {
        FMatchState S;
        const EStone Color=Sixth ? EStone::Black : EStone::White;
        for (int32 X=12; X<16; ++X) { S.Board.At({X,10}).Stone=Color; }
        S.Board.At({16,11}).Stone=OppositeStone(Color); S.Board.At({16,12}).Stone=Color;
        StartHidden(S);
        if (Sixth)
        { for (int32 I=0; I<5; ++I) { GhostPlace(S,{I*2,1}); } }
        const int32 Actor=S.CurrentPlayerIndex;
        const auto R=GhostPlace(S,{16,10});
        TestTrue(TEXT("Winning hidden placement still earns one card"),R.bBlockingReward && S.Players[Actor].Hand.Num()==1);
        if (!Sixth)
        {
            TestTrue(TEXT("Early winning line not adjudicated"),S.Result.Status==EMatchStatus::InProgress);
            for (int32 I=0; I<5; ++I) { GhostPlace(S,{I*2,1}); }
        }
        TestTrue(TEXT("Reward retained through terminal reveal"),S.Result.Status==EMatchStatus::Won && S.Result.WinningStone==Color && S.Players[Actor].Hand.Num()>=1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostPersistence,"Gomokards.Phase3B.PersistentEffectsAndReset",GhostFlags)
bool FGhostPersistence::RunTest(const FString& Parameters)
{
    FMatchState S(82);
    S.bCardsDisabled=true; S.Players[0].Hand={ECardId::Ghost};
    GhostReject(*this,S,FActionRequest::Play(0,ECardId::Ghost),EActionError::CardsDisabled);
    S.bCardsDisabled=false;
    for (int32 X=0; X<4; ++X) { S.Board.At({X,4}).Stone=EStone::White; }
    S.Board.Barriers.Add({1,4}); S.Board.At({18,18}).bForbidden=true;
    S.Board.At({15,15}).Stone=EStone::Black; // Existing committed color (e.g. Polarity).
    ResolveAction(S,FActionRequest::Play(0,ECardId::Ghost));
    S.bCardsDisabled=true; // Trusted setup: independent lock must never be cleared on reveal.
    BeginGhostHidden(S);
    GhostReject(*this,S,FActionRequest::Place(1,{18,18}),EActionError::Forbidden);
    GhostPlace(S,{4,4});
    for (int32 I=0; I<5; ++I) { GhostPlace(S,{I*2,10}); }
    TestTrue(TEXT("Barrier remains effective in global reveal scan"),S.Result.Status==EMatchStatus::InProgress && S.Board.Barriers==TArray<FIntPoint>{FIntPoint(1,4)});
    TestTrue(TEXT("Forbidden/removed/committed stone metadata preserved"),S.Board.At({18,18})==FCell{EStone::Empty,true} && S.Board.At({15,15}).Stone==EStone::Black);
    TestTrue(TEXT("Ending temporary restriction never clears independent permanent lock"),S.GhostPhase==EGhostPhase::None && S.bCardsDisabled && !CanPlayCards(S));
    GhostReject(*this,S,FActionRequest::Play(GhostActor(S),ECardId::Ghost),EActionError::CardsDisabled);
    S.Reset(82); TestTrue(TEXT("Reset returns every state field to initial"),S==FMatchState(82) && CanPlayCards(S));
    StartHidden(S); GhostPlace(S,{2,2}); S.Reset(82);
    TestTrue(TEXT("Reset during Hidden restores visibility and card access"),S==FMatchState(82) && StoneDisplayColor(S,EStone::Black)!=StoneDisplayColor(S,EStone::White));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostPresentation,"Gomokards.Phase3B.VisibilityAndPool",GhostFlags)
bool FGhostPresentation::RunTest(const FString& Parameters)
{
    FMatchState S;
    const auto Black=StoneDisplayColor(S,EStone::Black),White=StoneDisplayColor(S,EStone::White);
    S.GhostPhase=EGhostPhase::Preparation;
    TestTrue(TEXT("Preparation retains distinct true colors"),StoneDisplayColor(S,EStone::Black)==Black && StoneDisplayColor(S,EStone::White)==White);
    TestTrue(TEXT("Countdown label shows frozen state"),GhostLabel(S,4.5).Contains(TEXT("4.5s")) && GhostLabel(S,4.5).Contains(TEXT("frozen")));
    S.GhostPhase=EGhostPhase::Hidden; S.GhostPlacementsCompleted=2;
    TestTrue(TEXT("Hidden label reports placements, not actions"),GhostLabel(S,0).Contains(TEXT("4 placements remaining")));
    TestTrue(TEXT("Empty stays empty; every occupied identity maps to same gray"),StoneDisplayColor(S,EStone::Empty)==FLinearColor::Transparent && StoneDisplayColor(S,EStone::Black)==StoneDisplayColor(S,EStone::White));
    TestEqual(TEXT("Exactly ten generated definitions"),GetPlayableCards().Num(),10);
    TestTrue(TEXT("Ghost is non-targeted playable"),FindCardDefinition(ECardId::Ghost) && !FindCardDefinition(ECardId::Ghost)->RequiresTarget());
    for (ECardId Id : {ECardId::FastDuel,ECardId::Undo,ECardId::Joker}) { TestNull(TEXT("Unimplemented card remains excluded"),FindCardDefinition(Id)); }
    FMatchState A(32),B(32); StartHidden(A); StartHidden(B);
    A.Board.At({6,5}).Stone=B.Board.At({6,5}).Stone=EStone::Black;
    A.Board.At({7,5}).Stone=B.Board.At({7,5}).Stone=EStone::White;
    GhostPlace(A,{5,5}); GhostPlace(B,{5,5});
    TestTrue(TEXT("Fixed seed + explicit transition + actions reproduce complete state"),A==B);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostRuntimeTest,"Gomokards.Phase3B.RuntimeTimerLifecycle",GhostFlags)
bool FGhostRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("Transient world"),World)) { return false; }
    auto* Owner=World->SpawnActor<ALocalMatchGameMode>();
    if (!TestNotNull(TEXT("Runtime owner"),Owner)) { World->DestroyWorld(false); return false; }
    auto Cast=[&]()
    {
        // White earns the first draw against the edge; Black earns Ghost on the second draw.
        Owner->StartWithSeed(6);
        for (FIntPoint P : {FIntPoint(0,0),FIntPoint(1,0),FIntPoint(2,0),FIntPoint(18,18)})
        { TestTrue(TEXT("Runtime opening accepted"),Owner->Submit(FActionRequest::Place(GhostActor(Owner->GetMatch()),P)).IsAccepted()); }
        TestTrue(TEXT("Runtime Ghost cast accepted"),Owner->Submit(FActionRequest::Play(GhostActor(Owner->GetMatch()),ECardId::Ghost)).IsAccepted());
    };
    int32 Notifications=0;
    Owner->OnMatchChanged.AddLambda([&Notifications]{++Notifications;});
    Cast();
    TestTrue(TEXT("One pending ticker after cast"),Owner->GhostTicker.IsValid());
    TestTrue(TEXT("Countdown starts near five real seconds"),Owner->GhostPreparationSecondsRemaining()>4 && Owner->GhostPreparationSecondsRemaining()<=5);
    const auto Generation=Owner->GhostTimerGeneration;
    const double Deadline=Owner->GhostDeadline;
    const FMatchState Before=Owner->GetMatch(); const int32 BeforeNotifications=Notifications;
    TestTrue(TEXT("Before deadline timer continues"),Owner->PollGhostPreparation(Deadline-.01,Generation));
    TestTrue(TEXT("Polling has no authoritative effect"),Owner->GetMatch()==Before && Notifications==BeforeNotifications);
    Owner->Submit(FActionRequest::Place(GhostActor(Before),{8,8}));
    TestTrue(TEXT("Rejected input never reschedules timer"),Owner->GhostTimerGeneration==Generation && Owner->GhostDeadline==Deadline && Notifications==BeforeNotifications);
    TestFalse(TEXT("Deadline completes without sleeping"),Owner->PollGhostPreparation(Deadline,Generation));
    FMatchState Expected=Before; BeginGhostHidden(Expected);
    TestTrue(TEXT("Timer invokes only logical transition and one notification"),Owner->GetMatch()==Expected && Notifications==BeforeNotifications+1 && !Owner->GhostTicker.IsValid());
    Owner->PollGhostPreparation(Deadline+1,Generation);
    TestTrue(TEXT("Duplicate expiration is harmless"),Owner->GetMatch()==Expected && Notifications==BeforeNotifications+1);
    Cast();
    const uint64 OldGeneration=Owner->GhostTimerGeneration; const double OldDeadline=Owner->GhostDeadline;
    Owner->NewMatch();
    TestTrue(TEXT("Restart cancels ticker/countdown and restores normal phase"),!Owner->GhostTicker.IsValid() && Owner->GhostPreparationSecondsRemaining()==0 && Owner->GetMatch().GhostPhase==EGhostPhase::None);
    const FMatchState Fresh=Owner->GetMatch();
    Owner->PollGhostPreparation(OldDeadline+10,OldGeneration);
    TestTrue(TEXT("Stale callback cannot transition new match"),Owner->GetMatch()==Fresh);
    Cast(); const FMatchState NewPreparation=Owner->GetMatch();
    Owner->PollGhostPreparation(OldDeadline+10,OldGeneration);
    TestTrue(TEXT("Stale callback cannot transition a newer Ghost cast either"),Owner->GetMatch()==NewPreparation && Owner->GhostTicker.IsValid());
    Owner->PollGhostPreparation(Owner->GhostDeadline,Owner->GhostTimerGeneration);
    Owner->NewMatch();
    TestTrue(TEXT("Runtime restart from Hidden is clean"),Owner->GetMatch()==FMatchState(Owner->GetMatch().Random.GetInitialSeed()));
    Cast(); Owner->EndPlay(EEndPlayReason::Quit);
    TestFalse(TEXT("Map teardown cancels preparation ticker"),Owner->GhostTicker.IsValid());
    Owner->OnMatchChanged.Clear(); World->DestroyWorld(false);
    return true;
}
#endif
