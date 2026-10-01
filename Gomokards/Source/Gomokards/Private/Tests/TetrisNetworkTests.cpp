#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchGameState.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Presentation/SLocalMatchView.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
template<typename T> bool SameTetrisView(const T& A,const T& B)
{ return T::StaticStruct()->CompareScriptStruct(&A,&B,0); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisNetworkTest,"Gomokards.Phase5B.TetrisAuthorityPoseAndLifecycle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FTetrisNetworkTest::RunTest(const FString&)
{
    using namespace Gomokards;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("World"),World)) { return false; }
    auto* GM=World->SpawnActor<ALocalMatchGameMode>(); auto* GS=World->SpawnActor<ALocalMatchGameState>();
    auto* A=World->SpawnActor<ALocalMatchPlayerController>(); auto* B=World->SpawnActor<ALocalMatchPlayerController>();
    auto* Third=World->SpawnActor<ALocalMatchPlayerController>();
    if (!GM || !GS || !A || !B || !Third) { AddError(TEXT("Fixture spawn failed")); World->DestroyWorld(false); return false; }
    World->SetGameState(GS); GM->GameState=GS; GM->bSessionInitialized=true;
    GM->StartWithSeed(5751); GM->Join(A,true);
    TestFalse(TEXT("Waiting card rejection"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris)).bAccepted);
    GM->Join(B,false);
    const auto Reset=[GM,A,B](int32 Seed=5751)
    { GM->Session=EMatchSession::Playing; GM->Assignments.Add(A,0); GM->Assignments.Add(B,1); GM->StartWithSeed(Seed); };
    const auto Operator=[GM,A,B]() { return GM->Assignments[A]==GM->Match.Players[GM->Match.Tetris.OperatorIndex].Id ? A : B; };
    const auto CheckViews=[this,GM,GS,A,B]()
    {
        const auto& Pose=GS->TetrisPose;
        TestTrue(TEXT("Pose tied to committed board"),Pose.Epoch==GM->MatchEpoch && Pose.BoardRevision==GM->Revision && Pose.ActivationToken==GM->TetrisActivationToken);
        for (auto* PC : {A,B})
        {
            PC->RefreshPresentation();
            TestTrue(TEXT("Full public/private snapshots coherent"),PC->IsPresentationReady() && PC->PrivateView.Revision==GM->Revision && SameTetrisView(PC->GetPublicView(),GS->PublicView));
            if (Pose.bActive) { TestTrue(TEXT("Both clients render same authoritative pose"),PC->GetDisplayTetrisPose() && SameTetrisView(*PC->GetDisplayTetrisPose(),Pose)); }
            else { TestNull(TEXT("Inactive pose not rendered"),PC->GetDisplayTetrisPose()); }
        }
        if (Pose.bActive)
        {
            const auto& T=GM->Match.Tetris;
            TestTrue(TEXT("Pose is exact current piece only"),Pose.BlockNumber==T.BlockNumber && Pose.OperatorPlayerId==GM->Match.Players[T.OperatorIndex].Id && Pose.Shape==uint8(T.Shape) && Pose.Rotation==T.Rotation && Pose.Origin==T.Origin && Pose.SpawnEdge==uint8(T.Edge) && Pose.Stone==uint8(T.Stone));
        }
        for (int32 I=0; I<361; ++I) { TestTrue(TEXT("Committed board projection exact"),GS->PublicView.Cells[I].Stone==uint8(GM->Match.Board.Cells[I].Stone) && GS->PublicView.Cells[I].bForbidden==GM->Match.Board.Cells[I].bForbidden); }
    };
    const auto RejectInput=[this,GM,GS,A,B](ALocalMatchPlayerController* PC,uint64 Epoch,uint64 Token,int32 Block,EMatchTetrisInput Input)
    {
        const auto State=GM->Match; const auto Rev=GM->Revision; const auto Seq=GM->TetrisPoseSequence;
        const auto Deadline=GM->TetrisDeadline; const auto Gen=GM->TetrisTimerGeneration;
        const auto Public=GS->PublicView; const auto Pose=GS->TetrisPose; const auto PA=A->PrivateView,PB=B->PrivateView;
        TestFalse(TEXT("Mode input rejected"),GM->TetrisInputFrom(PC,Epoch,Token,Block,Input));
        TestTrue(TEXT("Rejected key preserves full state/RNG/timer/revision/sequence"),GM->Match==State && GM->Revision==Rev && GM->TetrisPoseSequence==Seq && GM->TetrisDeadline==Deadline && GM->TetrisTimerGeneration==Gen);
        TestTrue(TEXT("Rejected key publishes nothing"),SameTetrisView(Public,GS->PublicView) && SameTetrisView(Pose,GS->TetrisPose) && SameTetrisView(PA,A->PrivateView) && SameTetrisView(PB,B->PrivateView));
    };
    const auto RejectCard=[this,GM,GS](ALocalMatchPlayerController* PC,uint64 Epoch,uint64 Actions,uint8 Card,bool Targeted=false)
    {
        const auto State=GM->Match; const auto Rev=GM->Revision; const auto Pose=GS->TetrisPose;
        const auto Ack=Targeted ? GM->TargetedCardFrom(PC,Epoch,Actions,Card,{5,5}) : GM->CardFrom(PC,Epoch,Actions,Card);
        TestTrue(TEXT("Invalid card request atomic"),!Ack.bAccepted && GM->Match==State && GM->Revision==Rev && SameTetrisView(Pose,GS->TetrisPose));
    };
    Reset(); RejectCard(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
    GM->Match.Players[0].Hand={ECardId::Tetris}; GM->Match.Players[1].Hand={ECardId::Tetris}; GM->PublishViews();
    RejectCard(B,GM->MatchEpoch,0,uint8(ECardId::Tetris)); RejectCard(Third,GM->MatchEpoch,0,uint8(ECardId::Tetris));
    RejectCard(A,GM->MatchEpoch-1,0,uint8(ECardId::Tetris)); RejectCard(A,GM->MatchEpoch,1,uint8(ECardId::Tetris));
    RejectCard(A,GM->MatchEpoch,0,uint8(ECardId::Tetris),true);
    for (int32 C=0; C<256; ++C)
    {
        if (!IsNetworkCardEnabled(uint8(C))) { RejectCard(A,GM->MatchEpoch,0,uint8(C)); }
        if (!IsTargetedNetworkCardEnabled(uint8(C))) { RejectCard(A,GM->MatchEpoch,0,uint8(C),true); }
    }
    TestEqual(TEXT("Ten cards reachable at exactly one boundary"),GetPlayableCards().Num(),10);
    for (const auto& Card : GetPlayableCards()) { TestTrue(TEXT("Every catalog card has one correct network boundary"),IsNetworkCardEnabled(uint8(Card.Id)) != IsTargetedNetworkCardEnabled(uint8(Card.Id))); }
    GM->Match.bCardsDisabled=true; RejectCard(A,GM->MatchEpoch,0,uint8(ECardId::Tetris)); GM->Match.bCardsDisabled=false;
    GM->Match.GhostPhase=EGhostPhase::Preparation; RejectCard(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
    GM->Match.GhostPhase=EGhostPhase::Hidden; RejectCard(A,GM->MatchEpoch,0,uint8(ECardId::Tetris)); GM->Match.GhostPhase=EGhostPhase::None;
    GM->Session=EMatchSession::SessionEnded; RejectCard(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));

    for (bool Reverse : {false,true})
    {
        Reset(); if (Reverse) { GM->Assignments[A]=1; GM->Assignments[B]=0; }
        auto* Caster=Reverse ? B : A;
        GM->Match.Players[0].Hand={ECardId::Tetris,ECardId::Barrier}; GM->Match.Players[1].Hand={ECardId::Restock,ECardId::Barrier};
        GM->Match.ConfusionActionsRemaining=2; GM->PublishViews(); Caster->ToggleTargeting(uint8(ECardId::Barrier));
        auto Expected=GM->Match; ResolveAction(Expected,FActionRequest::Play(0,ECardId::Tetris));
        TestTrue(TEXT("Normal card activation parity"),GM->CardFrom(Caster,GM->MatchEpoch,0,uint8(ECardId::Tetris)).bAccepted && GM->Match==Expected);
        TestTrue(TEXT("One card action/transfer/Confusion; opponent first, opposite block color"),GM->Match.CompletedActions==1 && GM->Match.CurrentPlayerIndex==1 && GM->Match.ConfusionActionsRemaining==1 && GM->Match.Tetris.OperatorIndex==1 && GM->Match.Tetris.Stone==EStone::Black);
        CheckViews(); TestEqual(TEXT("Target selection cleared"),Caster->SelectedTargetedCard,uint8(0));
        for (auto* PC : {A,B}) { TestFalse(TEXT("No placement UI"),PC->CanPlace({8,8})); TestFalse(TEXT("No card UI"),PC->CanPlayCard(uint8(ECardId::Restock))); TestFalse(TEXT("No targeting UI"),PC->CanTargetCard(uint8(ECardId::Barrier))); }
        Operator()->bPending=true;
        TestTrue(TEXT("Tetris keys independent of ordinary pending action gate"),Operator()->CanSendTetrisInput());
        Operator()->bPending=false;
        const auto Token=GM->TetrisActivationToken; const auto Epoch=GM->MatchEpoch;
        RejectInput(Caster,Epoch,Token,1,EMatchTetrisInput::Rotate); // Host admin is not operator when A is caster.
        RejectInput(Third,Epoch,Token,1,EMatchTetrisInput::Rotate);
        RejectInput(Operator(),Epoch-1,Token,1,EMatchTetrisInput::Rotate);
        RejectInput(Operator(),Epoch,Token-1,1,EMatchTetrisInput::Rotate);
        RejectInput(Operator(),Epoch,Token,2,EMatchTetrisInput::Rotate);
        for (int32 Input=5; Input<256; ++Input) { RejectInput(Operator(),Epoch,Token,1,static_cast<EMatchTetrisInput>(Input)); }
        const auto Frozen=GM->Match; const auto Rev=GM->Revision;
        TestFalse(TEXT("Crafted ordinary placement rejected"),GM->PlaceFrom(Operator(),Epoch,1,{8,8}).bAccepted);
        RejectCard(Operator(),Epoch,1,uint8(ECardId::Restock)); RejectCard(Operator(),Epoch,1,uint8(ECardId::Barrier),true);
        TestTrue(TEXT("Rejected ordinary inputs preserve mode"),GM->Match==Frozen && GM->Revision==Rev);
        // Six real opportunities: only server timer steps, exact Core parity including RNG after each spawn.
        int32 Steps=0,Locks=0;
        while (GM->Match.Tetris.bActive && Steps++<200)
        {
            const auto Before=GM->Match; const auto Public=GS->PublicView; const auto PA=A->PrivateView,PB=B->PrivateView;
            const auto Pose=GS->TetrisPose; Expected=Before; StepTetrisGravity(Expected);
            GM->PollTetrisGravity(GM->TetrisDeadline,GM->TetrisTimerGeneration);
            TestTrue(TEXT("Gravity/spawn/clear RNG exactly Core"),GM->Match==Expected);
            TestTrue(TEXT("Mode never spends ordinary action/Confusion/rewards"),GM->Match.CompletedActions==1 && GM->Match.CurrentPlayerIndex==1 && GM->Match.ConfusionActionsRemaining==1 && GM->Match.Players==Before.Players);
            if (GM->Match.Tetris.bActive && GM->Match.Tetris.BlockNumber==Before.Tetris.BlockNumber)
            { TestTrue(TEXT("Gravity move only pose"),GM->Revision==Public.Revision && GS->TetrisPose.PoseSequence>Pose.PoseSequence && SameTetrisView(Public,GS->PublicView) && SameTetrisView(PA,A->PrivateView) && SameTetrisView(PB,B->PrivateView)); }
            else
            {
                ++Locks; TestEqual(TEXT("Lock publishes one full revision"),GM->Revision,Public.Revision+1);
                if (GM->Match.Tetris.bActive)
                {
                    TestEqual(TEXT("Operator alternates independently of ordinary turn"),GM->Match.Tetris.OperatorIndex,(GM->Match.Tetris.BlockNumber%2));
                    RejectInput(Operator(),Epoch,Token,Before.Tetris.BlockNumber,EMatchTetrisInput::Rotate);
                    // Current piece accepts input from its operator even on the other ordinary player's block.
                    GM->TetrisInputFrom(Operator(),Epoch,Token,GM->Match.Tetris.BlockNumber,EMatchTetrisInput::Rotate);
                }
            }
            CheckViews();
        }
        TestTrue(TEXT("Six opportunities finish without extra ordinary transfer"),!GM->Match.Tetris.bActive && Locks==6 && GM->Match.CurrentPlayerIndex==1 && GM->Match.Result.Status==EMatchStatus::InProgress);
        TestTrue(TEXT("Ordinary play resumes"),GM->PlaceFrom(Reverse ? A : B,Epoch,1,{18,18}).bAccepted);
        RejectInput(A,Epoch,Token,6,EMatchTetrisInput::Rotate);
    }

    // Four orientations from a centered valid fixture, all keys through assigned network adapter.
    for (int32 Edge=0; Edge<4; ++Edge)
    {
        Reset(); GM->Match.Players[0].Hand={ECardId::Tetris}; GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
        auto& T=GM->Match.Tetris; T.Shape=ETetrisShape::L; T.Rotation=0; T.Origin={8,8}; T.Edge=static_cast<ETetrisEdge>(Edge); GM->PublishViews();
        for (int32 K=0; K<5; ++K)
        {
            T.Origin={8,8}; T.Rotation=0; GM->PublishViews();
            const auto Input=static_cast<ETetrisInput>(K); const auto Delta=TetrisTranslation(Input),Gravity=TetrisGravity(T.Edge);
            if (Delta==Gravity*-1) { RejectInput(B,GM->MatchEpoch,GM->TetrisActivationToken,T.BlockNumber,static_cast<EMatchTetrisInput>(K)); continue; }
            auto Expected=GM->Match; ApplyTetrisInput(Expected,Input);
            const auto Public=GS->PublicView; const auto PA=A->PrivateView,PB=B->PrivateView; const auto Pose=GS->TetrisPose;
            const auto Deadline=GM->TetrisDeadline; const double BeforeTime=FPlatformTime::Seconds();
            TestTrue(TEXT("Allowed absolute input exact Core"),GM->TetrisInputFrom(B,GM->MatchEpoch,GM->TetrisActivationToken,T.BlockNumber,static_cast<EMatchTetrisInput>(K)) && GM->Match==Expected);
            const double AfterTime=FPlatformTime::Seconds();
            TestTrue(TEXT("Manual movement publishes only pose"),GM->Revision==Public.Revision && GS->TetrisPose.PoseSequence==Pose.PoseSequence+1 && GS->TetrisPose.BoardRevision==Public.Revision && SameTetrisView(Public,GS->PublicView) && SameTetrisView(PA,A->PrivateView) && SameTetrisView(PB,B->PrivateView));
            TestTrue(TEXT("Soft drop resets to now+.5 only; rotation/perpendicular preserve deadline"),Delta==Gravity ? GM->TetrisDeadline>=BeforeTime+.5 && GM->TetrisDeadline<=AfterTime+.5 : GM->TetrisDeadline==Deadline);
            CheckViews();
        }
        T.Shape=ETetrisShape::Line; T.Rotation=0; T.Origin={18,8}; GM->PublishViews();
        RejectInput(B,GM->MatchEpoch,GM->TetrisActivationToken,T.BlockNumber,EMatchTetrisInput::Rotate);
        // Rotation cannot kick the vertical line away from the right boundary.
        T.Shape=ETetrisShape::Square; T.Rotation=0; T.Origin={8,8}; GM->Match.Board.Barriers.Add({8,8}); GM->PublishViews();
        const auto Side=Edge<2 ? EMatchTetrisInput::Right : EMatchTetrisInput::Down;
        TestTrue(TEXT("Barrier is not physical collision through server adapter"),GM->TetrisInputFrom(B,GM->MatchEpoch,GM->TetrisActivationToken,T.BlockNumber,Side));
        // Make an obstacle in the leading footprint; manual gravity rejects, timer alone locks.
        T.Shape=ETetrisShape::Square; T.Rotation=0; T.Origin={8,8};
        const auto G=TetrisGravity(T.Edge); const auto Cells=TetrisOffsets(T.Shape);
        for (auto O : Cells) { if (!Cells.Contains(O+G)) { GM->Match.Board.At(T.Origin+O+G).bForbidden=true; } }
        GM->PublishViews();
        EMatchTetrisInput Fall=Edge==0 ? EMatchTetrisInput::Down : Edge==1 ? EMatchTetrisInput::Up : Edge==2 ? EMatchTetrisInput::Right : EMatchTetrisInput::Left;
        RejectInput(B,GM->MatchEpoch,GM->TetrisActivationToken,T.BlockNumber,Fall);
        const auto Board=GM->Match.Board; const auto Origin=T.Origin; const auto Stone=T.Stone; const auto Rev=GM->Revision;
        GM->PollTetrisGravity(GM->TetrisDeadline,GM->TetrisTimerGeneration);
        TestEqual(TEXT("Blocked automatic step locks and commits"),GM->Revision,Rev+1);
        for (auto O : Cells) { TestEqual(TEXT("Authoritative lock stone"),GM->Match.Board.At(Origin+O).Stone,Stone); }
        for (int32 I=0; I<361; ++I) { TestEqual(TEXT("Nuke flags preserved"),GM->Match.Board.Cells[I].bForbidden,Board.Cells[I].bForbidden); }
        CheckViews();
    }

    // Line-clear and Barrier parity on final lock: no collapse, no reward, final inactive pose.
    for (bool Barrier : {false,true})
    {
        Reset(); GM->Match.Players[0].Hand={ECardId::Tetris}; GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
        auto& T=GM->Match.Tetris; T.BlockNumber=6; T.Shape=ETetrisShape::Square; T.Rotation=0; T.Edge=ETetrisEdge::Top; T.Origin={3,17}; T.Stone=EStone::Black;
        for (int32 X=0; X<3; ++X) { GM->Match.Board.At({X,18}).Stone=EStone::Black; }
        GM->Match.Board.At({12,12}).Stone=EStone::White; GM->Match.Board.At({10,10}).bForbidden=true;
        if (Barrier) { GM->Match.Board.Barriers.Add({1,17}); }
        GM->PublishViews(); auto Expected=GM->Match; StepTetrisGravity(Expected);
        GM->PollTetrisGravity(GM->TetrisDeadline,GM->TetrisTimerGeneration);
        TestTrue(TEXT("Final clear/result exact Core"),GM->Match==Expected && !GS->PublicView.bTetrisActive && !GS->TetrisPose.bActive);
        TestEqual(TEXT("Barrier splits five-cell connectivity"),GM->Match.Board.At({0,18}).Stone,Barrier ? EStone::Black : EStone::Empty);
        TestTrue(TEXT("No collapse; other stones/forbidden remain"),GM->Match.Board.At({3,17}).Stone==EStone::Black && GM->Match.Board.At({12,12}).Stone==EStone::White && GM->Match.Board.At({10,10}).bForbidden);
        CheckViews();
    }
    // No-spawn activation skips all six with no fake active frame and no invented loss.
    Reset(); for (auto& C : GM->Match.Board.Cells) { C.bForbidden=true; }
    GM->Match.Players[0].Hand={ECardId::Tetris}; GM->PublishViews(); auto Skipped=GM->Match; ResolveAction(Skipped,FActionRequest::Play(0,ECardId::Tetris));
    TestTrue(TEXT("All skipped activation exact Core"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris)).bAccepted && GM->Match==Skipped && !GS->TetrisPose.bActive && !GS->PublicView.bTetrisActive);
    TestTrue(TEXT("No top-out invented"),GM->Match.Result.Status==EMatchStatus::AwaitingRuleDecision); CheckViews();
    // Mixed skip/deploy: only a 2x2 aperture fits, so failed opportunities are never published as fake blocks.
    int32 SkipSeed=0;
    for (; SkipSeed<1000; ++SkipSeed) { FRandomStream Probe(SkipSeed); if (Probe.RandRange(0,5)!=0 && Probe.RandRange(0,5)==0) { break; } }
    Reset(SkipSeed); for (auto& C : GM->Match.Board.Cells) { C.bForbidden=true; }
    for (int32 X=8; X<10; ++X) { for (int32 Y=0; Y<2; ++Y) { GM->Match.Board.At({X,Y}).bForbidden=false; } }
    GM->Match.Players[0].Hand={ECardId::Tetris}; GM->PublishViews();
    auto Mixed=GM->Match; ResolveAction(Mixed,FActionRequest::Play(0,ECardId::Tetris));
    GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
    TestTrue(TEXT("Skipped first opportunity publishes block two/caster operator/fallback"),GM->Match==Mixed && GS->TetrisPose.BlockNumber==2 && GS->TetrisPose.OperatorPlayerId==0 && GS->TetrisPose.Origin==FIntPoint(8,0));
    RejectInput(B,GM->MatchEpoch,GM->TetrisActivationToken,1,EMatchTetrisInput::Down);
    StepTetrisGravity(Mixed); const auto MixedRev=GM->Revision;
    GM->PollTetrisGravity(GM->TetrisDeadline,GM->TetrisTimerGeneration);
    TestTrue(TEXT("Lock plus remaining skips publish one final inactive revision"),GM->Match==Mixed && !GS->TetrisPose.bActive && GM->Revision==MixedRev+1); CheckViews();
    for (int32 Result=0; Result<3; ++Result)
    {
        Reset(); for (auto& C : GM->Match.Board.Cells) { C.bForbidden=true; }
        for (int32 X=0; X<5; ++X)
        {
            if (Result!=1) { GM->Match.Board.At({X,2})={EStone::Black,false}; }
            if (Result!=0) { GM->Match.Board.At({X,4})={EStone::White,false}; }
        }
        GM->Match.Players[0].Hand={ECardId::Tetris}; GM->PublishViews();
        auto Terminal=GM->Match; ResolveAction(Terminal,FActionRequest::Play(0,ECardId::Tetris));
        GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
        TestTrue(TEXT("Skipped exit propagates global Black/White/Draw result"),GM->Match==Terminal && GM->Match.Result.Status==(Result==2 ? EMatchStatus::Draw : EMatchStatus::Won) && !GS->PublicView.bTetrisActive && !GS->TetrisPose.bActive);
        CheckViews();
    }
    // Progress gate even if no ordinary point remains; this fixture tests transport availability only.
    Reset(); GM->Match.Players[0].Hand={ECardId::Tetris}; GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
    for (auto& C : GM->Match.Board.Cells) { C.bForbidden=true; } GM->PublishViews();
    TestEqual(TEXT("Active Tetris cannot prematurely end runtime session"),GM->Session,EMatchSession::Playing);

    // Separate pose/public/private arrival, coalescing, stale sequence and final mode exit.
    Reset(); GM->Match.Players[0].Hand={ECardId::Tetris}; GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris)); CheckViews();
    const auto OldPublic=GS->PublicView; const auto OldPrivate=A->PrivateView; const auto OldPose=GS->TetrisPose;
    auto NewPose=OldPose; ++NewPose.BoardRevision; ++NewPose.PoseSequence; ++NewPose.BlockNumber;
    GS->TetrisPose=NewPose; A->RefreshPresentation(); TestNull(TEXT("Future pose withheld against old board"),A->GetDisplayTetrisPose());
    auto NewPublic=OldPublic; ++NewPublic.Revision; GS->PublicView=NewPublic; A->RefreshPresentation(); TestNull(TEXT("Public waits for private"),A->GetDisplayTetrisPose());
    ++A->PrivateView.Revision; A->RefreshPresentation(); TestNotNull(TEXT("Matching board/private/pose displays"),A->GetDisplayTetrisPose());
    GS->TetrisPose=OldPose; A->RefreshPresentation(); TestTrue(TEXT("Old sequence cannot resurrect old block"),A->GetDisplayTetrisPose() && A->GetDisplayTetrisPose()->BlockNumber==NewPose.BlockNumber);
    A->LatestTetrisPose=OldPose; A->DisplayPublic=OldPublic; A->DisplayPrivate=OldPrivate; A->RefreshPresentation();
    TestNull(TEXT("Board-first does not show stale old piece"),A->GetDisplayTetrisPose());
    GS->TetrisPose=NewPose; NewPose.PoseSequence+=10; NewPose.Origin+={1,0}; GS->TetrisPose=NewPose; A->RefreshPresentation();
    TestTrue(TEXT("Coalesced sequence accepted without intermediate frames"),A->GetDisplayTetrisPose() && A->GetDisplayTetrisPose()->PoseSequence==NewPose.PoseSequence);
    GS->PublicView.bTetrisActive=false; ++GS->PublicView.Revision; ++A->PrivateView.Revision; A->RefreshPresentation(); TestNull(TEXT("Final board gate hides stale active pose"),A->GetDisplayTetrisPose());

    // Practical recipe from accepted seed, no injected hand or forced runtime spawn.
    Reset(5751);
    const TArray<FIntPoint> Opening={{5,5},{6,5},{0,0},{7,5},{8,5},{1,0},{9,9},{10,9},{12,12},{11,9},{12,9},{5,12},{6,12},{0,18},{7,12},{8,12}};
    for (int32 I=0; I<Opening.Num(); ++I) { TestTrue(TEXT("Manual opening"),GM->PlaceFrom(I%2==0 ? A : B,GM->MatchEpoch,I,Opening[I]).bAccepted); }
    TestTrue(TEXT("White naturally owns Tetris"),GM->Match.Players[1].Hand==TArray<ECardId>{ECardId::SwapHands,ECardId::Tetris});
    TestTrue(TEXT("Black ordinary move transfers to White"),GM->PlaceFrom(A,GM->MatchEpoch,16,{17,17}).bAccepted);
    TestTrue(TEXT("White casts Tetris; Black operates first"),GM->CardFrom(B,GM->MatchEpoch,17,uint8(ECardId::Tetris)).bAccepted && GM->Match.Tetris.OperatorIndex==0);
    AddInfo(FString::Printf(TEXT("Seed5751 initial: shape=%d edge=%d origin=(%d,%d) blockColor=%d"),int32(GM->Match.Tetris.Shape),int32(GM->Match.Tetris.Edge),GM->Match.Tetris.Origin.X,GM->Match.Tetris.Origin.Y,int32(GM->Match.Tetris.Stone)));
    int32 RecipeSteps=0; TArray<int32> Opportunities;
    while (GM->Match.Tetris.bActive && RecipeSteps++<200)
    { Opportunities.AddUnique(GM->Match.Tetris.BlockNumber); GM->PollTetrisGravity(GM->TetrisDeadline,GM->TetrisTimerGeneration); }
    TestTrue(TEXT("Hands-off recipe finishes all six and returns to Black"),Opportunities.Num()==6 && !GM->Match.Tetris.bActive && GM->Match.Result.Status==EMatchStatus::InProgress && GM->Match.CurrentPlayerIndex==0 && GM->Match.CompletedActions==18);
    TestTrue(TEXT("Normal play resumes after recipe"),GM->PlaceFrom(A,GM->MatchEpoch,18,{18,0}).bAccepted);

    const auto OldActivation=GM->TetrisActivationToken;
    GM->Match.Players[1].Hand.Add(ECardId::Tetris); GM->PublishViews();
    TestTrue(TEXT("Second cast in same epoch creates a new activation token"),GM->CardFrom(B,GM->MatchEpoch,19,uint8(ECardId::Tetris)).bAccepted && GM->TetrisActivationToken>OldActivation);
    RejectInput(A,GM->MatchEpoch,OldActivation,GM->Match.Tetris.BlockNumber,EMatchTetrisInput::Rotate);

    for (int32 End=0; End<3; ++End)
    {
        Reset(); GM->Match.Players[0].Hand={ECardId::Tetris}; GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
        const auto Epoch=GM->MatchEpoch,Token=GM->TetrisActivationToken,Gen=GM->TetrisTimerGeneration; const auto Deadline=GM->TetrisDeadline;
        if (End==0) { TestTrue(TEXT("Authorized restart"),GM->RestartFrom(A,Epoch).bAccepted); }
        else if (End==1) { GM->Leave(B); }
        else { GM->EndPlay(EEndPlayReason::Destroyed); }
        const auto State=GM->Match; const auto Rev=GM->Revision; const auto Seq=GM->TetrisPoseSequence;
        TestFalse(TEXT("Stale gravity cancelled"),GM->PollTetrisGravity(Deadline+10,Gen));
        TestTrue(TEXT("No stale timer mutation or publication"),GM->Match==State && GM->Revision==Rev && GM->TetrisPoseSequence==Seq && !GM->TetrisTicker.IsValid());
        if (End<2) { RejectInput(A,Epoch,Token,1,EMatchTetrisInput::Down); TestFalse(TEXT("Restart/disconnect clears transmitted pose"),GS->TetrisPose.bActive); }
        if (End==0)
        {
            GM->Match.Players[0].Hand={ECardId::Tetris}; GM->PublishViews(); GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Tetris));
            TestTrue(TEXT("Recast token changes"),GM->TetrisActivationToken!=Token);
            RejectInput(B,GM->MatchEpoch,Token,1,EMatchTetrisInput::Rotate);
        }
    }
    World->DestroyWorld(false);
    return true;
}
#endif
