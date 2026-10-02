#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchGameState.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Presentation/MatchPresentation.h"
#include "Presentation/SLocalMatchView.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags TargetedCardNetworkTestsFlags=EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
template<typename T> bool SameTargetProjection(const T& A,const T& B)
{ return T::StaticStruct()->CompareScriptStruct(&A,&B,0); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetedCardContractTest,"Gomokards.Phase4B2.TargetRpcAndGeometry",TargetedCardNetworkTestsFlags)
bool FTargetedCardContractTest::RunTest(const FString&)
{
    using namespace Gomokards;
    auto* RPC=ALocalMatchPlayerController::StaticClass()->FindFunctionByName(TEXT("ServerPlayTargetedCard"));
    if (!TestNotNull(TEXT("Dedicated target RPC"),RPC)) { return false; }
    TestTrue(TEXT("Reliable server RPC"),RPC->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
    TArray<FName> Names;
    for (TFieldIterator<FProperty> It(RPC); It; ++It) { Names.Add(It->GetFName()); }
    TestTrue(TEXT("Only epoch/token/card/normalized integer target; no identity/domain/result"),Names==TArray<FName>{TEXT("Epoch"),TEXT("ExpectedCompletedActions"),TEXT("CardId"),TEXT("TargetX"),TEXT("TargetY")});
    TestNotNull(TEXT("Target X is normalized integer, not screen coordinate"),FindFProperty<FIntProperty>(RPC,TEXT("TargetX")));
    TestNotNull(TEXT("Target Y is normalized integer"),FindFProperty<FIntProperty>(RPC,TEXT("TargetY")));
    for (int32 I=0; I<=255; ++I)
    {
        const bool Expected=I==uint8(ECardId::TacticalNuke) || I==uint8(ECardId::Polarity) || I==uint8(ECardId::Barrier);
        TestEqual(TEXT("Exact targeted whitelist for every byte"),IsTargetedNetworkCardEnabled(uint8(I)),Expected);
        TestFalse(TEXT("Two RPC whitelists never overlap"),IsTargetedNetworkCardEnabled(uint8(I)) && IsNetworkCardEnabled(uint8(I)));
    }
    for (int32 Y=0; Y<19; ++Y)
    for (int32 X=0; X<19; ++X)
    {
        const FIntPoint Anchor(X,Y);
        TestTrue(TEXT("Nuke intersection target including final row/column"),FBoardLayout::TargetAt(FBoardLayout::Center(Anchor),ECardId::TacticalNuke).Get(FIntPoint(-1,-1))==Anchor);
        const auto Polarity=FBoardLayout::TargetAt(FBoardLayout::Center(Anchor),ECardId::Polarity);
        const auto Barrier=FBoardLayout::TargetAt(FBoardLayout::Center(Anchor)+FVector2D(FBoardLayout::CellSize*.5f),ECardId::Barrier);
        TestEqual(TEXT("Polarity anchor domain"),Polarity.IsSet(),FBoard::ContainsAnchor(Anchor));
        TestEqual(TEXT("Barrier cell-center domain"),Barrier.IsSet(),FBoard::ContainsAnchor(Anchor));
        if (FBoard::ContainsAnchor(Anchor))
        {
            TestTrue(TEXT("Exact normalized anchor for both targeted geometries"),Polarity.GetValue()==Anchor && Barrier.GetValue()==Anchor);
            const auto Cross=FBoardLayout::BarrierCross(Anchor);
            const auto Center=FBoardLayout::Center(Anchor)+FVector2D(15);
            TestTrue(TEXT("Preview cross is centered between intersections"),((Cross[0]+Cross[1])*.5).Equals(Center) && ((Cross[2]+Cross[3])*.5).Equals(Center));
        }
    }
    for (auto Card : {ECardId::TacticalNuke,ECardId::Polarity,ECardId::Barrier})
    for (auto Point : {FVector2D(-1,30),FVector2D(30,-1),FVector2D(570,30),FVector2D(30,570)})
    { TestFalse(TEXT("Outside never clamps into a target"),FBoardLayout::TargetAt(Point,Card).IsSet()); }
    TestFalse(TEXT("Barrier excludes outer margin before first intersection"),FBoardLayout::TargetAt({14,30},ECardId::Barrier).IsSet());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetedCardNetworkTest,"Gomokards.Phase4B2.AuthorityOutcomesAndLocalTargeting",TargetedCardNetworkTestsFlags)
bool FTargetedCardNetworkTest::RunTest(const FString&)
{
    using namespace Gomokards;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("World"),World)) { return false; }
    // A real authority lookup lets the production controller/RPC wrappers exercise click routing.
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    World->SetGameMode(FURL(nullptr,TEXT("/Game/Maps/LocalMatch?game=/Script/Gomokards.LocalMatchGameMode"),TRAVEL_Absolute));
    auto* GM=World->GetAuthGameMode<ALocalMatchGameMode>();
    auto* GS=World->SpawnActor<ALocalMatchGameState>();
    auto* A=World->SpawnActor<ALocalMatchPlayerController>();
    auto* B=World->SpawnActor<ALocalMatchPlayerController>();
    auto* Unassigned=World->SpawnActor<ALocalMatchPlayerController>();
    if (!GM || !GS || !A || !B || !Unassigned) { AddError(TEXT("Fixture spawn failed")); World->DestroyWorld(false); return false; }
    World->SetGameState(GS); GM->GameState=GS; GM->bSessionInitialized=true;
    GM->StartWithSeed(1294); GM->Join(A,true);
    const auto Reject=[this,GM,GS,A,B](ALocalMatchPlayerController* PC,uint64 Epoch,uint64 Token,uint8 Card,FIntPoint Target,EMatchIntentError Error,EActionError Rule=EActionError::None,bool bBasicRpc=false)
    {
        const auto Before=GM->Match; const auto Revision=GM->Revision;
        const auto Public=GS->PublicView; const auto PrivateA=A->PrivateView,PrivateB=B->PrivateView;
        const auto Ack=bBasicRpc ? GM->CardFrom(PC,Epoch,Token,Card) : GM->TargetedCardFrom(PC,Epoch,Token,Card,Target);
        TestTrue(TEXT("Target request safely rejected with expected reason"),!Ack.bAccepted && Ack.Error==Error && Ack.RuleError==uint8(Rule) && !Ack.bBlockingReward);
        TestTrue(TEXT("Complete state/RNG and revision unchanged"),GM->Match==Before && GM->Revision==Revision);
        TestTrue(TEXT("All transport snapshots unchanged on rejection"),SameTargetProjection(Public,GS->PublicView) && SameTargetProjection(PrivateA,A->PrivateView) && SameTargetProjection(PrivateB,B->PrivateView));
    };
    const auto Cards={ECardId::TacticalNuke,ECardId::Polarity,ECardId::Barrier};
    for (auto Card : Cards) { Reject(A,GM->MatchEpoch,0,uint8(Card),{2,2},EMatchIntentError::NotPlaying); }
    GM->Join(B,false);
    const auto Reset=[GM,A,B]()
    {
        GM->Session=EMatchSession::Playing; GM->Assignments[A]=0; GM->Assignments[B]=1;
        GM->StartWithSeed(1294);
    };
    const auto CheckViews=[this,GM,GS,A,B]()
    {
        for (auto* PC : {A,B})
        {
            PC->RefreshPresentation();
            const auto Expected=MakePrivateView(GM->Match,GM->Assignments[PC],GM->DevelopmentAdmins.Contains(PC),GM->MatchEpoch,GM->Revision);
            TestTrue(TEXT("Both private revisions coherent and exact own hands replace caches"),PC->IsPresentationReady() && SameTargetProjection(PC->PrivateView,Expected) && SameTargetProjection(PC->GetPrivateView(),Expected) && PC->PrivateView.Revision==GS->PublicView.Revision);
            TestTrue(TEXT("Both clients read same public board/result snapshot"),SameTargetProjection(PC->GetPublicView(),GS->PublicView));
        }
        for (int32 I=0; I<361; ++I)
        { TestTrue(TEXT("Projected stone/forbidden matches authority"),GS->PublicView.Cells[I].Stone==uint8(GM->Match.Board.Cells[I].Stone) && GS->PublicView.Cells[I].bForbidden==GM->Match.Board.Cells[I].bForbidden); }
        TestTrue(TEXT("Barrier anchors match authority"),GS->PublicView.Barriers==GM->Match.Board.Barriers);
        for (int32 I=0; I<2; ++I) { TestEqual(TEXT("Public count matches authoritative own hand"),GS->PublicView.Seats[I].HandCount,GM->Match.Players[I].Hand.Num()); }
    };
    for (auto Card : Cards)
    {
        Reset();
        Reject(A,GM->MatchEpoch,0,uint8(Card),{2,2},EMatchIntentError::RuleRejected,EActionError::CardNotOwned);
        GM->Match.Players[0].Hand={Card}; GM->Match.Players[1].Hand={Card,ECardId::Ghost}; GM->PublishViews();
        Reject(Unassigned,GM->MatchEpoch,0,uint8(Card),{2,2},EMatchIntentError::Unassigned);
        Reject(B,GM->MatchEpoch,0,uint8(Card),{2,2},EMatchIntentError::RuleRejected,EActionError::WrongPlayer);
        Reject(A,GM->MatchEpoch-1,0,uint8(Card),{2,2},EMatchIntentError::StaleEpoch);
        Reject(A,GM->MatchEpoch,1,uint8(Card),{2,2},EMatchIntentError::StaleAction);
        for (auto Target : {FIntPoint(-1,0),FIntPoint(0,-1),FIntPoint(19,0),FIntPoint(0,19)})
        { Reject(A,GM->MatchEpoch,0,uint8(Card),Target,EMatchIntentError::RuleRejected,EActionError::InvalidTarget); }
        if (Card!=ECardId::TacticalNuke)
        { for (auto Target : {FIntPoint(18,0),FIntPoint(0,18),FIntPoint(18,18)}) { Reject(A,GM->MatchEpoch,0,uint8(Card),Target,EMatchIntentError::RuleRejected,EActionError::InvalidTarget); } }
        Reject(A,GM->MatchEpoch,0,uint8(Card),{2,2},EMatchIntentError::CardNotNetworkEnabled,EActionError::None,true);
        GM->Match.Result.Status=EMatchStatus::Won; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,uint8(Card),{2,2},EMatchIntentError::RuleRejected,EActionError::MatchStopped);
        GM->Match.Result.Status=EMatchStatus::InProgress; GM->Session=EMatchSession::SessionEnded; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,uint8(Card),{2,2},EMatchIntentError::NotPlaying);
    }
    Reset();
    for (int32 I=0; I<=255; ++I)
    {
        if (IsTargetedNetworkCardEnabled(uint8(I))) { continue; }
        GM->Match.Players[0].Hand={static_cast<ECardId>(I)}; GM->PublishViews();
        Reject(A,GM->MatchEpoch,0,uint8(I),{2,2},EMatchIntentError::CardNotNetworkEnabled);
        if (!IsNetworkCardEnabled(uint8(I))) { Reject(A,GM->MatchEpoch,0,uint8(I),{2,2},EMatchIntentError::CardNotNetworkEnabled,EActionError::None,true); }
    }
    // Network parity, edge targets and existing Confusion lifecycle, including reversed controller seats.
    for (bool bReverse : {false,true})
    for (auto Card : Cards)
    {
        Reset(); if (bReverse) { GM->Assignments[A]=1; GM->Assignments[B]=0; }
        auto* Actor=bReverse ? B : A;
        GM->Match.Players[0].Hand={Card,ECardId::Restock}; GM->Match.Players[1].Hand={ECardId::Ghost};
        GM->Match.ConfusionActionsRemaining=2;
        GM->Match.Board.At({17,17}).Stone=EStone::Black; GM->Match.Board.At({18,17}).Stone=EStone::White;
        GM->Match.Board.At({18,18}).Stone=EStone::Black;
        GM->PublishViews(); const auto Before=GM->Match; const auto Revision=GM->Revision;
        const FIntPoint Target=Card==ECardId::TacticalNuke ? FIntPoint(18,18) : FIntPoint(17,17);
        auto Expected=Before; const auto Core=ResolveAction(Expected,FActionRequest::Play(0,Card,Target));
        const auto Ack=GM->TargetedCardFrom(Actor,GM->MatchEpoch,0,uint8(Card),Target);
        TestTrue(TEXT("Accepted target exactly matches existing Core"),Core.IsAccepted() && Ack.bAccepted && GM->Match==Expected);
        TestTrue(TEXT("One revision/action, normal turn transfer, existing Confusion consumed"),GM->Revision==Revision+1 && GM->Match.CompletedActions==1 && GM->Match.CurrentPlayerIndex==1 && GM->Match.ConfusionActionsRemaining==1);
        TestEqual(TEXT("Targeted effects consume no RNG"),GM->Match.Random.GetCurrentSeed(),Before.Random.GetCurrentSeed()); CheckViews();
        if (Card==ECardId::TacticalNuke) { TestTrue(TEXT("Nuke removes stone and forbids"),GM->Match.Board.At(Target)==FCell{EStone::Empty,true}); }
        else if (Card==ECardId::Polarity) { TestTrue(TEXT("Polarity ordinary fixture remains nonterminal"),GM->Match.Result.Status==EMatchStatus::InProgress); }
        else { auto Board=GM->Match.Board; Board.Barriers=Before.Board.Barriers; TestTrue(TEXT("Barrier never alters stones/occupancy"),Board==Before.Board); }
    }
    // Reuse accepted Core outcome fixtures at the network boundary; no new result rules.
    for (auto Winner : {EStone::Black,EStone::White,EStone::Empty})
    {
        Reset(); GM->Match.Players[0].Hand={ECardId::Polarity,ECardId::Restock};
        const EStone First=Winner==EStone::Empty ? EStone::Black : Winner;
        for (int32 X=0; X<5; ++X)
        {
            GM->Match.Board.At({X,2}).Stone=X<3 ? First : OppositeStone(First);
            if (Winner==EStone::Empty) { GM->Match.Board.At({X,3}).Stone=X<3 ? EStone::White : EStone::Black; }
        }
        GM->PublishViews();
        auto Expected=GM->Match; ResolveAction(Expected,FActionRequest::Play(0,ECardId::Polarity,FIntPoint(3,2)));
        TestTrue(TEXT("Polarity network terminal commit equals Core"),GM->TargetedCardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Polarity),{3,2}).bAccepted && GM->Match==Expected); CheckViews();
        TestTrue(TEXT("Win/draw publicly represented on both clients"),GS->PublicView.WinningStone==uint8(Winner) && GS->PublicView.Result==uint8(Winner==EStone::Empty ? EMatchStatus::Draw : EMatchStatus::Won));
        Reject(A,GM->MatchEpoch,1,uint8(ECardId::Polarity),{3,2},EMatchIntentError::RuleRejected,EActionError::MatchStopped);
        const auto Before=GM->Match;
        TestFalse(TEXT("Placement after Polarity terminal rejects"),GM->PlaceFrom(A,GM->MatchEpoch,1,{10,10}).bAccepted);
        TestTrue(TEXT("Terminal placement atomic"),GM->Match==Before);
    }
    for (auto Card : {ECardId::TacticalNuke,ECardId::Barrier})
    {
        Reset(); GM->Match.Players[0].Hand={Card};
        for (int32 X=0; X<5; ++X) { GM->Match.Board.At({X,0}).Stone=EStone::Black; }
        GM->PublishViews();
        TestTrue(TEXT("Nuke/Barrier do not invent global evaluation of unrelated fixture line"),GM->TargetedCardFrom(A,GM->MatchEpoch,0,uint8(Card),{10,10}).bAccepted && GM->Match.Result.Status==EMatchStatus::InProgress);
    }
    Reset(); GM->Match.Players[0].Hand={ECardId::Barrier}; GM->Match.Players[1].Hand={ECardId::Barrier};
    for (int32 X=0; X<4; ++X) { GM->Match.Board.At({X,1}).Stone=EStone::Black; }
    GM->PublishViews();
    const auto CenterTarget=FBoardLayout::TargetAt(FBoardLayout::Center({1,1})+FVector2D(15),ECardId::Barrier).GetValue();
    TestTrue(TEXT("Cell-center normalized anchor resolves"),GM->TargetedCardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Barrier),CenterTarget).bAccepted);
    TestTrue(TEXT("Duplicate Barrier remains legal and consumed"),GM->TargetedCardFrom(B,GM->MatchEpoch,1,uint8(ECardId::Barrier),CenterTarget).bAccepted && GM->Match.Board.Barriers.Num()==1 && GM->Match.Players[1].Hand.IsEmpty());
    TestTrue(TEXT("Subsequent fifth stone accepted, Barrier prevents connected five"),GM->PlaceFrom(A,GM->MatchEpoch,2,{4,1}).bAccepted && GM->Match.Result.Status==EMatchStatus::InProgress); CheckViews();

    // Selection/cancel and board-click exclusivity exercise real controller RPC wrappers and Slate handlers.
    // This transient world has not begun play; allow local ProcessEvent dispatch for this scoped test only.
    {
    TGuardValue<bool> AllowLocalScript(GAllowActorScriptExecutionInEditor,true);
    Reset(); GM->Match.Players[0].Hand={ECardId::TacticalNuke,ECardId::Polarity,ECardId::Barrier}; GM->PublishViews();
    const auto BeforeSelection=GM->Match; const auto SelectionRevision=GM->Revision;
    TSharedPtr<SLocalMatchView> View=SNew(SLocalMatchView).Owner(A);
    A->ToggleTargeting(uint8(ECardId::TacticalNuke));
    TestTrue(TEXT("Selection is local and not pending"),A->SelectedTargetedCard==uint8(ECardId::TacticalNuke) && !A->bPending);
    A->ToggleTargeting(uint8(ECardId::TacticalNuke)); TestEqual(TEXT("Reselect cancels"),A->SelectedTargetedCard,uint8(0));
    A->ToggleTargeting(uint8(ECardId::Polarity));
    View->OnPreviewKeyDown(FGeometry(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
    TestEqual(TEXT("Escape cancels"),A->SelectedTargetedCard,uint8(0));
    A->ToggleTargeting(uint8(ECardId::Barrier));
    View->OnMouseButtonDown(FGeometry(),FPointerEvent(0,FVector2D::ZeroVector,FVector2D::ZeroVector,TSet<FKey>{EKeys::RightMouseButton},EKeys::RightMouseButton,0,FModifierKeysState()));
    TestTrue(TEXT("Right-click cancellation no action/revision/RNG"),A->SelectedTargetedCard==0 && !A->bPending && GM->Match==BeforeSelection && GM->Revision==SelectionRevision);
    View->BoardClick({3,3});
    TestTrue(TEXT("After cancel exactly one ordinary placement"),GM->Match.CompletedActions==1 && GM->Match.Board.At({3,3}).Stone==EStone::Black && GM->Match.Players[0].Hand.Num()==3);
    GM->PlaceFrom(B,GM->MatchEpoch,1,{4,3});
    A->ToggleTargeting(uint8(ECardId::TacticalNuke)); View->BoardClick({3,3});
    TestTrue(TEXT("Target click sends card only, not a second placement"),GM->Match.CompletedActions==3 && GM->Match.Board.At({3,3})==FCell{EStone::Empty,true} && GM->Match.Players[0].Hand.Num()==2 && A->SelectedTargetedCard==0 && !A->bPending);
    GM->PlaceFrom(B,GM->MatchEpoch,3,{5,3});
    A->ToggleTargeting(uint8(ECardId::Polarity)); const auto BeforeInvalid=GM->Match; const auto InvalidRevision=GM->Revision;
    View->BoardClick({18,18});
    TestTrue(TEXT("Invalid target rejection clears pending, preserves match, allows reselect"),GM->Match==BeforeInvalid && GM->Revision==InvalidRevision && A->SelectedTargetedCard==0 && !A->bPending && A->CanTargetCard(uint8(ECardId::Polarity)) && !A->GetFeedback().IsEmpty());
    A->ToggleTargeting(uint8(ECardId::Polarity)); GM->RestartFrom(A,GM->MatchEpoch);
    TestEqual(TEXT("Epoch reset cancels stale local selection"),A->SelectedTargetedCard,uint8(0)); View.Reset();
    }

    // Short deterministic manual recipe: eleven ordinary placements, no hand injection.
    Reset(); GM->StartWithSeed(1294);
    const TArray<FIntPoint> Opening={{5,5},{6,5},{0,0},{7,5},{8,5},{1,0},{9,9},{10,9},{12,12},{11,9},{12,9}};
    for (int32 I=0; I<Opening.Num(); ++I)
    {
        const auto Ack=GM->PlaceFrom(I%2==0 ? A : B,GM->MatchEpoch,GM->Match.CompletedActions,Opening[I]);
        TestTrue(TEXT("Manual opening accepted"),Ack.bAccepted);
        TestEqual(TEXT("Exactly three natural blocking rewards"),Ack.bBlockingReward,I==4 || I==5 || I==10);
    }
    TestTrue(TEXT("Seed naturally earns Nuke/Barrier for Black and Polarity for White"),GM->Match.Players[0].Hand==TArray<ECardId>{ECardId::TacticalNuke,ECardId::Barrier} && GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::Polarity});
    TestTrue(TEXT("Manual White Polarity"),GM->TargetedCardFrom(B,GM->MatchEpoch,11,uint8(ECardId::Polarity),{5,5}).bAccepted);
    TestTrue(TEXT("Manual exact recolor"),GM->Match.Board.At({5,5}).Stone==EStone::White && GM->Match.Board.At({6,5}).Stone==EStone::Black);
    TestTrue(TEXT("Manual Black Nuke removes recolored stone"),GM->TargetedCardFrom(A,GM->MatchEpoch,12,uint8(ECardId::TacticalNuke),{6,5}).bAccepted && GM->Match.Board.At({6,5})==FCell{EStone::Empty,true});
    const auto AfterNuke=GM->Match;
    TestTrue(TEXT("Manual forbidden placement rejects unchanged"),!GM->PlaceFrom(B,GM->MatchEpoch,13,{6,5}).bAccepted && GM->Match==AfterNuke);
    TestTrue(TEXT("Manual White filler"),GM->PlaceFrom(B,GM->MatchEpoch,13,{15,15}).bAccepted);
    const auto BeforeBarrier=GM->Match.Board.Cells;
    TestTrue(TEXT("Manual Black Barrier"),GM->TargetedCardFrom(A,GM->MatchEpoch,14,uint8(ECardId::Barrier),{9,9}).bAccepted && GM->Match.Board.Barriers.Contains(FIntPoint(9,9)));
    for (int32 I=0; I<361; ++I) { TestTrue(TEXT("Manual Barrier preserves every stone"),GM->Match.Board.Cells[I]==BeforeBarrier[I]); }
    TestTrue(TEXT("Manual next ordinary placement"),GM->PlaceFrom(B,GM->MatchEpoch,15,{16,15}).bAccepted); CheckViews();
    World->DestroyWorld(false);
    return true;
}
#endif
