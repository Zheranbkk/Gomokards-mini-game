#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchGameState.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Presentation/SLocalMatchView.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
template<typename T> bool SameGhostView(const T& A,const T& B)
{ return T::StaticStruct()->CompareScriptStruct(&A,&B,0); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostNetworkTest,"Gomokards.Phase5A.GhostTransportAuthorityAndLifecycle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FGhostNetworkTest::RunTest(const FString&)
{
    using namespace Gomokards;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("World"),World)) { return false; }
    auto* GM=World->SpawnActor<ALocalMatchGameMode>(); auto* GS=World->SpawnActor<ALocalMatchGameState>();
    auto* A=World->SpawnActor<ALocalMatchPlayerController>(); auto* B=World->SpawnActor<ALocalMatchPlayerController>();
    auto* Third=World->SpawnActor<ALocalMatchPlayerController>();
    if (!GM || !GS || !A || !B || !Third) { AddError(TEXT("Fixture spawn failed")); World->DestroyWorld(false); return false; }
    World->SetGameState(GS); GM->GameState=GS; GM->bSessionInitialized=true;
    GM->StartWithSeed(182); GM->Join(A,true);
    // Kind: 0 ordinary card, 1 targeted card, 2 placement. These are existing adapters, not test-only RPCs.
    const auto Reject=[this,GM,GS,A,B](ALocalMatchPlayerController* PC,uint64 Epoch,uint64 Token,int32 Kind,uint8 Card,FIntPoint Point,EMatchIntentError Error,EActionError Rule=EActionError::None)
    {
        const auto State=GM->Match; const auto Revision=GM->Revision;
        const auto Public=GS->PublicView; const auto PA=A->PrivateView,PB=B->PrivateView;
        const auto Deadline=GM->GhostDeadline; const auto Generation=GM->GhostTimerGeneration;
        const auto Ack=Kind==2 ? GM->PlaceFrom(PC,Epoch,Token,Point) : (Kind==1 ? GM->TargetedCardFrom(PC,Epoch,Token,Card,Point) : GM->CardFrom(PC,Epoch,Token,Card));
        TestTrue(TEXT("Expected safe rejection"),!Ack.bAccepted && Ack.Error==Error && Ack.RuleError==uint8(Rule));
        TestTrue(TEXT("Rejected request preserves full state/RNG/action/effects and revision"),GM->Match==State && GM->Revision==Revision);
        TestTrue(TEXT("Rejected request does not reschedule preparation"),GM->GhostDeadline==Deadline && GM->GhostTimerGeneration==Generation);
        TestTrue(TEXT("Rejection leaves all projections unchanged"),SameGhostView(Public,GS->PublicView) && SameGhostView(PA,A->PrivateView) && SameGhostView(PB,B->PrivateView));
    };
    Reject(A,GM->MatchEpoch,0,0,uint8(ECardId::Ghost),{},EMatchIntentError::NotPlaying);
    GM->Join(B,false);
    const auto Reset=[GM,A,B](int32 Seed=182)
    { GM->Session=EMatchSession::Playing; GM->Assignments[A]=0; GM->Assignments[B]=1; GM->StartWithSeed(Seed); };
    const auto CheckViews=[this,GM,GS,A,B]()
    {
        for (auto* PC : {A,B})
        {
            PC->RefreshPresentation();
            const auto Private=MakePrivateView(GM->Match,GM->Assignments[PC],GM->DevelopmentAdmins.Contains(PC),GM->MatchEpoch,GM->Revision);
            TestTrue(TEXT("Both displayed views coherent; exact own hand only"),PC->IsPresentationReady() && SameGhostView(PC->GetPublicView(),GS->PublicView) && SameGhostView(PC->GetPrivateView(),Private));
            TestTrue(TEXT("Both private revision stamps updated"),PC->PrivateView.Epoch==GS->PublicView.Epoch && PC->PrivateView.Revision==GS->PublicView.Revision);
        }
        for (int32 I=0; I<361; ++I)
        {
            const auto& C=GM->Match.Board.Cells[I]; const auto& P=GS->PublicView.Cells[I];
            const uint8 Expected=GM->Match.GhostPhase==EGhostPhase::Hidden && C.Stone!=EStone::Empty ? uint8(EMatchDisplayStone::HiddenOccupied) : uint8(C.Stone);
            TestTrue(TEXT("Actual transported cell is redacted, not merely gray-painted"),P.Stone==Expected && P.bForbidden==C.bForbidden);
        }
        TestTrue(TEXT("Public phase/count/effects match authority"),GS->PublicView.GhostPhase==static_cast<EMatchGhostPhase>(GM->Match.GhostPhase) && GS->PublicView.GhostPlacementsCompleted==GM->Match.GhostPlacementsCompleted && GS->PublicView.ConfusionRemaining==GM->Match.ConfusionActionsRemaining);
        TestTrue(TEXT("Public barriers preserved"),GS->PublicView.Barriers==GM->Match.Board.Barriers);
        for (int32 I=0; I<2; ++I) { TestEqual(TEXT("Public counts match private ownership"),GS->PublicView.Seats[I].HandCount,GM->Match.Players[I].Hand.Num()); }
    };
    Reset();
    Reject(A,GM->MatchEpoch,0,0,uint8(ECardId::Ghost),{},EMatchIntentError::RuleRejected,EActionError::CardNotOwned);
    GM->Match.Players[0].Hand={ECardId::Ghost}; GM->Match.Players[1].Hand={ECardId::Ghost}; GM->PublishViews();
    Reject(Third,GM->MatchEpoch,0,0,uint8(ECardId::Ghost),{},EMatchIntentError::Unassigned);
    Reject(B,GM->MatchEpoch,0,0,uint8(ECardId::Ghost),{},EMatchIntentError::RuleRejected,EActionError::WrongPlayer);
    Reject(A,GM->MatchEpoch-1,0,0,uint8(ECardId::Ghost),{},EMatchIntentError::StaleEpoch);
    Reject(A,GM->MatchEpoch,1,0,uint8(ECardId::Ghost),{},EMatchIntentError::StaleAction);
    Reject(A,GM->MatchEpoch,0,1,uint8(ECardId::Ghost),{5,5},EMatchIntentError::CardNotNetworkEnabled);
    for (int32 Card=0; Card<=255; ++Card)
    {
        if (!IsNetworkCardEnabled(uint8(Card))) { Reject(A,GM->MatchEpoch,0,0,uint8(Card),{},EMatchIntentError::CardNotNetworkEnabled); }
        if (!IsTargetedNetworkCardEnabled(uint8(Card))) { Reject(A,GM->MatchEpoch,0,1,uint8(Card),{5,5},EMatchIntentError::CardNotNetworkEnabled); }
    }
    GM->Match.bCardsDisabled=true; GM->PublishViews();
    Reject(A,GM->MatchEpoch,0,0,uint8(ECardId::Ghost),{},EMatchIntentError::RuleRejected,EActionError::CardsDisabled);
    GM->Match.bCardsDisabled=false; GM->Match.Result.Status=EMatchStatus::Won; GM->PublishViews();
    Reject(A,GM->MatchEpoch,0,0,uint8(ECardId::Ghost),{},EMatchIntentError::RuleRejected,EActionError::MatchStopped);
    GM->Session=EMatchSession::SessionEnded; GM->PublishViews();
    Reject(A,GM->MatchEpoch,0,0,uint8(ECardId::Ghost),{},EMatchIntentError::NotPlaying);

    for (bool Reverse : {false,true})
    {
        Reset(); if (Reverse) { GM->Assignments[A]=1; GM->Assignments[B]=0; }
        auto* Actor=Reverse ? B : A; auto* Other=Reverse ? A : B;
        GM->Match.Players[0].Hand={ECardId::Ghost,ECardId::TacticalNuke}; GM->Match.Players[1].Hand={ECardId::Restock,ECardId::Barrier};
        GM->Match.Board.At({2,2}).Stone=EStone::Black; GM->Match.Board.At({3,2}).Stone=EStone::White;
        GM->Match.ConfusionActionsRemaining=2; GM->PublishViews();
        Actor->ToggleTargeting(uint8(ECardId::TacticalNuke));
        TestTrue(TEXT("Eligible owner's Ghost is enabled"),Actor->CanPlayCard(uint8(ECardId::Ghost)));
        const auto Before=GM->Match; auto Expected=Before; ResolveAction(Expected,FActionRequest::Play(0,ECardId::Ghost));
        TestTrue(TEXT("Ghost accepted through ordinary card adapter"),GM->CardFrom(Actor,GM->MatchEpoch,0,uint8(ECardId::Ghost)).bAccepted);
        TestTrue(TEXT("Exact Core activation, one ordinary action and transfer"),GM->Match==Expected && GM->Match.CompletedActions==1 && GM->Match.CurrentPlayerIndex==1 && GM->Match.ConfusionActionsRemaining==1);
        CheckViews();
        TestTrue(TEXT("Preparation endpoint in display clock and cleared local target"),GS->PublicView.GhostDisplayEndServerTime>GS->GetServerWorldTimeSeconds() && GS->PublicView.GhostDisplayEndServerTime<=GS->GetServerWorldTimeSeconds()+5.01 && Actor->SelectedTargetedCard==0);
        TestTrue(TEXT("Preparation controls frozen"),!Other->CanPlace({6,6}) && !Other->CanPlayCard(uint8(ECardId::Restock)) && !Other->CanTargetCard(uint8(ECardId::Barrier)));
        Other->RequestBoardClick({6,6});
        TestFalse(TEXT("Preparation board click sends no outstanding intent"),Other->bPending);
        Reject(Other,GM->MatchEpoch,1,2,0,{6,6},EMatchIntentError::RuleRejected,EActionError::GhostPreparation);
        Reject(Other,GM->MatchEpoch,1,0,uint8(ECardId::Restock),{},EMatchIntentError::RuleRejected,EActionError::GhostPreparation);
        Reject(Other,GM->MatchEpoch,1,1,uint8(ECardId::Barrier),{6,6},EMatchIntentError::RuleRejected,EActionError::GhostPreparation);
        const auto Prep=GM->Match; const auto PrepRevision=GM->Revision; const auto Deadline=GM->GhostDeadline; const auto Generation=GM->GhostTimerGeneration;
        TestTrue(TEXT("Before deadline unchanged"),GM->PollGhostPreparation(Deadline-.001,Generation) && GM->Match==Prep && GM->Revision==PrepRevision);
        // A displayed zero cannot cause any authoritative transition.
        Other->DisplayPublic.GhostDisplayEndServerTime=GS->GetServerWorldTimeSeconds()-1;
        TestTrue(TEXT("Countdown zero is frozen display only"),Other->GhostStatusLabel().Contains(TEXT("0.0s")) && GM->Match==Prep && GM->Revision==PrepRevision);
        TestFalse(TEXT("Deadline transitions once, retires ticker"),GM->PollGhostPreparation(Deadline,Generation));
        TestTrue(TEXT("Timer publishes one revision, no ordinary action or RNG change"),GM->Revision==PrepRevision+1 && GM->Match.CompletedActions==Prep.CompletedActions && GM->Match.Random.GetCurrentSeed()==Prep.Random.GetCurrentSeed() && GM->Match.Board==Prep.Board);
        CheckViews();
        TestTrue(TEXT("Timer transition needs no client acknowledgement"),!Actor->bPending && !Other->bPending && GS->PublicView.GhostDisplayEndServerTime==0);
        const auto Hidden=GM->Match; const auto HiddenRevision=GM->Revision;
        GM->PollGhostPreparation(Deadline+1,Generation);
        TestTrue(TEXT("Old callback cannot repeat transition"),GM->Match==Hidden && GM->Revision==HiddenRevision);
        Reject(Actor,GM->MatchEpoch,1,2,0,{6,6},EMatchIntentError::RuleRejected,EActionError::WrongPlayer);
        Reject(Other,GM->MatchEpoch-1,1,2,0,{6,6},EMatchIntentError::StaleEpoch);
        Reject(Other,GM->MatchEpoch,0,2,0,{6,6},EMatchIntentError::StaleAction);
        Reject(Other,GM->MatchEpoch,1,2,0,{-1,6},EMatchIntentError::RuleRejected,EActionError::InvalidCoordinate);
        Reject(Other,GM->MatchEpoch,1,2,0,{2,2},EMatchIntentError::RuleRejected,EActionError::Occupied);
        Reject(Other,GM->MatchEpoch,1,0,uint8(ECardId::Restock),{},EMatchIntentError::RuleRejected,EActionError::GhostCardsRestricted);
        Reject(Other,GM->MatchEpoch,1,1,uint8(ECardId::Barrier),{6,6},EMatchIntentError::RuleRejected,EActionError::GhostCardsRestricted);
        TestTrue(TEXT("Hidden placement allowed"),Other->CanPlace({6,6}) && GM->PlaceFrom(Other,GM->MatchEpoch,1,{6,6}).bAccepted);
        TestTrue(TEXT("Confusion uses actual reversed color without ownership swap"),GM->Match.Board.At({6,6}).Stone==EStone::Black && Other->PrivateView.PlayerId==1 && Other->PrivateView.Stone==uint8(EStone::White) && GM->Match.ConfusionActionsRemaining==0 && GM->Match.GhostPlacementsCompleted==1);
        CheckViews();
        TSharedPtr<SLocalMatchView> View=SNew(SLocalMatchView).Owner(Actor);
        TestEqual(TEXT("Hidden hover preview carries no Black/White color"),View->PreviewStone(),uint8(EMatchDisplayStone::HiddenOccupied)); View.Reset();
        GM->Join(Third,false);
        TestTrue(TEXT("Unassigned client gets no private board or hand"),Third->PrivateView.PlayerId==INDEX_NONE && Third->PrivateView.Hand.IsEmpty());
        TestEqual(TEXT("Unassigned clients' shared public board is redacted too"),GS->PublicView.Cells[FBoard::ToIndex({6,6})].Stone,uint8(EMatchDisplayStone::HiddenOccupied));
    }

    // Cross-Actor arrival order: public Hidden masks stale visible display before private catch-up.
    Reset(); GM->Match.Players[0].Hand={ECardId::Ghost}; GM->Match.Board.At({2,2}).Stone=EStone::Black; GM->PublishViews();
    GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Ghost)); CheckViews();
    const auto PrepPublic=GS->PublicView; const auto PrepPrivate=A->PrivateView;
    GM->PollGhostPreparation(GM->GhostDeadline,GM->GhostTimerGeneration); CheckViews();
    const auto HiddenPublic=GS->PublicView; const auto HiddenPrivate=A->PrivateView;
    A->DisplayPublic=PrepPublic; A->DisplayPrivate=PrepPrivate; A->PrivateView=PrepPrivate; A->RefreshPresentation();
    TestTrue(TEXT("Public-first hides stale colors and gates interaction"),!A->IsPresentationReady() && A->GetPublicView().Cells[FBoard::ToIndex({2,2})].Stone==uint8(EMatchDisplayStone::HiddenOccupied));
    A->PrivateView=HiddenPrivate; A->OnRep_PrivateView(); TestTrue(TEXT("Private catch-up restores coherence"),A->IsPresentationReady());
    GS->PublicView=PrepPublic; A->DisplayPublic=PrepPublic; A->DisplayPrivate=PrepPrivate; A->RefreshPresentation();
    TestFalse(TEXT("Private-first disables until Hidden public arrives"),A->IsPresentationReady());
    GS->PublicView=HiddenPublic; A->RefreshPresentation(); CheckViews();

    // Global reveal results and Barrier use existing Core; first five never adjudicate lines.
    for (int32 Outcome=0; Outcome<4; ++Outcome) // Black, White, Draw, Barrier-blocked continuing.
    {
        Reset(); GM->Match.CurrentPlayerIndex=1; GM->Match.Players[1].Hand={ECardId::Ghost};
        for (int32 X=0; X<4; ++X)
        {
            if (Outcome!=1) { GM->Match.Board.At({X,2}).Stone=EStone::Black; }
            if (Outcome==1 || Outcome==2) { GM->Match.Board.At({X,4}).Stone=EStone::White; }
        }
        if (Outcome==3) { GM->Match.Board.Barriers.Add({1,2}); }
        GM->PublishViews(); GM->CardFrom(B,GM->MatchEpoch,0,uint8(ECardId::Ghost));
        GM->PollGhostPreparation(GM->GhostDeadline,GM->GhostTimerGeneration);
        const TArray<FIntPoint> Points={{4,2},{4,4},{10,10},{12,10},{10,12},{12,12}};
        FMatchPublicView FifthPublic; FMatchPrivateView FifthPrivate;
        for (int32 I=0; I<6; ++I)
        {
            auto* PC=I%2==0 ? A : B; auto Expected=GM->Match;
            ResolveAction(Expected,FActionRequest::Place(I%2,Points[I])); const auto Revision=GM->Revision;
            TestTrue(TEXT("Each Hidden placement is one exact Core commit"),GM->PlaceFrom(PC,GM->MatchEpoch,GM->Match.CompletedActions,Points[I]).bAccepted && GM->Match==Expected && GM->Revision==Revision+1); CheckViews();
            if (I<5) { TestTrue(TEXT("First five keep Hidden and suppress line wins"),GM->Match.GhostPhase==EGhostPhase::Hidden && GM->Match.GhostPlacementsCompleted==I+1 && GM->Match.Result.Status==EMatchStatus::InProgress); }
            if (I==4) { FifthPublic=GS->PublicView; FifthPrivate=A->PrivateView; }
        }
        const auto Status=Outcome==2 ? EMatchStatus::Draw : (Outcome==3 ? EMatchStatus::InProgress : EMatchStatus::Won);
        TestTrue(TEXT("Sixth atomically reveals with expected outcome"),GM->Match.GhostPhase==EGhostPhase::None && GM->Match.Result.Status==Status && GS->PublicView.Result==uint8(Status));
        if (Outcome<2) { TestEqual(TEXT("Correct winner color"),GM->Match.Result.WinningStone,Outcome==0 ? EStone::Black : EStone::White); }
        const auto FinalPrivate=A->PrivateView; A->DisplayPublic=FifthPublic; A->DisplayPrivate=FifthPrivate; A->PrivateView=FifthPrivate; A->RefreshPresentation();
        TestTrue(TEXT("Reveal waits for same-revision private hand, no mixed interaction"),!A->IsPresentationReady() && A->GetPublicView().GhostPhase==EMatchGhostPhase::Hidden);
        A->PrivateView=FinalPrivate; A->OnRep_PrivateView(); CheckViews();
        if (Outcome!=3) { Reject(A,GM->MatchEpoch,GM->Match.CompletedActions,2,0,{18,18},EMatchIntentError::RuleRejected,EActionError::MatchStopped); }
    }

    // Pathological boards preserve stopped/no-action semantics, never invent an early reveal/winner.
    Reset(); for (auto& C : GM->Match.Board.Cells) { C.bForbidden=true; }
    GM->Match.Players[0].Hand={ECardId::Ghost}; GM->PublishViews();
    TestTrue(TEXT("Ghost on board with no placement still enters Preparation"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Ghost)).bAccepted && GM->Session==EMatchSession::Playing && GM->GhostTicker.IsValid());
    GM->PollGhostPreparation(GM->GhostDeadline,GM->GhostTimerGeneration);
    TestTrue(TEXT("No-progress Hidden ends runtime slice without invented Core result/reveal"),GM->Session==EMatchSession::SessionEnded && GM->Match.GhostPhase==EGhostPhase::Hidden && GM->Match.Result.Status==EMatchStatus::InProgress);
    Reset(); for (auto& C : GM->Match.Board.Cells) { C.bForbidden=true; } GM->Match.Board.At({5,5}).bForbidden=false;
    GM->Match.Players[0].Hand={ECardId::Ghost}; GM->PublishViews(); GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Ghost));
    GM->PollGhostPreparation(GM->GhostDeadline,GM->GhostTimerGeneration);
    TestTrue(TEXT("Hidden continues with legal placement despite no card capability"),GM->Session==EMatchSession::Playing);
    GM->PlaceFrom(B,GM->MatchEpoch,1,{5,5}); CheckViews();
    TestTrue(TEXT("Exhaustion retains existing Core unresolved rule, redacted Hidden"),GM->Match.Result.Status==EMatchStatus::AwaitingRuleDecision && GM->Match.Result.Decision==EDecisionReason::NoLegalAction && GM->Match.GhostPhase==EGhostPhase::Hidden);

    // Deterministic user recipe: real rewards, no hand grants.
    Reset(182);
    const TArray<FIntPoint> Opening={{5,5},{6,5},{0,0},{7,5},{8,5},{1,0},{9,9},{10,9},{12,12},{11,9},{12,9},{5,12}};
    for (int32 I=0; I<Opening.Num(); ++I) { TestTrue(TEXT("Recipe opening"),GM->PlaceFrom(I%2==0 ? A : B,GM->MatchEpoch,I,Opening[I]).bAccepted); }
    TestTrue(TEXT("Recipe naturally gives Black Confusion/Restock and White Ghost"),GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::Confusion,ECardId::Restock} && GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::Ghost});
    TestTrue(TEXT("Recipe Black Confusion"),GM->CardFrom(A,GM->MatchEpoch,12,uint8(ECardId::Confusion)).bAccepted);
    TestTrue(TEXT("Recipe White Ghost consumes one Confusion"),GM->CardFrom(B,GM->MatchEpoch,13,uint8(ECardId::Ghost)).bAccepted && GM->Match.ConfusionActionsRemaining==1);
    GM->PollGhostPreparation(GM->GhostDeadline,GM->GhostTimerGeneration); CheckViews();
    const TArray<FIntPoint> HiddenMoves={{8,9},{15,15},{17,17},{15,17},{17,15},{16,16}};
    for (int32 I=0; I<6; ++I)
    {
        const auto OldOther=GM->Match.Players[1-I%2].Hand;
        const auto Ack=GM->PlaceFrom(I%2==0 ? A : B,GM->MatchEpoch,14+I,HiddenMoves[I]);
        TestTrue(TEXT("Recipe hidden move and exact single reward event"),Ack.bAccepted && Ack.bBlockingReward==(I==0));
        TestTrue(TEXT("Other owner's private hand never receives the reward"),GM->Match.Players[1-I%2].Hand==OldOther); CheckViews();
        if (I==0)
        {
            TestTrue(TEXT("Black's reversed White stone stays hidden; exact private Restock reward"),GM->Match.Board.At({8,9}).Stone==EStone::White && GS->PublicView.Cells[FBoard::ToIndex({8,9})].Stone==uint8(EMatchDisplayStone::HiddenOccupied) && GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::Restock,ECardId::Restock} && GM->Match.ConfusionActionsRemaining==0 && A->PrivateView.Stone==uint8(EStone::Black));
        }
    }
    TestTrue(TEXT("Recipe reveal continues, Black resumes with all colors restored"),GM->Match.Result.Status==EMatchStatus::InProgress && GM->Match.GhostPhase==EGhostPhase::None && GM->Match.CurrentPlayerIndex==0);
    TestTrue(TEXT("Normal play after recipe"),GM->PlaceFrom(A,GM->MatchEpoch,20,{18,0}).bAccepted);

    // Lifecycle generation guards preserve both authority and network revision after cancellation.
    for (int32 End=0; End<3; ++End)
    {
        Reset(); GM->Match.Players[0].Hand={ECardId::Ghost}; GM->PublishViews(); GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Ghost));
        const auto Deadline=GM->GhostDeadline; const auto Generation=GM->GhostTimerGeneration; const auto Epoch=GM->MatchEpoch;
        if (End==0) { TestTrue(TEXT("Authorized restart during Preparation"),GM->RestartFrom(A,Epoch).bAccepted && GM->MatchEpoch>Epoch); }
        else if (End==1) { GM->Leave(B); }
        else { GM->EndPlay(EEndPlayReason::Destroyed); }
        const auto Before=GM->Match; const auto Revision=GM->Revision;
        TestFalse(TEXT("Cancelled generation cannot run"),GM->PollGhostPreparation(Deadline+10,Generation));
        TestTrue(TEXT("No stale mutation/publication after reset/session end/teardown"),GM->Match==Before && GM->Revision==Revision && !GM->GhostTicker.IsValid());
        if (End==1) { GM->Assignments.Add(B,1); }
    }
    World->DestroyWorld(false);
    return true;
}
#endif
