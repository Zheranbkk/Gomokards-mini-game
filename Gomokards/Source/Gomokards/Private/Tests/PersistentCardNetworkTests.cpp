#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchGameState.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
template<typename T> bool SamePersistentView(const T& A,const T& B)
{ return T::StaticStruct()->CompareScriptStruct(&A,&B,0); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersistentCardNetworkTest,"Gomokards.Phase4B3.PersistentAuthorityAndRecipes",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPersistentCardNetworkTest::RunTest(const FString&)
{
    using namespace Gomokards;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("World"),World)) { return false; }
    auto* GM=World->SpawnActor<ALocalMatchGameMode>();
    auto* GS=World->SpawnActor<ALocalMatchGameState>();
    auto* A=World->SpawnActor<ALocalMatchPlayerController>();
    auto* B=World->SpawnActor<ALocalMatchPlayerController>();
    auto* Unassigned=World->SpawnActor<ALocalMatchPlayerController>();
    if (!GM || !GS || !A || !B || !Unassigned) { AddError(TEXT("Fixture spawn failed")); World->DestroyWorld(false); return false; }
    World->SetGameState(GS); GM->GameState=GS; GM->bSessionInitialized=true;
    GM->StartWithSeed(182); GM->Join(A,true);
    const auto Reject=[this,GM,GS,A,B](ALocalMatchPlayerController* PC,uint64 Epoch,uint64 Token,ECardId Card,
        EMatchIntentError Error,EActionError Rule=EActionError::None,bool bTargeted=false)
    {
        const auto Before=GM->Match; const auto Revision=GM->Revision;
        const auto Public=GS->PublicView; const auto PrivateA=A->PrivateView,PrivateB=B->PrivateView;
        const auto Ack=bTargeted ? GM->TargetedCardFrom(PC,Epoch,Token,uint8(Card),{5,5}) : GM->CardFrom(PC,Epoch,Token,uint8(Card));
        TestTrue(TEXT("Correct rejection reason"),!Ack.bAccepted && Ack.Error==Error && Ack.RuleError==uint8(Rule));
        TestTrue(TEXT("Rejection preserves complete state, effects, RNG and revision"),GM->Match==Before && GM->Revision==Revision);
        TestTrue(TEXT("Rejection preserves public and both private projections"),SamePersistentView(Public,GS->PublicView) && SamePersistentView(PrivateA,A->PrivateView) && SamePersistentView(PrivateB,B->PrivateView));
    };
    for (auto Card : {ECardId::Confusion,ECardId::BackToBasics})
    { Reject(A,GM->MatchEpoch,0,Card,EMatchIntentError::NotPlaying); }
    GM->Join(B,false);
    const auto Reset=[GM,A,B](int32 Seed=182)
    {
        GM->Session=EMatchSession::Playing; GM->Assignments[A]=0; GM->Assignments[B]=1;
        GM->StartWithSeed(Seed);
    };
    const auto CheckViews=[this,GM,GS,A,B]()
    {
        for (auto* PC : {A,B})
        {
            PC->RefreshPresentation();
            const auto Private=MakePrivateView(GM->Match,GM->Assignments[PC],GM->DevelopmentAdmins.Contains(PC),GM->MatchEpoch,GM->Revision);
            TestTrue(TEXT("Each owner sees exact own hand and coherent public revision"),PC->IsPresentationReady() && SamePersistentView(PC->GetPrivateView(),Private) && SamePersistentView(PC->GetPublicView(),GS->PublicView) && Private.Revision==GS->PublicView.Revision);
        }
        TestTrue(TEXT("Shared effect count and card lock are authority projections"),GS->PublicView.ConfusionRemaining==GM->Match.ConfusionActionsRemaining && GS->PublicView.bCardsDisabled==GM->Match.bCardsDisabled);
        for (int32 I=0; I<361; ++I)
        { TestTrue(TEXT("Board history projected exactly"),GS->PublicView.Cells[I].Stone==uint8(GM->Match.Board.Cells[I].Stone) && GS->PublicView.Cells[I].bForbidden==GM->Match.Board.Cells[I].bForbidden); }
        TestTrue(TEXT("Barrier history projected exactly"),GS->PublicView.Barriers==GM->Match.Board.Barriers);
        for (int32 I=0; I<2; ++I) { TestEqual(TEXT("Only public count accompanies private hand"),GS->PublicView.Seats[I].HandCount,GM->Match.Players[I].Hand.Num()); }
    };
    for (auto Card : {ECardId::Confusion,ECardId::BackToBasics})
    {
        Reset(); GM->Match.ConfusionActionsRemaining=2; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,Card,EMatchIntentError::RuleRejected,EActionError::CardNotOwned);
        GM->Match.Players[0].Hand={Card}; GM->Match.Players[1].Hand={Card}; GM->PublishViews();
        Reject(Unassigned,GM->MatchEpoch,0,Card,EMatchIntentError::Unassigned);
        Reject(B,GM->MatchEpoch,0,Card,EMatchIntentError::RuleRejected,EActionError::WrongPlayer);
        Reject(A,GM->MatchEpoch-1,0,Card,EMatchIntentError::StaleEpoch);
        Reject(A,GM->MatchEpoch,1,Card,EMatchIntentError::StaleAction);
        Reject(A,GM->MatchEpoch,0,Card,EMatchIntentError::CardNotNetworkEnabled,EActionError::None,true);
        GM->Match.Result.Status=EMatchStatus::Won; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,Card,EMatchIntentError::RuleRejected,EActionError::MatchStopped);
        GM->Match.Result.Status=EMatchStatus::InProgress; GM->Match.bCardsDisabled=true; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,Card,EMatchIntentError::RuleRejected,EActionError::CardsDisabled);
        GM->Session=EMatchSession::SessionEnded; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,Card,EMatchIntentError::NotPlaying);
    }
    // Both controller assignments use server identity and the same atomic Core resolution.
    for (bool bReverse : {false,true})
    for (auto Card : {ECardId::Confusion,ECardId::BackToBasics})
    {
        Reset(); if (bReverse) { GM->Assignments[A]=1; GM->Assignments[B]=0; }
        auto* Actor=bReverse ? B : A;
        GM->Match.Players[0].Hand={Card,ECardId::TacticalNuke}; GM->Match.Players[1].Hand={ECardId::Ghost};
        GM->Match.ConfusionActionsRemaining=2; GM->PublishViews();
        TestTrue(TEXT("Persistent card enabled only for current owner"),Actor->CanPlayCard(uint8(Card)));
        const auto Before=GM->Match; const auto Revision=GM->Revision;
        auto Expected=Before; ResolveAction(Expected,FActionRequest::Play(0,Card));
        const auto Ack=GM->CardFrom(Actor,GM->MatchEpoch,0,uint8(Card));
        TestTrue(TEXT("Persistent network commit matches Core; one action/revision"),Ack.bAccepted && GM->Match==Expected && GM->Revision==Revision+1 && GM->Match.CompletedActions==1 && GM->Match.CurrentPlayerIndex==1);
        TestTrue(TEXT("Consumed card and unchanged RNG/identity"),GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::TacticalNuke} && GM->Match.Random.GetCurrentSeed()==Before.Random.GetCurrentSeed() && Actor->PrivateView.PlayerId==0 && Actor->PrivateView.Stone==uint8(EStone::Black));
        TestEqual(TEXT("Confusion recast refreshes; Basics clears"),GM->Match.ConfusionActionsRemaining,Card==ECardId::Confusion ? 2 : 0); CheckViews();
    }
    // Shared action counter, both colors, rejected input and target cancellation.
    Reset(); GM->Match.Players[0].Hand={ECardId::Confusion,ECardId::Restock};
    GM->Match.Players[1].Hand={ECardId::Barrier}; GM->PublishViews();
    TestTrue(TEXT("Confusion activation"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Confusion)).bAccepted);
    CheckViews(); TestEqual(TEXT("Two future shared actions"),GS->PublicView.ConfusionRemaining,2);
    const auto SelectionState=GM->Match; const auto SelectionRevision=GM->Revision;
    B->ToggleTargeting(uint8(ECardId::Barrier)); B->CancelTargeting();
    TestTrue(TEXT("Target selection/cancel consumes nothing"),GM->Match==SelectionState && GM->Revision==SelectionRevision && !B->bPending);
    TestFalse(TEXT("Invalid placement rejects"),GM->PlaceFrom(B,GM->MatchEpoch,1,{-1,0}).bAccepted);
    TestTrue(TEXT("Invalid placement preserves shared effect/RNG/revision"),GM->Match==SelectionState && GM->Revision==SelectionRevision);
    TestTrue(TEXT("White identity places Black under shared Confusion"),GM->PlaceFrom(B,GM->MatchEpoch,1,{5,5}).bAccepted && GM->Match.Board.At({5,5}).Stone==EStone::Black && B->PrivateView.PlayerId==1 && B->PrivateView.Stone==uint8(EStone::White));
    TestEqual(TEXT("Placement consumes 2 to 1"),GM->Match.ConfusionActionsRemaining,1); CheckViews();
    TestTrue(TEXT("Restock is an action event"),GM->CardFrom(A,GM->MatchEpoch,2,uint8(ECardId::Restock)).bAccepted);
    TestEqual(TEXT("Card consumes 1 to 0"),GM->Match.ConfusionActionsRemaining,0); CheckViews();
    TestTrue(TEXT("White resumes assigned color"),GM->PlaceFrom(B,GM->MatchEpoch,3,{7,7}).bAccepted && GM->Match.Board.At({7,7}).Stone==EStone::White);
    Reset(); GM->Match.Players[0].Hand={ECardId::Confusion}; GM->PublishViews();
    GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Confusion));
    GM->PlaceFrom(B,GM->MatchEpoch,1,{5,5});
    TestTrue(TEXT("Caster also affected without identity change"),GM->PlaceFrom(A,GM->MatchEpoch,2,{7,7}).bAccepted && GM->Match.Board.At({7,7}).Stone==EStone::White && A->PrivateView.Stone==uint8(EStone::Black) && GM->Match.ConfusionActionsRemaining==0);

    // Actual prior card effects, then Basics: no rollback and no extra global result scan.
    Reset(); GM->Match.Players[0].Hand={ECardId::TacticalNuke,ECardId::Barrier,ECardId::BackToBasics,ECardId::Restock};
    GM->Match.Players[1].Hand={ECardId::Polarity,ECardId::Confusion,ECardId::Steal,ECardId::TacticalNuke};
    GM->Match.Board.At({1,1}).Stone=EStone::Black; GM->Match.Board.At({5,5}).Stone=EStone::White; GM->PublishViews();
    TestTrue(TEXT("History Nuke"),GM->TargetedCardFrom(A,GM->MatchEpoch,0,uint8(ECardId::TacticalNuke),{1,1}).bAccepted);
    TestTrue(TEXT("History Polarity"),GM->TargetedCardFrom(B,GM->MatchEpoch,1,uint8(ECardId::Polarity),{5,5}).bAccepted);
    TestTrue(TEXT("History Barrier"),GM->TargetedCardFrom(A,GM->MatchEpoch,2,uint8(ECardId::Barrier),{8,8}).bAccepted);
    TestTrue(TEXT("Active Confusion before Basics"),GM->CardFrom(B,GM->MatchEpoch,3,uint8(ECardId::Confusion)).bAccepted && GM->Match.ConfusionActionsRemaining==2);
    // Stale local selection may survive property transport briefly; commit must clear it on both views.
    A->SelectedTargetedCard=uint8(ECardId::Barrier); B->SelectedTargetedCard=uint8(ECardId::TacticalNuke);
    const auto BeforeBasics=GM->Match; const auto BasicsRevision=GM->Revision;
    TestTrue(TEXT("Basics accepted"),GM->CardFrom(A,GM->MatchEpoch,4,uint8(ECardId::BackToBasics)).bAccepted);
    TestTrue(TEXT("Basics clears rule effect, preserves entire board and RNG"),GM->Match.bCardsDisabled && GM->Match.ConfusionActionsRemaining==0 && GM->Match.Board==BeforeBasics.Board && GM->Match.Random.GetCurrentSeed()==BeforeBasics.Random.GetCurrentSeed());
    TestTrue(TEXT("Basics clears stale targeting without extra action"),A->SelectedTargetedCard==0 && B->SelectedTargetedCard==0 && GM->Match.CompletedActions==5 && GM->Revision==BasicsRevision+1); CheckViews();
    for (const auto& Def : GetPlayableCards())
    {
        TestTrue(TEXT("All own-card UI inputs disabled after Basics"),!B->CanPlayCard(uint8(Def.Id)) && !B->CanTargetCard(uint8(Def.Id)));
        const bool bBasic=IsNetworkCardEnabled(uint8(Def.Id)),bTarget=IsTargetedNetworkCardEnabled(uint8(Def.Id));
        Reject(B,GM->MatchEpoch,5,Def.Id,bBasic ? EMatchIntentError::RuleRejected : EMatchIntentError::CardNotNetworkEnabled,bBasic ? EActionError::CardsDisabled : EActionError::None);
        Reject(B,GM->MatchEpoch,5,Def.Id,bTarget ? EMatchIntentError::RuleRejected : EMatchIntentError::CardNotNetworkEnabled,bTarget ? EActionError::CardsDisabled : EActionError::None,true);
    }
    TestTrue(TEXT("Ordinary placement after Basics uses normal White"),GM->PlaceFrom(B,GM->MatchEpoch,5,{12,12}).bAccepted && GM->Match.Board.At({12,12}).Stone==EStone::White); CheckViews();
    TestTrue(TEXT("Development restart restores card availability and clears effects"),GM->RestartFrom(A,GM->MatchEpoch).bAccepted && !GM->Match.bCardsDisabled && GM->Match.ConfusionActionsRemaining==0);

    // Fixed-seed manual recipes use only normal rewards and production adapter actions.
    const TArray<FIntPoint> Opening={{5,5},{6,5},{0,0},{7,5},{8,5},{1,0},{9,9},{10,9},
        {12,12},{11,9},{12,9},{5,12},{6,12},{0,18},{7,12},{8,12}};
    const auto Open=[this,GM,A,B,&Opening](int32 Count)
    {
        for (int32 I=0; I<Count; ++I)
        {
            const auto Ack=GM->PlaceFrom(I%2==0 ? A : B,GM->MatchEpoch,I,Opening[I]);
            TestTrue(TEXT("Manual opening accepted with exact reward points"),Ack.bAccepted && Ack.bBlockingReward==(I==4 || I==5 || I==10 || I==15));
        }
    };
    Reset(182); Open(12);
    TestTrue(TEXT("Seed 182 natural hands"),GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::Confusion,ECardId::Restock} && GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::Ghost});
    TestTrue(TEXT("Manual Confusion"),GM->CardFrom(A,GM->MatchEpoch,12,uint8(ECardId::Confusion)).bAccepted && GM->Match.ConfusionActionsRemaining==2);
    TestTrue(TEXT("Manual White places Black, count one"),GM->PlaceFrom(B,GM->MatchEpoch,13,{15,15}).bAccepted && GM->Match.Board.At({15,15}).Stone==EStone::Black && GM->Match.ConfusionActionsRemaining==1);
    TestTrue(TEXT("Manual Restock expires effect"),GM->CardFrom(A,GM->MatchEpoch,14,uint8(ECardId::Restock)).bAccepted && GM->Match.ConfusionActionsRemaining==0);
    TestTrue(TEXT("Manual White normal again"),GM->PlaceFrom(B,GM->MatchEpoch,15,{17,17}).bAccepted && GM->Match.Board.At({17,17}).Stone==EStone::White); CheckViews();
    Reset(5211); Open(16);
    TestTrue(TEXT("Seed 5211 natural hands"),GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::TacticalNuke,ECardId::BackToBasics} && GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::Barrier,ECardId::Confusion});
    TestTrue(TEXT("Manual Nuke"),GM->TargetedCardFrom(A,GM->MatchEpoch,16,uint8(ECardId::TacticalNuke),{6,5}).bAccepted);
    TestTrue(TEXT("Manual Barrier"),GM->TargetedCardFrom(B,GM->MatchEpoch,17,uint8(ECardId::Barrier),{9,9}).bAccepted);
    TestTrue(TEXT("Manual filler"),GM->PlaceFrom(A,GM->MatchEpoch,18,{15,15}).bAccepted);
    TestTrue(TEXT("Manual Confusion before Basics"),GM->CardFrom(B,GM->MatchEpoch,19,uint8(ECardId::Confusion)).bAccepted && GM->Match.ConfusionActionsRemaining==2);
    const auto History=GM->Match.Board;
    TestTrue(TEXT("Manual Basics clears effect without reverting history"),GM->CardFrom(A,GM->MatchEpoch,20,uint8(ECardId::BackToBasics)).bAccepted && GM->Match.bCardsDisabled && GM->Match.ConfusionActionsRemaining==0 && GM->Match.Board==History);
    const auto Reward=GM->PlaceFrom(B,GM->MatchEpoch,21,{8,9});
    TestTrue(TEXT("Manual ordinary White placement earns inert Confusion"),Reward.bAccepted && Reward.bBlockingReward && GM->Match.Board.At({8,9}).Stone==EStone::White && GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::Confusion});
    TestTrue(TEXT("Manual Black filler after Basics"),GM->PlaceFrom(A,GM->MatchEpoch,22,{17,17}).bAccepted);
    TestFalse(TEXT("Manual newly earned card visible but disabled on owner's turn"),B->CanPlayCard(uint8(ECardId::Confusion)));
    Reject(B,GM->MatchEpoch,23,ECardId::Confusion,EMatchIntentError::RuleRejected,EActionError::CardsDisabled); CheckViews();
    World->DestroyWorld(false);
    return true;
}
#endif
