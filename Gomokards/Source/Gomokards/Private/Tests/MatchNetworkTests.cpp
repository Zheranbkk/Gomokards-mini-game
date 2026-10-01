#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchGameState.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Presentation/SLocalMatchView.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags NetworkFlags=EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchProjectionTest,"Gomokards.Phase4A.ProjectionSeparation",NetworkFlags)
bool FMatchProjectionTest::RunTest(const FString&)
{
    using namespace Gomokards;
    FMatchState State(771);
    State.Players[0].Id=71; State.Players[1].Id=32;
    State.Players[0].Hand={ECardId::Restock,ECardId::Restock,ECardId::Ghost};
    State.Players[1].Hand={ECardId::Tetris,ECardId::Barrier};
    State.Board.At({2,3})={EStone::Black,false}; State.Board.At({4,5})={EStone::White,false};
    State.Board.At({8,9}).bForbidden=true; State.Board.Barriers.Add({1,1});
    State.CompletedActions=42; State.ConfusionActionsRemaining=2; State.bCardsDisabled=true;
    const auto Before=State;
    const auto Public=MakePublicView(State,{71},EMatchSession::WaitingForPlayers,6,9);
    TestEqual(TEXT("Complete public board"),Public.Cells.Num(),361);
    for (int32 I=0; I<Public.Cells.Num(); ++I)
    {
        TestTrue(TEXT("Ordinary board cell matches"),Public.Cells[I].Stone==static_cast<uint8>(State.Board.Cells[I].Stone) && Public.Cells[I].bForbidden==State.Board.Cells[I].bForbidden);
    }
    TestTrue(TEXT("Public barrier/effect/action metadata"),Public.Barriers==State.Board.Barriers && Public.bCardsDisabled && Public.ConfusionRemaining==2 && Public.CompletedActions==42);
    TestTrue(TEXT("IDs are gameplay IDs, both hand counts public"),Public.Seats[0].PlayerId==71 && Public.Seats[0].HandCount==3 && Public.Seats[1].PlayerId==32 && Public.Seats[1].HandCount==2);
    TestTrue(TEXT("Occupied seats and turn independent of controller index"),Public.Seats[0].bOccupied && !Public.Seats[1].bOccupied && Public.CurrentPlayerId==71);
    const auto A=MakePrivateView(State,71,true,6,9), B=MakePrivateView(State,32,false,6,9);
    TestTrue(TEXT("A gets only its ordered duplicates"),A.Hand==TArray<uint8>{uint8(ECardId::Restock),uint8(ECardId::Restock),uint8(ECardId::Ghost)});
    TestTrue(TEXT("B gets only its hand"),B.Hand==TArray<uint8>{uint8(ECardId::Tetris),uint8(ECardId::Barrier)});
    TestTrue(TEXT("Role/admin are explicit"),A.PlayerId==71 && A.Stone==1 && A.bDevelopmentAdmin && B.PlayerId==32 && B.Stone==2 && !B.bDevelopmentAdmin);
    TestTrue(TEXT("Unassigned view never defaults to either hand"),MakePrivateView(State,999,false,6,9).Hand.IsEmpty());
    TestTrue(TEXT("Projection does not consume RNG or mutate state"),State==Before);
    for (auto Result : {EMatchStatus::Won,EMatchStatus::Draw,EMatchStatus::AwaitingRuleDecision})
    {
        State.Result={Result,Result==EMatchStatus::Won ? EStone::White : EStone::Empty,Result==EMatchStatus::AwaitingRuleDecision ? EDecisionReason::NoLegalAction : EDecisionReason::None};
        const auto Projected=MakePublicView(State,{71,32},EMatchSession::Playing,6,10);
        TestTrue(TEXT("All core terminal results represented without invented rules"),Projected.Result==uint8(Result) && Projected.WinningStone==uint8(State.Result.WinningStone) && Projected.DecisionReason==uint8(State.Result.Decision));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchReflectionTest,"Gomokards.Phase4A.ReflectedPrivacyAndRpcContract",NetworkFlags)
bool FMatchReflectionTest::RunTest(const FString&)
{
    const auto Fields=[this](UStruct* Type,TArray<FName> Expected)
    {
        TArray<FName> Actual;
        for (TFieldIterator<FProperty> It(Type,EFieldIteratorFlags::ExcludeSuper); It; ++It) { Actual.Add(It->GetFName()); }
        TestEqual(TEXT("Exact reflected field count, no extra secrets"),Actual.Num(),Expected.Num());
        for (auto Name : Actual) { TestTrue(*FString::Printf(TEXT("Approved reflected field %s.%s"),*Type->GetName(),*Name.ToString()),Expected.Contains(Name)); }
    };
    Fields(FMatchPublicView::StaticStruct(),{TEXT("Cells"),TEXT("Barriers"),TEXT("Seats"),TEXT("CurrentPlayerId"),TEXT("CompletedActions"),TEXT("Result"),TEXT("WinningStone"),TEXT("DecisionReason"),TEXT("bCardsDisabled"),TEXT("ConfusionRemaining"),TEXT("GhostPhase"),TEXT("GhostPlacementsCompleted"),TEXT("GhostDisplayEndServerTime"),TEXT("Session"),TEXT("Epoch"),TEXT("Revision")});
    Fields(FMatchDisplayCell::StaticStruct(),{TEXT("Stone"),TEXT("bForbidden")});
    Fields(FMatchSeatView::StaticStruct(),{TEXT("PlayerId"),TEXT("Stone"),TEXT("bOccupied"),TEXT("HandCount")});
    Fields(FMatchPrivateView::StaticStruct(),{TEXT("PlayerId"),TEXT("Stone"),TEXT("Hand"),TEXT("bDevelopmentAdmin"),TEXT("Epoch"),TEXT("Revision")});
    Fields(FMatchActionAck::StaticStruct(),{TEXT("Epoch"),TEXT("Revision"),TEXT("bAccepted"),TEXT("Error"),TEXT("RuleError"),TEXT("bBlockingReward")});
    auto* Controller=ALocalMatchPlayerController::StaticClass()->GetDefaultObject<ALocalMatchPlayerController>();
    auto* GS=ALocalMatchGameState::StaticClass()->GetDefaultObject<ALocalMatchGameState>();
    TestTrue(TEXT("Controller actor is owner relevant"),Controller->bOnlyRelevantToOwner);
    const auto Replication=[this](UClass* Type,const TCHAR* Name,const UScriptStruct* Struct,const TArray<FLifetimeProperty>& Props,ELifetimeCondition Condition)
    {
        const auto* Property=FindFProperty<FStructProperty>(Type,Name);
        if (!TestNotNull(TEXT("Compiled reflected snapshot property"),Property)) { return; }
        TestTrue(TEXT("Only explicit DTO is replicated with notification"),Property->Struct==Struct && Property->HasAllPropertyFlags(CPF_Net | CPF_RepNotify));
        const auto* Rep=Props.FindByPredicate([Property](const FLifetimeProperty& Item){return Item.RepIndex==Property->RepIndex;});
        TestTrue(TEXT("Actual lifetime replication condition"),Rep && Rep->Condition==Condition);
        int32 OwnReplicated=0;
        for (TFieldIterator<FProperty> It(Type,EFieldIteratorFlags::ExcludeSuper); It; ++It)
        { if (It->HasAnyPropertyFlags(CPF_Net)) { ++OwnReplicated; TestEqual(TEXT("No other replicated match field"),It->GetName(),FString(Name)); } }
        TestEqual(TEXT("One match projection per actor"),OwnReplicated,1);
    };
    // Net drivers normally initialize RepIndex before asking for lifetime properties.
    Controller->GetClass()->SetUpRuntimeReplicationData();
    GS->GetClass()->SetUpRuntimeReplicationData();
    TArray<FLifetimeProperty> PrivateProps,PublicProps;
    Controller->GetLifetimeReplicatedProps(PrivateProps); GS->GetLifetimeReplicatedProps(PublicProps);
    Replication(Controller->GetClass(),TEXT("PrivateView"),FMatchPrivateView::StaticStruct(),PrivateProps,COND_OwnerOnly);
    Replication(GS->GetClass(),TEXT("PublicView"),FMatchPublicView::StaticStruct(),PublicProps,COND_None);
    auto* Place=Controller->FindFunction(TEXT("ServerPlaceStone"));
    if (TestNotNull(TEXT("Placement RPC exists"),Place))
    {
        TestTrue(TEXT("Reliable server RPC"),Place->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
        Fields(Place,{TEXT("Epoch"),TEXT("ExpectedCompletedActions"),TEXT("X"),TEXT("Y")});
    }
    auto* Ack=Controller->FindFunction(TEXT("ClientActionResult"));
    TestTrue(TEXT("Acknowledgement is owner Client RPC, not multicast"),Ack && Ack->HasAllFunctionFlags(FUNC_Net | FUNC_NetClient | FUNC_NetReliable) && !Ack->HasAnyFunctionFlags(FUNC_NetMulticast));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchNetworkTest,"Gomokards.Phase4A.AuthorityLifecycleAndPresentation",NetworkFlags)
bool FMatchNetworkTest::RunTest(const FString&)
{
    using namespace Gomokards;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("Test world"),World)) { return false; }
    auto* GM=World->SpawnActor<ALocalMatchGameMode>();
    auto* GS=World->SpawnActor<ALocalMatchGameState>();
    auto* A=World->SpawnActor<ALocalMatchPlayerController>();
    auto* B=World->SpawnActor<ALocalMatchPlayerController>();
    auto* Third=World->SpawnActor<ALocalMatchPlayerController>();
    if (!GM || !GS || !A || !B || !Third) { AddError(TEXT("Fixture spawn failed")); World->DestroyWorld(false); return false; }
    World->SetGameState(GS); GM->GameState=GS; GM->bSessionInitialized=true;
    GM->StartWithSeed(771);
    const auto Coherent=[this,GM,GS,A,B]()
    {
        for (auto* PC : {A,B})
        {
            PC->RefreshPresentation();
            TestTrue(TEXT("Both owners/public commit share epoch and revision"),PC->PrivateView.Epoch==GS->PublicView.Epoch && PC->PrivateView.Revision==GS->PublicView.Revision && GS->PublicView.Epoch==GM->MatchEpoch && GS->PublicView.Revision==GM->Revision);
            TestTrue(TEXT("Both views ready after coherent publication"),PC->IsPresentationReady());
        }
    };
    const auto Reject=[this,GM](ALocalMatchPlayerController* PC,uint64 Epoch,uint64 Token,FIntPoint Point,EMatchIntentError Error)
    {
        const auto Before=GM->Match; const auto Revision=GM->Revision; const auto MatchEpoch=GM->MatchEpoch;
        const double GhostDeadline=GM->GhostDeadline,TetrisDeadline=GM->TetrisDeadline;
        const auto Reply=GM->PlaceFrom(PC,Epoch,Token,Point);
        TestTrue(TEXT("Expected rejection without reward"),!Reply.bAccepted && Reply.Error==Error && !Reply.bBlockingReward);
        TestTrue(TEXT("Rejected request preserves complete authoritative state including RNG"),GM->Match==Before);
        TestTrue(TEXT("Rejected request preserves revisions and timer deadlines"),GM->Revision==Revision && GM->MatchEpoch==MatchEpoch && GM->GhostDeadline==GhostDeadline && GM->TetrisDeadline==TetrisDeadline);
    };
    Reject(A,GM->MatchEpoch,0,{0,0},EMatchIntentError::Unassigned);
    GM->Join(A,true);
    TestTrue(TEXT("First seat waits"),GM->Session==EMatchSession::WaitingForPlayers && A->PrivateView.PlayerId==0 && GS->PublicView.Seats[0].bOccupied && !GS->PublicView.Seats[1].bOccupied);
    Reject(A,GM->MatchEpoch,0,{0,0},EMatchIntentError::NotPlaying);
    const auto WaitingEpoch=GM->MatchEpoch;
    GM->Join(B,false); Coherent();
    TestTrue(TEXT("Second seat resets and starts Black"),GM->Session==EMatchSession::Playing && GM->MatchEpoch>WaitingEpoch && GS->PublicView.CurrentPlayerId==0);
    GM->Join(Third,false);
    TestTrue(TEXT("Third unassigned without extra seat"),GM->Assignments.Num()==2 && Third->PrivateView.PlayerId==INDEX_NONE && Third->PrivateView.Hand.IsEmpty());
    Reject(Third,GM->MatchEpoch,0,{0,0},EMatchIntentError::Unassigned);
    Reject(B,GM->MatchEpoch,0,{0,0},EMatchIntentError::RuleRejected);
    TestTrue(TEXT("Assigned Black accepted"),GM->PlaceFrom(A,GM->MatchEpoch,0,{0,0}).bAccepted && GM->Match.Board.At({0,0}).Stone==EStone::Black);
    TestTrue(TEXT("Assigned White accepted"),GM->PlaceFrom(B,GM->MatchEpoch,1,{1,0}).bAccepted && GM->Match.Board.At({1,0}).Stone==EStone::White);
    Reject(A,GM->MatchEpoch,0,{2,0},EMatchIntentError::StaleAction); // Delayed duplicate now that A is current again.
    Reject(A,GM->MatchEpoch-1,2,{2,0},EMatchIntentError::StaleEpoch);
    Reject(A,GM->MatchEpoch,2,{-1,0},EMatchIntentError::RuleRejected);
    Reject(A,GM->MatchEpoch,2,{19,19},EMatchIntentError::RuleRejected);
    Reject(A,GM->MatchEpoch,2,{0,0},EMatchIntentError::RuleRejected);
    GM->Match.Board.At({2,0}).bForbidden=true;
    Reject(A,GM->MatchEpoch,2,{2,0},EMatchIntentError::RuleRejected);
    const auto BeforeRemoteRestart=GM->Match; const auto BeforeRemoteEpoch=GM->MatchEpoch;
    TestTrue(TEXT("Remote restart rejected even if current player"),GM->RestartFrom(B,GM->MatchEpoch).Error==EMatchIntentError::Unauthorized && GM->Match==BeforeRemoteRestart && GM->MatchEpoch==BeforeRemoteEpoch);

    // Reverse both connections: development host is White; authority follows assignment, not process/seat assumptions.
    GM->Assignments[A]=1; GM->Assignments[B]=0;
    TestTrue(TEXT("White host can restart via separate permission"),GM->RestartFrom(A,GM->MatchEpoch).bAccepted);
    Coherent();
    TestTrue(TEXT("Reversed owner identities"),A->PrivateView.PlayerId==1 && A->PrivateView.Stone==2 && B->PrivateView.PlayerId==0 && B->PrivateView.Stone==1);
    Reject(A,GM->MatchEpoch,0,{4,4},EMatchIntentError::RuleRejected);
    TestTrue(TEXT("Remote assigned Black starts"),GM->PlaceFrom(B,GM->MatchEpoch,0,{4,4}).bAccepted);
    TestTrue(TEXT("White host acts only on White turn"),GM->PlaceFrom(A,GM->MatchEpoch,1,{5,4}).bAccepted);

    GM->StartWithSeed(771);
    // Black closes White's two-stone run against board edge; exactly one draw from the unchanged pool.
    GM->Match.Board.At({0,4}).Stone=EStone::White;
    GM->Match.Board.At({1,4}).Stone=EStone::White;
    auto Expected=GM->Match;
    const auto CoreResult=ResolveAction(Expected,FActionRequest::Place(0,{2,4}));
    const auto Reward=GM->PlaceFrom(B,GM->MatchEpoch,0,{2,4}); Coherent();
    TestTrue(TEXT("Reward follows existing core and RNG exactly"),Reward.bAccepted && Reward.bBlockingReward && CoreResult.bBlockingReward && GM->Match==Expected && GM->Match.Players[0].Hand.Num()==1);
    TestTrue(TEXT("Only reward owner receives identity"),B->PrivateView.Hand.Num()==1 && B->PrivateView.Hand[0]==uint8(Expected.Players[0].Hand[0]) && A->PrivateView.Hand.IsEmpty());
    TestTrue(TEXT("Both observers receive hand counts"),GS->PublicView.Seats[0].HandCount==1 && GS->PublicView.Seats[1].HandCount==0);
    const auto SavedEpoch=GM->MatchEpoch;
    TestTrue(TEXT("Admin reset accepted"),GM->RestartFrom(A,SavedEpoch).bAccepted); Coherent();
    TestTrue(TEXT("Reset is clean and epoch advances"),GM->MatchEpoch>SavedEpoch && GM->Match==FMatchState(GM->Match.Random.GetInitialSeed()) && A->PrivateView.Hand.IsEmpty() && B->PrivateView.Hand.IsEmpty());
    Reject(B,SavedEpoch,0,{4,4},EMatchIntentError::StaleEpoch);
    for (int32 X=0; X<4; ++X) { GM->Match.Board.At({X,0}).Stone=EStone::Black; }
    TestTrue(TEXT("Ordinary win accepted"),GM->PlaceFrom(B,GM->MatchEpoch,0,{4,0}).bAccepted); Coherent();
    TestTrue(TEXT("Public winner synchronized"),GS->PublicView.Result==uint8(EMatchStatus::Won) && GS->PublicView.WinningStone==uint8(EStone::Black));
    Reject(B,GM->MatchEpoch,1,{5,0},EMatchIntentError::RuleRejected);
    for (auto Status : {EMatchStatus::Draw,EMatchStatus::AwaitingRuleDecision})
    {
        GM->Match.Result={Status,EStone::Empty,Status==EMatchStatus::AwaitingRuleDecision ? EDecisionReason::NoLegalAction : EDecisionReason::None};
        GM->PublishViews(); Coherent();
        TestTrue(TEXT("Trusted terminal fixture projected to both controllers"),A->GetPublicView().Result==uint8(Status) && B->GetPublicView().Result==uint8(Status));
        Reject(B,GM->MatchEpoch,1,{5,0},EMatchIntentError::RuleRejected);
    }
    GM->RestartFrom(A,GM->MatchEpoch); Coherent();
    // This world intentionally has no AuthGameMode: client read/construction must not need it.
    TestNull(TEXT("Presentation world has no authoritative GameMode lookup"),World->GetAuthGameMode());
    TestTrue(TEXT("Controller reads identity without GameMode"),B->StatusLabel().Contains(TEXT("Black")) && B->CanPlace({3,3}) && !A->CanPlace({3,3}));
    if (TestTrue(TEXT("Slate initialized for construction check"),FSlateApplication::IsInitialized()))
    {
        TSharedPtr<SLocalMatchView> View=SNew(SLocalMatchView).Owner(B);
        TestTrue(TEXT("Client Slate constructed without GameMode and reads public state"),View->GetPublicView().Epoch==GM->MatchEpoch);
        View.Reset();
    }
    // Simulate independent replicated properties and an acknowledgement preceding them.
    const auto InitialPublic=GS->PublicView; const auto InitialPrivate=B->PrivateView;
    auto FutureAck=GM->Acknowledgement(EMatchIntentError::None); FutureAck.Revision+=2;
    B->bPending=true; B->ClientActionResult_Implementation(FutureAck);
    TestTrue(TEXT("Early ack never fabricates board or releases input"),B->bPending && B->GetPublicView().Revision==InitialPublic.Revision && !B->CanPlace({3,3}));
    GS->PublicView.Revision+=3; B->RefreshPresentation();
    TestFalse(TEXT("Public arrives first, coherence gates input"),B->IsPresentationReady());
    B->PrivateView.Revision+=3; B->OnRep_PrivateView();
    TestTrue(TEXT("Coalesced newer revision satisfies ack without every intermediate state"),B->IsPresentationReady() && !B->bPending && !B->PendingAck.IsSet());
    B->PrivateView.Revision++; B->OnRep_PrivateView();
    TestFalse(TEXT("Private arrives first also gates input"),B->IsPresentationReady());
    GS->PublicView.Revision++; B->RefreshPresentation();
    TestTrue(TEXT("Either arrival order recovers"),B->IsPresentationReady());
    B->bPending=true;
    GS->PublicView.Epoch++; B->RefreshPresentation();
    TestTrue(TEXT("Reset does not release an unacknowledged request"),B->bPending);
    TestTrue(TEXT("New epoch clears cached private contents and blocks interaction"),B->GetPrivateView().Hand.IsEmpty() && !B->IsPresentationReady());
    B->PrivateView.Epoch++; B->OnRep_PrivateView();
    B->ClientActionResult_Implementation(FutureAck);
    TestTrue(TEXT("Obsolete ack accounted for, releasing pending input"),!B->PendingAck.IsSet() && !B->bPending);
    GS->PublicView=InitialPublic; B->PrivateView=InitialPrivate;
    GM->MatchEpoch=InitialPublic.Epoch+1; // Advance beyond the synthetic future epoch used above.
    GM->RestartFrom(A,GM->MatchEpoch); Coherent();
    const auto BeforeDisconnect=GM->Match;
    GM->Leave(B);
    TestTrue(TEXT("Disconnect ends session without inventing winner"),GM->Session==EMatchSession::SessionEnded && GM->Match==BeforeDisconnect && !GM->GhostTicker.IsValid() && !GM->TetrisTicker.IsValid());
    TestTrue(TEXT("Remaining owner receives session end and occupancy"),A->GetPublicView().Session==EMatchSession::SessionEnded && !GS->PublicView.Seats[0].bOccupied);
    Reject(A,GM->MatchEpoch,0,{0,0},EMatchIntentError::NotPlaying);
    GM->Join(Third,false);
    TestTrue(TEXT("No reconnect after disconnect"),!GM->Assignments.Contains(Third) && GM->Session==EMatchSession::SessionEnded);
    TestFalse(TEXT("Admin cannot revive disconnected session"),GM->RestartFrom(A,GM->MatchEpoch).bAccepted);
    World->DestroyWorld(false);
    return true;
}
#endif
