#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchGameState.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Cards/CardEffects.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags Flags=EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
template<typename T> bool SameProjection(const T& A,const T& B)
{ return T::StaticStruct()->CompareScriptStruct(&A,&B,0); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBasicCardContractTest,"Gomokards.Phase4B1.CardRpcAndWhitelist",Flags)
bool FBasicCardContractTest::RunTest(const FString&)
{
    using namespace Gomokards;
    auto* RPC=ALocalMatchPlayerController::StaticClass()->FindFunctionByName(TEXT("ServerPlayCard"));
    if (!TestNotNull(TEXT("Card RPC reflected"),RPC)) { return false; }
    TestTrue(TEXT("Reliable owned-controller server RPC"),RPC->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
    TArray<FName> Names;
    for (TFieldIterator<FProperty> It(RPC); It; ++It) { Names.Add(It->GetFName()); }
    TestTrue(TEXT("Only epoch/action token/card byte, no actor/target/random/result payload"),Names==TArray<FName>{TEXT("Epoch"),TEXT("ExpectedCompletedActions"),TEXT("CardId")});
    TestNotNull(TEXT("Card byte safely represents malformed enum input for adapter rejection"),FindFProperty<FByteProperty>(RPC,TEXT("CardId")));
    for (int32 I=0; I<=255; ++I)
    {
        const bool Expected=I==uint8(ECardId::Restock) || I==uint8(ECardId::SwapHands) || I==uint8(ECardId::Steal) || I==uint8(ECardId::Confusion) || I==uint8(ECardId::BackToBasics);
        TestEqual(TEXT("Non-targeted whitelist exactly five, including every malformed byte"),IsNetworkCardEnabled(uint8(I)),Expected);
    }
    TestEqual(TEXT("Authoritative pool remains all ten cards"),GetPlayableCards().Num(),10);
    for (auto Card : {ECardId::TacticalNuke,ECardId::Polarity,ECardId::Confusion,ECardId::Barrier,ECardId::BackToBasics,ECardId::Ghost,ECardId::Tetris})
    { TestNotNull(TEXT("Other existing cards still supported by Core"),FindCardDefinition(Card)); }
    // The Phase 4A reflected privacy tests continue to assert the exact public/private/ack field inventories.
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBasicCardNetworkTest,"Gomokards.Phase4B1.CardAuthorityPrivacyAndCoherence",Flags)
bool FBasicCardNetworkTest::RunTest(const FString&)
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
    GM->StartWithSeed(771); GM->Join(A,true);
    const auto Reject=[this,GM,GS,A,B](ALocalMatchPlayerController* PC,uint64 Epoch,uint64 Token,uint8 Card,EMatchIntentError Error,EActionError Rule=EActionError::None)
    {
        const auto State=GM->Match; const auto Revision=GM->Revision;
        const auto Public=GS->PublicView; const auto PrivateA=A->PrivateView,PrivateB=B->PrivateView;
        const auto DisplayA=A->GetPrivateView(),DisplayB=B->GetPrivateView();
        const double GhostDeadline=GM->GhostDeadline,TetrisDeadline=GM->TetrisDeadline;
        const auto Ack=GM->CardFrom(PC,Epoch,Token,Card);
        TestTrue(TEXT("Expected safe rejection"),!Ack.bAccepted && Ack.Error==Error && Ack.RuleError==uint8(Rule) && !Ack.bBlockingReward);
        TestTrue(TEXT("Every rejection preserves complete state and RNG"),GM->Match==State && GM->Match.Random.GetCurrentSeed()==State.Random.GetCurrentSeed());
        TestTrue(TEXT("Rejection preserves revision, epoch and deadlines"),GM->Revision==Revision && Ack.Epoch==Public.Epoch && GM->GhostDeadline==GhostDeadline && GM->TetrisDeadline==TetrisDeadline);
        TestTrue(TEXT("Rejection preserves public and BOTH private snapshots"),SameProjection(Public,GS->PublicView) && SameProjection(PrivateA,A->PrivateView) && SameProjection(PrivateB,B->PrivateView));
        TestTrue(TEXT("Rejection preserves both displayed own hands"),SameProjection(DisplayA,A->GetPrivateView()) && SameProjection(DisplayB,B->GetPrivateView()));
    };
    Reject(A,GM->MatchEpoch,0,uint8(ECardId::Restock),EMatchIntentError::NotPlaying);
    Reject(Unassigned,GM->MatchEpoch,0,uint8(ECardId::Restock),EMatchIntentError::Unassigned);
    GM->Join(B,false);
    const auto Reset=[GM,A,B]()
    {
        GM->Session=EMatchSession::Playing; GM->Assignments[A]=0; GM->Assignments[B]=1;
        GM->StartWithSeed(771);
    };
    const auto CheckViews=[this,GM,GS,A,B]()
    {
        for (auto* PC : {A,B})
        {
            PC->RefreshPresentation();
            const int32 Id=GM->Assignments[PC];
            const auto Expected=MakePrivateView(GM->Match,Id,GM->DevelopmentAdmins.Contains(PC),GM->MatchEpoch,GM->Revision);
            TestTrue(TEXT("Owner receives only exact authoritative own hand including order"),SameProjection(PC->PrivateView,Expected) && SameProjection(PC->GetPrivateView(),Expected));
            TestTrue(TEXT("No stale own-hand cache after hand transfers"),PC->GetPrivateView().Hand==Expected.Hand);
            TestTrue(TEXT("Public and both private views coherent"),PC->IsPresentationReady() && PC->PrivateView.Epoch==GS->PublicView.Epoch && PC->PrivateView.Revision==GS->PublicView.Revision && GS->PublicView.Revision==GM->Revision);
        }
        for (int32 I=0; I<2; ++I) { TestEqual(TEXT("Public counts match authority"),GS->PublicView.Seats[I].HandCount,GM->Match.Players[I].Hand.Num()); }
    };
    // Every enabled card: possession, turn, assignment, terminal/session/token checks occur on the server.
    for (auto Card : {ECardId::Restock,ECardId::SwapHands,ECardId::Steal})
    {
        Reset();
        Reject(A,GM->MatchEpoch,0,uint8(Card),EMatchIntentError::RuleRejected,EActionError::CardNotOwned);
        GM->Match.Players[0].Hand={Card,ECardId::Ghost}; GM->Match.Players[1].Hand={Card,ECardId::Tetris}; GM->PublishViews();
        Reject(B,GM->MatchEpoch,0,uint8(Card),EMatchIntentError::RuleRejected,EActionError::WrongPlayer);
        Reject(Unassigned,GM->MatchEpoch,0,uint8(Card),EMatchIntentError::Unassigned);
        Reject(A,GM->MatchEpoch-1,0,uint8(Card),EMatchIntentError::StaleEpoch);
        Reject(A,GM->MatchEpoch,9,uint8(Card),EMatchIntentError::StaleAction);
        GM->Match.Result.Status=EMatchStatus::Won; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,uint8(Card),EMatchIntentError::RuleRejected,EActionError::MatchStopped);
        GM->Match.Result.Status=EMatchStatus::InProgress; GM->Match.bCardsDisabled=true; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,uint8(Card),EMatchIntentError::RuleRejected,EActionError::CardsDisabled);
        GM->Match.bCardsDisabled=false; GM->Session=EMatchSession::SessionEnded; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,uint8(Card),EMatchIntentError::NotPlaying);
    }
    Reset();
    for (auto Card : {ECardId::TacticalNuke,ECardId::Polarity,ECardId::Barrier,ECardId::Ghost,ECardId::Tetris,
        ECardId::Invalid,ECardId::FastDuel,ECardId::Undo,ECardId::Joker,static_cast<ECardId>(255)})
    {
        GM->Match.Players[0].Hand={Card}; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,uint8(Card),EMatchIntentError::CardNotNetworkEnabled);
        TestFalse(TEXT("Cards outside the non-targeted whitelist cannot use basic input"),A->CanPlayCard(uint8(Card)));
        auto Ack=GM->CardFrom(A,GM->MatchEpoch,0,uint8(Card)); A->ClientActionResult_Implementation(Ack);
        TestTrue(TEXT("Safe explanatory rejection feedback"),A->GetFeedback().Contains(TEXT("later phase")));
    }
    // Deterministic parity for both seat mappings, including White development host.
    for (bool bReverse : {false,true})
    for (auto Card : {ECardId::Restock,ECardId::SwapHands,ECardId::Steal})
    {
        Reset();
        if (bReverse) { GM->Assignments[A]=1; GM->Assignments[B]=0; }
        auto* Actor=bReverse ? B : A; auto* Other=bReverse ? A : B;
        GM->Match.Players[0].Hand={Card,ECardId::Confusion,ECardId::Restock};
        GM->Match.Players[1].Hand={ECardId::Tetris,ECardId::Barrier,ECardId::Barrier,ECardId::Ghost};
        GM->PublishViews(); CheckViews();
        const auto OldPublic=GS->PublicView, OldActorPublic=Actor->DisplayPublic;
        const auto OldActorPrivate=Actor->PrivateView,OldOtherPrivate=Other->PrivateView;
        const auto State=GM->Match; auto Expected=State;
        const auto Core=ResolveAction(Expected,FActionRequest::Play(0,Card));
        TestTrue(TEXT("Current owner can click eligible card; opposite owner cannot"),Actor->CanPlayCard(uint8(Card)) && !Other->CanPlayCard(uint8(Card)));
        Actor->bPending=true;
        TestFalse(TEXT("One outstanding request gates card clicks"),Actor->CanPlayCard(uint8(Card)));
        const auto Ack=GM->CardFrom(Actor,GM->MatchEpoch,0,uint8(Card));
        TestTrue(TEXT("Network commit exactly equals Core including hands/order/RNG/effects/turn"),Core.IsAccepted() && Ack.bAccepted && !Ack.bBlockingReward && GM->Match==Expected);
        TestTrue(TEXT("Exactly one revision/action committed"),Ack.Revision==OldPublic.Revision+1 && GM->Match.CompletedActions==1 && GM->Match.CurrentPlayerIndex==1);
        if (Card==ECardId::Restock)
        {
            TestEqual(TEXT("Restock consumes itself then adds exactly two"),GM->Match.Players[0].Hand.Num(),State.Players[0].Hand.Num()+1);
            TestTrue(TEXT("Restock RNG advances; opponent hand unchanged"),GM->Match.Random.GetCurrentSeed()!=State.Random.GetCurrentSeed() && Other->PrivateView.Hand==OldOtherPrivate.Hand);
        }
        else if (Card==ECardId::SwapHands)
        {
            auto Remaining=State.Players[0].Hand; Remaining.RemoveAt(0);
            TestTrue(TEXT("Swap consumes first then exchanges hands"),GM->Match.Players[0].Hand==State.Players[1].Hand && GM->Match.Players[1].Hand==Remaining);
            TestEqual(TEXT("Swap never uses RNG"),GM->Match.Random.GetCurrentSeed(),State.Random.GetCurrentSeed());
        }
        else
        {
            TestTrue(TEXT("Steal consumes itself, appends stolen card; victim loses one"),GM->Match.Players[0].Hand.Num()==State.Players[0].Hand.Num() && GM->Match.Players[1].Hand.Num()==State.Players[1].Hand.Num()-1);
            TestTrue(TEXT("Accepted nonempty Steal advances RNG"),GM->Match.Random.GetCurrentSeed()!=State.Random.GetCurrentSeed());
        }
        CheckViews();
        const auto NewPublic=GS->PublicView; const auto NewPrivate=Actor->PrivateView;
        // Replay delivery orders on the receiving view; no socket/PIE gameplay is simulated here.
        GS->PublicView=OldPublic; Actor->PrivateView=OldActorPrivate;
        Actor->DisplayPublic=OldActorPublic; Actor->DisplayPrivate=OldActorPrivate; Actor->PendingAck.Reset(); Actor->bPending=true;
        Actor->ClientActionResult_Implementation(Ack);
        TestTrue(TEXT("Card ack before projection cannot fabricate hand or board"),Actor->bPending && SameProjection(Actor->GetPrivateView(),OldActorPrivate) && SameProjection(Actor->GetPublicView(),OldActorPublic));
        Actor->PrivateView=NewPrivate; Actor->OnRep_PrivateView();
        TestFalse(TEXT("Private first disables input until public catches up"),Actor->IsPresentationReady());
        GS->PublicView=NewPublic; Actor->RefreshPresentation();
        TestTrue(TEXT("Committed hand replaces cache, no old extra hand retained"),Actor->IsPresentationReady() && !Actor->bPending && SameProjection(Actor->GetPrivateView(),NewPrivate));
        Actor->PrivateView=OldActorPrivate; Actor->DisplayPrivate=OldActorPrivate; Actor->DisplayPublic=OldPublic; Actor->bPending=true;
        Actor->ClientActionResult_Implementation(Ack);
        TestFalse(TEXT("Public first also disables input"),Actor->IsPresentationReady());
        Actor->PrivateView=NewPrivate; Actor->OnRep_PrivateView(); CheckViews();
        TestTrue(TEXT("Private catch-up completes ack"),!Actor->bPending && SameProjection(Actor->GetPrivateView(),NewPrivate));
        TestTrue(TEXT("Ordinary placement after card still works"),GM->PlaceFrom(Other,GM->MatchEpoch,1,{10,10}).bAccepted);
        Reject(Actor,GM->MatchEpoch,0,uint8(Card),EMatchIntentError::StaleAction);
    }
    Reset();
    GM->Match.Players[0].Hand={ECardId::Steal}; GM->PublishViews();
    const int32 SeedBefore=GM->Match.Random.GetCurrentSeed();
    TestTrue(TEXT("Empty victim Steal remains accepted and consumed"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Steal)).bAccepted && GM->Match.Players[0].Hand.IsEmpty() && GM->Match.CompletedActions==1);
    TestEqual(TEXT("Empty victim Steal does not roll RNG"),GM->Match.Random.GetCurrentSeed(),SeedBefore); CheckViews();

    // Search a bounded deterministic seed fixture, without changing the production pool or injecting a runtime cheat.
    int32 LaterSeed=INDEX_NONE;
    for (int32 Seed=0; Seed<100 && LaterSeed==INDEX_NONE; ++Seed)
    {
        FRandomStream Random(Seed);
        if (!IsNetworkCardEnabled(uint8(DrawCard(Random)))) { LaterSeed=Seed; }
    }
    TestTrue(TEXT("Full pool can draw a later-phase card"),LaterSeed!=INDEX_NONE);
    Reset(); GM->StartWithSeed(LaterSeed);
    GM->Match.Board.At({0,4}).Stone=EStone::White; GM->Match.Board.At({1,4}).Stone=EStone::White;
    TestTrue(TEXT("Blocking reward still draws from full pool"),GM->PlaceFrom(A,GM->MatchEpoch,0,{2,4}).bBlockingReward);
    CheckViews();
    const ECardId Later=GM->Match.Players[0].Hand[0];
    TestTrue(TEXT("Non-basic card exists privately, count public, basic input disabled"),!IsNetworkCardEnabled(uint8(Later)) && A->PrivateView.Hand.Contains(uint8(Later)) && GS->PublicView.Seats[0].HandCount==1 && B->PrivateView.Hand.IsEmpty() && !A->CanPlayCard(uint8(Later)));
    GM->PlaceFrom(B,GM->MatchEpoch,1,{10,10});
    Reject(A,GM->MatchEpoch,2,uint8(Later),EMatchIntentError::CardNotNetworkEnabled);
    Reset(); GM->StartWithSeed(LaterSeed); GM->Match.Players[0].Hand={ECardId::Restock}; GM->PublishViews();
    TestTrue(TEXT("Restock also retains full pool"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Restock)).bAccepted && GM->Match.Players[0].Hand.Contains(Later)); CheckViews();
    TestTrue(TEXT("Opponent receives none of actor's new draws"),B->PrivateView.Hand.IsEmpty() && A->PrivateView.Hand.Num()==2 && GS->PublicView.Seats[0].HandCount==2);
    GM->PlaceFrom(B,GM->MatchEpoch,1,{10,10});
    Reject(A,GM->MatchEpoch,2,uint8(Later),EMatchIntentError::CardNotNetworkEnabled);

    // README manual recipe: existing server Seed option, real placements/rewards, no hand injection.
    Reset(); GM->StartWithSeed(5751);
    const TArray<FIntPoint> Opening={{5,5},{6,5},{0,0},{7,5},{8,5},{1,0},{9,9},{10,9},
        {12,12},{11,9},{12,9},{5,12},{6,12},{0,18},{7,12},{8,12}};
    for (int32 I=0; I<Opening.Num(); ++I)
    {
        const auto Ack=GM->PlaceFrom(I%2==0 ? A : B,GM->MatchEpoch,GM->Match.CompletedActions,Opening[I]);
        TestTrue(TEXT("Deterministic manual opening placement accepted"),Ack.bAccepted);
        TestEqual(TEXT("Manual opening rewards only at four documented placements"),Ack.bBlockingReward,I==4 || I==5 || I==10 || I==15);
    }
    TestTrue(TEXT("Manual fixture naturally earns Black Restock/Steal, White Swap/Tetris"),
        GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::Restock,ECardId::Steal} && GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::SwapHands,ECardId::Tetris});
    TestTrue(TEXT("Manual Black Restock"),GM->CardFrom(A,GM->MatchEpoch,16,uint8(ECardId::Restock)).bAccepted);
    TestTrue(TEXT("Manual Restock exact resulting own hand"),GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::Steal,ECardId::Ghost,ECardId::Steal}); CheckViews();
    TestTrue(TEXT("Manual White Swap"),GM->CardFrom(B,GM->MatchEpoch,17,uint8(ECardId::SwapHands)).bAccepted);
    TestTrue(TEXT("Manual swapped hands"),GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::Tetris} && GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::Steal,ECardId::Ghost,ECardId::Steal}); CheckViews();
    Reject(A,GM->MatchEpoch,18,uint8(ECardId::Tetris),EMatchIntentError::CardNotNetworkEnabled);
    TestTrue(TEXT("Manual Black ordinary move"),GM->PlaceFrom(A,GM->MatchEpoch,18,{17,17}).bAccepted);
    TestTrue(TEXT("Manual White Steal"),GM->CardFrom(B,GM->MatchEpoch,19,uint8(ECardId::Steal)).bAccepted);
    TestTrue(TEXT("Manual stolen card delivered only as resulting owner hand"),GM->Match.Players[0].Hand.IsEmpty() && GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::Ghost,ECardId::Steal,ECardId::Tetris}); CheckViews();

    // The old ordinary-only full-board guard must not terminate a now-playable card turn.
    Reset();
    for (auto& Cell : GM->Match.Board.Cells) { Cell.bForbidden=true; }
    GM->Match.Players[0].Hand={ECardId::Restock}; GM->Match.Players[1].Hand={ECardId::SwapHands};
    GM->PublishViews();
    TestTrue(TEXT("No empty legal point but network card keeps session playing"),GM->Session==EMatchSession::Playing);
    TestTrue(TEXT("Card on full/forbidden board resolves through Core"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Restock)).bAccepted && GM->Session==EMatchSession::Playing);
    GM->Match.Players[1].Hand={ECardId::Ghost}; GM->PublishViews();
    TestTrue(TEXT("Only unexposed actions ends runtime slice without core draw"),GM->Session==EMatchSession::SessionEnded && GM->Match.Result.Status==EMatchStatus::InProgress);
    World->DestroyWorld(false);
    return true;
}
#endif
