#include "Core/TetrisRules.h"
#include "Core/MatchRules.h"
#include "Presentation/MatchPresentation.h"
#include "Runtime/LocalMatchGameMode.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace Gomokards;
namespace
{
constexpr EAutomationTestFlags TetrisFlags=EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
const ETetrisEdge Edges[]={ETetrisEdge::Top,ETetrisEdge::Bottom,ETetrisEdge::Left,ETetrisEdge::Right};
const ETetrisInput Moves[]={ETetrisInput::Up,ETetrisInput::Down,ETetrisInput::Left,ETetrisInput::Right};
const FIntPoint Deltas[]={{0,-1},{0,1},{-1,0},{1,0}};
FActionResult CastTetris(FMatchState& S)
{
    S.Players[S.CurrentPlayerIndex].Hand.Add(ECardId::Tetris);
    return ResolveAction(S,FActionRequest::Play(S.Players[S.CurrentPlayerIndex].Id,ECardId::Tetris));
}
FMatchState TetrisFixture(ETetrisEdge Edge=ETetrisEdge::Top)
{
    FMatchState S(42); S.CurrentPlayerIndex=1; S.CompletedActions=1; S.ConfusionActionsRemaining=1;
    S.Tetris.bActive=true; S.Tetris.BlockNumber=1; S.Tetris.OperatorIndex=1;
    S.Tetris.Shape=ETetrisShape::Square; S.Tetris.Origin={8,8}; S.Tetris.Edge=Edge; S.Tetris.Stone=EStone::Black;
    return S;
}
FBoard ClosedBoard()
{
    FBoard B; for (auto& Cell : B.Cells) { Cell.bForbidden=true; } return B;
}
void OpenRectangle(FBoard& B,int32 X,int32 Y,int32 Width,int32 Height)
{ for (int32 J=Y; J<Y+Height; ++J) { for (int32 I=X; I<X+Width; ++I) { B.At({I,J})={}; } } }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisActivation,"Gomokards.Phase3C.ActivationAndEffects",TetrisFlags)
bool FTetrisActivation::RunTest(const FString&)
{
    FMatchState S(9); S.ConfusionActionsRemaining=2;
    S.Players[0].Hand={ECardId::Restock}; S.Players[1].Hand={ECardId::Ghost,ECardId::BackToBasics};
    S.Board.At({5,5}).Stone=EStone::White; S.Board.At({6,6}).bForbidden=true; S.Board.Barriers.Add({5,5});
    const auto Board=S.Board;
    const auto Result=CastTetris(S);
    TestTrue(TEXT("Card consumed once; one normal action and transfer"),Result.IsAccepted() && !Result.bBlockingReward && S.CompletedActions==1 && S.CurrentPlayerIndex==1 && S.Players[0].Hand==TArray<ECardId>{ECardId::Restock});
    TestTrue(TEXT("Opponent starts with opposite assigned color; Confusion only progresses on cast"),S.Tetris.bActive && S.Tetris.BlockNumber==1 && S.Tetris.OperatorIndex==1 && S.Tetris.Stone==EStone::Black && S.ConfusionActionsRemaining==1);
    TestTrue(TEXT("Cast preserves committed colors/metadata and other hand"),S.Board==Board && S.Players[1].Hand.Num()==2);
    const auto Before=S;
    for (const auto& Request : {FActionRequest::Place(1,{0,0}),FActionRequest::Play(1,ECardId::Ghost),FActionRequest::Play(1,ECardId::BackToBasics)})
    { TestTrue(TEXT("Ordinary actions reject atomically during Tetris"),ResolveAction(S,Request).Error==EActionError::TetrisActive && S==Before); }
    FTargetSelection Selection; S.Players[1].Hand.Add(ECardId::TacticalNuke);
    TestFalse(TEXT("No card targeting during Tetris"),Selection.Toggle(S,ECardId::TacticalNuke));
    TestFalse(TEXT("No duplicate mode entry"),BeginTetris(S));
    S.Reset(9); S.bCardsDisabled=true; S.Players[0].Hand={ECardId::Tetris}; const auto Locked=S;
    TestTrue(TEXT("Basics prevents Tetris cast without consuming"),ResolveAction(S,FActionRequest::Play(0,ECardId::Tetris)).Error==EActionError::CardsDisabled && S==Locked);
    S.bCardsDisabled=false; S.GhostPhase=EGhostPhase::Hidden;
    TestTrue(TEXT("Ghost prevents overlap"),ResolveAction(S,FActionRequest::Play(0,ECardId::Tetris)).Error==EActionError::GhostCardsRestricted && !S.Tetris.bActive);
    TestFalse(TEXT("Explicit Tetris entry also rejects Ghost"),BeginTetris(S));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisShapes,"Gomokards.Phase3C.ShapesAndRotation",TetrisFlags)
bool FTetrisShapes::RunTest(const FString&)
{
    TestEqual(TEXT("Exactly six definitions"),static_cast<int32>(ETetrisShape::Count),6);
    for (int32 I=0; I<6; ++I)
    {
        const auto Shape=static_cast<ETetrisShape>(I);
        for (uint8 R=0; R<4; ++R)
        {
            const auto Cells=TetrisOffsets(Shape,R); TArray<FIntPoint> Unique;
            for (FIntPoint P : Cells) { Unique.AddUnique(P); }
            TestTrue(TEXT("Every rotation preserves unique cell count"),Cells.Num()==(Shape==ETetrisShape::Cross ? 5 : 4) && Cells.Num()==Unique.Num());
        }
        auto S=TetrisFixture(); S.Tetris.Shape=Shape; const auto Before=S;
        for (int32 R=0; R<4; ++R) { TestTrue(TEXT("Clear-space rotation succeeds"),ApplyTetrisInput(S,ETetrisInput::Rotate)); }
        TestTrue(TEXT("Four rotations restore all state and RNG"),S==Before);
    }
    TestTrue(TEXT("L rotates clockwise in screen coordinates"),TetrisOffsets(ETetrisShape::L,1)==TArray<FIntPoint>{{2,0},{1,0},{0,0},{0,1}});
    auto S=TetrisFixture(); S.Tetris.Shape=ETetrisShape::Line; S.Tetris.Origin={18,8}; const auto Before=S;
    TestTrue(TEXT("Boundary rotation fails without kick or mutation"),!ApplyTetrisInput(S,ETetrisInput::Rotate) && S==Before);
    for (bool Forbidden : {false,true})
    {
        S=TetrisFixture(); S.Tetris.Shape=ETetrisShape::L;
        S.Board.At({10,8})=Forbidden ? FCell{EStone::Empty,true} : FCell{EStone::White,false}; const auto Blocked=S;
        TestTrue(TEXT("Stone/forbidden rotation preserves origin/orientation/RNG"),!ApplyTetrisInput(S,ETetrisInput::Rotate) && S==Blocked);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSpawnTest,"Gomokards.Phase3C.FourEdgeSpawnAndClearance",TetrisFlags)
bool FTetrisSpawnTest::RunTest(const FString&)
{
    const FIntPoint Gravity[]={{0,1},{0,-1},{1,0},{-1,0}};
    FBoard Empty;
    for (int32 I=0; I<4; ++I)
    {
        TestTrue(TEXT("All four inward gravity directions"),TetrisGravity(Edges[I])==Gravity[I]);
        for (int32 Shape=0; Shape<6; ++Shape)
        {
            const auto Id=static_cast<ETetrisShape>(Shape); const auto Cells=TetrisOffsets(Id);
            const auto Spawn=BestTetrisSpawnAtEdge(Empty,Id,Edges[I]);
            TestTrue(TEXT("Every shape can spawn at every edge"),Spawn.bLegal && Spawn.Edge==Edges[I] && TetrisFits(Empty,Cells,Spawn.Origin));
            bool Touches=false;
            for (FIntPoint P : Cells)
            { P+=Spawn.Origin; Touches|=(I==0 && P.Y==0)||(I==1 && P.Y==18)||(I==2 && P.X==0)||(I==3 && P.X==18); }
            TestTrue(TEXT("Canonical complete footprint touches selected edge"),Touches);
            TestTrue(TEXT("Clearance is exactly consecutive legal inward steps"),TetrisFits(Empty,Cells,Spawn.Origin+Gravity[I]*Spawn.Clearance) && !TetrisFits(Empty,Cells,Spawn.Origin+Gravity[I]*(Spawn.Clearance+1)));
        }
    }
    const auto Center=BestTetrisSpawnAtEdge(Empty,ETetrisShape::Square,ETetrisEdge::Top);
    TestTrue(TEXT("Center tie uses deterministic lower coordinate"),Center.Origin==FIntPoint(8,0) && Center.Clearance==17);
    FBoard Corridor=ClosedBoard(); OpenRectangle(Corridor,8,0,2,7);
    for (int32 Seed=0; Seed<32; ++Seed)
    {
        FRandomStream R(Seed); const auto Spawn=ChooseTetrisSpawn(Corridor,ETetrisShape::Square,R);
        TestTrue(TEXT("Clearance five qualifies only Top; no edge RNG for sole choice"),Spawn.bLegal && Spawn.Edge==ETetrisEdge::Top && Spawn.Clearance==5 && R.GetCurrentSeed()==Seed);
    }
    // Turn a formerly free cell into each physical obstacle type; unrelated edge positions stay irrelevant.
    for (bool Forbidden : {false,true})
    {
        FBoard B=Empty; B.At({8,1})=Forbidden ? FCell{EStone::Empty,true} : FCell{EStone::Black,false};
        const auto Spawn=BestTetrisSpawnAtEdge(B,ETetrisShape::Square,ETetrisEdge::Top);
        TestTrue(TEXT("Obstacle reroutes footprint, never rejects whole edge"),Spawn.bLegal && Spawn.Clearance==17 && TetrisFits(B,TetrisOffsets(ETetrisShape::Square),Spawn.Origin));
    }
    FBoard Barrier=Empty; Barrier.Barriers.Add({8,0});
    TestTrue(TEXT("Barrier ignored for spawn and inward clearance"),BestTetrisSpawnAtEdge(Barrier,ETetrisShape::Square,ETetrisEdge::Top)==Center);
    OpenRectangle(Corridor,8,7,2,12); // Only Top/Bottom have starting footprints, both preferred.
    TArray<ETetrisEdge> Seen;
    for (int32 Seed=0; Seed<64; ++Seed)
    {
        FRandomStream R(Seed),Expected(Seed); const int32 Pick=Expected.RandRange(0,1);
        const auto Spawn=ChooseTetrisSpawn(Corridor,ETetrisShape::Square,R); Seen.AddUnique(Spawn.Edge);
        TestTrue(TEXT("Sample uniformly only eligible edges; exactly one RNG draw"),Spawn.Edge==Edges[Pick] && R.GetCurrentSeed()==Expected.GetCurrentSeed());
    }
    TestEqual(TEXT("Both preferred edges selected in sample"),Seen.Num(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisFallback,"Gomokards.Phase3C.CrowdedFallbackAndSkippedBlocks",TetrisFlags)
bool FTetrisFallback::RunTest(const FString&)
{
    FBoard B=ClosedBoard(); OpenRectangle(B,2,0,2,4); OpenRectangle(B,8,15,2,4);
    FRandomStream R(31); auto Spawn=ChooseTetrisSpawn(B,ETetrisShape::Square,R);
    TestTrue(TEXT("Fallback under five chooses center after equal clearance"),Spawn.bLegal && Spawn.Edge==ETetrisEdge::Bottom && Spawn.Clearance==2 && R.GetCurrentSeed()==31);
    OpenRectangle(B,2,4,2,1); Spawn=ChooseTetrisSpawn(B,ETetrisShape::Square,R);
    TestTrue(TEXT("Fallback prioritizes greater clearance over center"),Spawn.Edge==ETetrisEdge::Top && Spawn.Clearance==3 && R.GetCurrentSeed()==31);
    B=ClosedBoard(); OpenRectangle(B,8,0,2,6); OpenRectangle(B,8,13,2,6);
    Spawn=ChooseTetrisSpawn(B,ETetrisShape::Square,R);
    TestTrue(TEXT("Clearance four is fallback and final edge tie is deterministic"),Spawn.Edge==ETetrisEdge::Top && Spawn.Clearance==4 && R.GetCurrentSeed()==31);
    FMatchState S(31); S.Board=ClosedBoard(); S.ConfusionActionsRemaining=2; const FBoard Before=S.Board;
    FRandomStream Expected(31); for (int32 I=0; I<6; ++I) { Expected.RandRange(0,5); }
    CastTetris(S);
    TestTrue(TEXT("Six blocked opportunities consume six shape draws, no edge draw"),S.Random.GetCurrentSeed()==Expected.GetCurrentSeed());
    TestTrue(TEXT("Skipped blocks do not write/reward/spend extra actions or turns"),S.Board==Before && S.CompletedActions==1 && S.CurrentPlayerIndex==1 && S.ConfusionActionsRemaining==1 && S.Players[0].Hand.IsEmpty() && S.Players[1].Hand.IsEmpty());
    TestTrue(TEXT("All skipped exits to existing no-action decision"),!S.Tetris.bActive && S.Result.Status==EMatchStatus::AwaitingRuleDecision && S.Result.Decision==EDecisionReason::NoLegalAction);
    // A 2x2 aperture only fits Square. Find a seed whose first shape skips and second deploys.
    int32 Seed=0;
    for (; Seed<1000; ++Seed) { FRandomStream Probe(Seed); if (Probe.RandRange(0,5)!=0 && Probe.RandRange(0,5)==0) { break; } }
    TestTrue(TEXT("Deterministic mixed skip/deploy seed exists"),Seed<1000);
    S=FMatchState(Seed); S.Board=ClosedBoard(); OpenRectangle(S.Board,8,0,2,2); CastTetris(S);
    TestTrue(TEXT("Exactly one skipped block advances to caster's block two"),S.Tetris.bActive && S.Tetris.BlockNumber==2 && S.Tetris.OperatorIndex==0 && S.Tetris.Stone==EStone::White && S.CompletedActions==1 && S.CurrentPlayerIndex==1);
    TestTrue(TEXT("Zero clearance deploys rather than skips"),S.Tetris.Origin==FIntPoint(8,0));
    StepTetrisGravity(S);
    TestTrue(TEXT("Lock then remaining skips exit without further action/turn"),!S.Tetris.bActive && S.Board.At({8,0}).Stone==EStone::White && S.CompletedActions==1 && S.CurrentPlayerIndex==1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisMovement,"Gomokards.Phase3C.AbsoluteMovementAndCollision",TetrisFlags)
bool FTetrisMovement::RunTest(const FString&)
{
    for (ETetrisEdge Edge : Edges)
    for (int32 I=0; I<4; ++I)
    {
        auto S=TetrisFixture(Edge); const auto Before=S;
        TestTrue(TEXT("Absolute translation independent of gravity"),ApplyTetrisInput(S,Moves[I]) && S.Tetris.Origin==Before.Tetris.Origin+Deltas[I]);
        auto Expected=Before; Expected.Tetris.Origin+=Deltas[I];
        TestTrue(TEXT("Movement changes only active origin"),S==Expected);
    }
    for (bool Forbidden : {false,true})
    {
        auto S=TetrisFixture(); S.Board.At({10,8})=Forbidden ? FCell{EStone::Empty,true} : FCell{EStone::White,false}; const auto Before=S;
        TestTrue(TEXT("Physical collision rejects without locking/RNG mutation"),!ApplyTetrisInput(S,ETetrisInput::Right) && S==Before);
        TestTrue(TEXT("Lateral contact does not prevent unrelated movement"),ApplyTetrisInput(S,ETetrisInput::Left) && S.Tetris.BlockNumber==1);
    }
    auto S=TetrisFixture(); S.Board.Barriers.Add({9,8});
    TestTrue(TEXT("Physical movement passes Barrier geometry"),ApplyTetrisInput(S,ETetrisInput::Right));
    S.Tetris.Origin={0,0}; const auto Before=S;
    TestTrue(TEXT("Boundary rejects without wrapping or clamping"),!ApplyTetrisInput(S,ETetrisInput::Left) && !ApplyTetrisInput(S,ETetrisInput::Up) && S==Before);
    TestTrue(TEXT("Unknown input rejects atomically"),!ApplyTetrisInput(S,static_cast<ETetrisInput>(255)) && S==Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisGravityTest,"Gomokards.Phase3C.GravityAndLocks",TetrisFlags)
bool FTetrisGravityTest::RunTest(const FString&)
{
    for (ETetrisEdge Edge : Edges)
    {
        auto S=TetrisFixture(Edge); auto Expected=S; Expected.Tetris.Origin+=TetrisGravity(Edge);
        TestTrue(TEXT("Automatic step changes origin only; no RNG/actions/effects"),StepTetrisGravity(S) && S==Expected);
    }
    auto S=TetrisFixture(); S.Tetris.BlockNumber=6; S.Tetris.OperatorIndex=0; S.Tetris.Stone=EStone::White; S.Tetris.Origin={8,17};
    S.Board.At({8,16}).Stone=EStone::Black; S.Board.At({8,15}).Stone=EStone::White;
    S.Players[0].Hand={ECardId::Ghost}; const auto Before=S;
    TestTrue(TEXT("Blocked manual inward move never locks"),!ApplyTetrisInput(S,ETetrisInput::Down) && S==Before);
    TestTrue(TEXT("Automatic blocked step locks"),StepTetrisGravity(S));
    for (FIntPoint P : TetrisOffsets(ETetrisShape::Square)) { TestTrue(TEXT("Lock writes true Tetris color"),S.Board.At(FIntPoint(8,17)+P).Stone==EStone::White); }
    TestTrue(TEXT("Lock qualifies as a block but grants no reward"),HasSuccessfulBlock(S.Board,{8,17},EStone::White) && S.Players==Before.Players && S.Random.GetCurrentSeed()==Before.Random.GetCurrentSeed());
    TestTrue(TEXT("No normal lifecycle progression on terminal block"),S.CompletedActions==Before.CompletedActions && S.CurrentPlayerIndex==Before.CurrentPlayerIndex && S.ConfusionActionsRemaining==Before.ConfusionActionsRemaining);
    TestTrue(TEXT("Sixth lock exits once; subsequent gravity inert"),!S.Tetris.bActive && !StepTetrisGravity(S));
    // Collision against a forbidden point locks at the last legal footprint, never writes the forbidden point.
    S=TetrisFixture(); S.Tetris.BlockNumber=6; S.Board.At({8,10}).bForbidden=true; StepTetrisGravity(S);
    TestTrue(TEXT("Forbidden obstacle safely locks preceding footprint"),S.Board.At({8,8}).Stone==EStone::Black && S.Board.At({8,10})==FCell{EStone::Empty,true});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisAlternation,"Gomokards.Phase3C.SixOperatorsAndDeterminism",TetrisFlags)
bool FTetrisAlternation::RunTest(const FString&)
{
    for (int32 Seed=0; Seed<16; ++Seed)
    {
        FMatchState A(Seed),B(Seed); A.ConfusionActionsRemaining=B.ConfusionActionsRemaining=2;
        CastTetris(A); CastTetris(B); TestTrue(TEXT("Seeded spawn reproduces all authoritative state"),A==B);
        int32 LastBlock=0,WhiteOperators=0,BlackOperators=0,Steps=0;
        while (A.Tetris.bActive && Steps++<500)
        {
            if (A.Tetris.BlockNumber!=LastBlock)
            {
                LastBlock=A.Tetris.BlockNumber;
                const int32 Operator=LastBlock%2;
                TestTrue(TEXT("Alternating operators and opposite assigned color"),A.Tetris.OperatorIndex==Operator && A.Tetris.Stone==OppositeStone(A.Players[Operator].AssignedStone));
                (Operator==0 ? BlackOperators : WhiteOperators)++;
            }
            StepTetrisGravity(A); StepTetrisGravity(B);
            TestTrue(TEXT("Deterministic complete sequence including shapes/edge draws/clears"),A==B);
            TestTrue(TEXT("All mode sub-actions preserve normal lifecycle and hands"),A.CompletedActions==1 && A.CurrentPlayerIndex==1 && A.ConfusionActionsRemaining==1 && A.Players[0].Hand.IsEmpty() && A.Players[1].Hand.IsEmpty());
        }
        TestTrue(TEXT("Six opportunities terminate, three per operator"),!A.Tetris.bActive && LastBlock==6 && WhiteOperators==3 && BlackOperators==3 && Steps<500);
        TestTrue(TEXT("Clearing precedes ordinary evaluation; unblocked sequence resumes"),A.Result.Status==EMatchStatus::InProgress && CanPlayCards(A));
        const auto Before=A; StepTetrisGravity(A); TestTrue(TEXT("Duplicate exit is inert"),A==Before);
        for (int32 I=0; I<361; ++I)
        {
            const auto P=FBoard::ToCoordinate(I);
            if (A.Board.At(P).Stone==EStone::Empty)
            {
                ResolveAction(A,FActionRequest::Place(1,P));
                TestTrue(TEXT("Saved Confusion resumes on next ordinary placement"),A.Board.At(P).Stone==EStone::Black && A.ConfusionActionsRemaining==0);
                break;
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisClearing,"Gomokards.Phase3C.SimultaneousConnectedLineClear",TetrisFlags)
bool FTetrisClearing::RunTest(const FString&)
{
    for (EStone Color : {EStone::Black,EStone::White})
    for (FIntPoint D : {FIntPoint(1,0),FIntPoint(0,1),FIntPoint(1,1),FIntPoint(1,-1)})
    for (int32 Length : {5,7})
    {
        FBoard B;
        for (int32 I=0; I<Length; ++I) { B.At(FIntPoint(4,10)+D*I).Stone=Color; }
        B.At({18,18}).Stone=Color; B.At({2,2}).bForbidden=true;
        TestEqual(TEXT("All four directions, both colors, entire long run removed"),ClearTetrisLines(B),Length);
        for (int32 I=0; I<Length; ++I) { TestTrue(TEXT("Every qualifying cell removed simultaneously"),B.At(FIntPoint(4,10)+D*I).Stone==EStone::Empty); }
        TestTrue(TEXT("No board collapse; isolated stone and forbidden metadata persist"),B.At({18,18}).Stone==Color && B.At({2,2}).bForbidden);
    }
    FBoard B;
    for (int32 I=0; I<7; ++I) { B.At({I+4,7}).Stone=EStone::Black; B.At({7,I+4}).Stone=EStone::Black; }
    for (int32 I=0; I<6; ++I) { B.At({I,15}).Stone=EStone::White; }
    TestEqual(TEXT("Intersecting runs clear union, both colors in one pass"),ClearTetrisLines(B),19);
    TestEqual(TEXT("Second pass unnecessary and empty"),ClearTetrisLines(B),0);
    for (int32 I=0; I<6; ++I) { B.At({I,4}).Stone=EStone::Black; }
    B.Barriers.Add({2,4}); const auto Before=B;
    TestTrue(TEXT("Barrier splits visual six into nonqualifying segments"),ClearTetrisLines(B)==0 && B==Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisExit,"Gomokards.Phase3C.FinalClearAdjudicationAndReset",TetrisFlags)
bool FTetrisExit::RunTest(const FString&)
{
    auto S=TetrisFixture(); S.Tetris.BlockNumber=6; S.Tetris.Origin={8,17}; S.bCardsDisabled=true;
    for (int32 X=4; X<8; ++X) { S.Board.At({X,17}).Stone=EStone::Black; }
    S.Board.At({1,1}).bForbidden=true; S.Board.Barriers.Add({1,1});
    StepTetrisGravity(S);
    TestTrue(TEXT("Sixth block clears before exit, so completed line does not win"),!S.Tetris.bActive && S.Result.Status==EMatchStatus::InProgress);
    for (int32 X=4; X<10; ++X) { TestTrue(TEXT("Existing and newly deposited line cells treated equally"),S.Board.At({X,17}).Stone==EStone::Empty); }
    TestTrue(TEXT("Non-line deposited cells remain, no collapse"),S.Board.At({8,18}).Stone==EStone::Black && S.Board.At({9,18}).Stone==EStone::Black);
    TestTrue(TEXT("Exit keeps independent lock/effects/turn/metadata"),S.bCardsDisabled && !CanPlayCards(S) && S.ConfusionActionsRemaining==1 && S.CurrentPlayerIndex==1 && S.Board.At({1,1}).bForbidden && S.Board.Barriers.Num()==1);
    S.Reset(42); TestTrue(TEXT("Reset restores complete normal state"),S==FMatchState(42));
    // After a real lock all winning runs clear. Trusted blocked-edge fixtures exercise the
    // final evaluator's single/both/no-win branches through skipped opportunities (no lock/clear).
    for (int32 Outcome=0; Outcome<4; ++Outcome)
    {
        S=FMatchState(19); S.Board=ClosedBoard(); OpenRectangle(S.Board,3,3,13,13);
        for (int32 X=5; X<10; ++X)
        {
            if (Outcome&1) { S.Board.At({X,5}).Stone=EStone::Black; }
            if (Outcome&2) { S.Board.At({X,9}).Stone=EStone::White; }
        }
        CastTetris(S);
        TestTrue(TEXT("Skipped final opportunity evaluates whole board"),!S.Tetris.bActive && S.Result.Status==(Outcome==0 ? EMatchStatus::InProgress : Outcome==3 ? EMatchStatus::Draw : EMatchStatus::Won));
        TestTrue(TEXT("Final true winning identity or simultaneous Draw"),S.Result.WinningStone==(Outcome==1 ? EStone::Black : Outcome==2 ? EStone::White : EStone::Empty));
        TestTrue(TEXT("Immediate skip exit still exactly one card action/turn transfer"),S.CompletedActions==1 && S.CurrentPlayerIndex==1);
        const auto Before=S; TestTrue(TEXT("Post-exit mode operations inert"),!ApplyTetrisInput(S,ETetrisInput::Down) && !StepTetrisGravity(S) && S==Before);
        S.Reset(19); TestTrue(TEXT("Reset after terminal/normal exit clean"),S==FMatchState(19));
    }
    S=TetrisFixture(); S.Reset(42); TestTrue(TEXT("Mid-mode reset clears shape, operator and lifetime"),S==FMatchState(42));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisPoolAndView,"Gomokards.Phase3C.PoolShapeSamplingAndPresentation",TetrisFlags)
bool FTetrisPoolAndView::RunTest(const FString&)
{
    TestEqual(TEXT("Exactly ten playable definitions"),GetPlayableCards().Num(),10);
    TestTrue(TEXT("Tetris non-targeted definition"),FindCardDefinition(ECardId::Tetris) && !FindCardDefinition(ECardId::Tetris)->RequiresTarget());
    for (ECardId Id : {ECardId::FastDuel,ECardId::Undo,ECardId::Joker}) { TestNull(TEXT("Unsupported cards excluded"),FindCardDefinition(Id)); }
    TArray<ETetrisShape> Seen;
    for (int32 Seed=0; Seed<64; ++Seed)
    {
        FMatchState S(Seed); FRandomStream Expected(Seed);
        const auto Shape=static_cast<ETetrisShape>(Expected.RandRange(0,5)); const auto Edge=Edges[Expected.RandRange(0,3)];
        CastTetris(S); Seen.AddUnique(S.Tetris.Shape);
        TestTrue(TEXT("Exactly shape draw then eligible-edge draw, no hidden RNG"),S.Tetris.Shape==Shape && S.Tetris.Edge==Edge && S.Random.GetCurrentSeed()==Expected.GetCurrentSeed());
    }
    TestEqual(TEXT("All six shapes sampled"),Seen.Num(),6);
    const FKey Keys[]={EKeys::Up,EKeys::Down,EKeys::Left,EKeys::Right};
    for (int32 I=0; I<4; ++I)
    { TestTrue(TEXT("Slate mapping is absolute"),TetrisInputForKey(Keys[I]).GetValue()==Moves[I] && TetrisTranslation(Moves[I])==Deltas[I]); }
    TestTrue(TEXT("Space maps clockwise; unrelated key not captured"),TetrisInputForKey(EKeys::SpaceBar).GetValue()==ETetrisInput::Rotate && !TetrisInputForKey(EKeys::Escape).IsSet());
    auto S=TetrisFixture(ETetrisEdge::Left);
    const auto Label=TetrisLabel(S);
    TestTrue(TEXT("Mode HUD reports block/operator/color/edge/gravity/controls"),Label.Contains(TEXT("1 / 6")) && Label.Contains(TEXT("White controls Black")) && Label.Contains(TEXT("Spawn: Left")) && Label.Contains(TEXT("Gravity: Right")) && Label.Contains(TEXT("absolute")) && Label.Contains(TEXT("Space")));
    TestTrue(TEXT("Confusion described as saved, not Tetris placement color"),EffectLabel(S).Contains(TEXT("saved for ordinary play")) && !EffectLabel(S).Contains(TEXT("Placement color:")));
    S.Tetris={}; TestTrue(TEXT("Mode label clears on exit"),TetrisLabel(S).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisRuntimeTest,"Gomokards.Phase3C.RuntimeGravityDeadlineAndLifecycle",TetrisFlags)
bool FTetrisRuntimeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("Transient world"),World)) { return false; }
    auto* Owner=World->SpawnActor<ALocalMatchGameMode>();
    if (!TestNotNull(TEXT("Owner"),Owner)) { World->DestroyWorld(false); return false; }
    int32 Notifications=0; Owner->OnMatchChanged.AddLambda([&Notifications]{++Notifications;});
    auto Cast=[&]()
    {
        Owner->StartWithSeed(9);
        for (FIntPoint P : {FIntPoint(0,0),FIntPoint(1,0),FIntPoint(2,0),FIntPoint(18,18)})
        { TestTrue(TEXT("Natural seeded opening accepted"),Owner->Submit(FActionRequest::Place(Owner->GetMatch().Players[Owner->GetMatch().CurrentPlayerIndex].Id,P)).IsAccepted()); }
        TestTrue(TEXT("Naturally earned Tetris cast"),Owner->Submit(FActionRequest::Play(0,ECardId::Tetris)).IsAccepted());
    };
    Cast();
    TestTrue(TEXT("One gravity ticker scheduled"),Owner->TetrisTicker.IsValid() && Owner->TetrisDeadline>0);
    const auto Generation=Owner->TetrisTimerGeneration; const double Deadline=Owner->TetrisDeadline;
    const auto Before=Owner->GetMatch(); const int32 Count=Notifications;
    TestTrue(TEXT("Pre-deadline poll does not move or notify"),Owner->PollTetrisGravity(Deadline-.001,Generation) && Owner->GetMatch()==Before && Notifications==Count);
    TestTrue(TEXT("Deadline advances one cell and next deadline by 0.5s"),Owner->PollTetrisGravity(Deadline,Generation) && Owner->GetMatch().Tetris.Origin==Before.Tetris.Origin+TetrisGravity(Before.Tetris.Edge) && Owner->TetrisDeadline==Deadline+.5 && Notifications==Count+1);
    const auto After=Owner->GetMatch(); Owner->PollTetrisGravity(Deadline,Generation);
    TestTrue(TEXT("Same deadline cannot tick twice"),Owner->GetMatch()==After && Notifications==Count+1);
    for (ETetrisEdge Edge : Edges)
    for (int32 I=0; I<4; ++I)
    {
        Owner->Match=TetrisFixture(Edge); Owner->TetrisDeadline=100;
        TestTrue(TEXT("Runtime absolute manual movement accepted"),Owner->SubmitTetrisAt(Moves[I],90));
        TestEqual(TEXT("Only successful gravity-direction movement resets deadline"),Owner->TetrisDeadline,Deltas[I]==TetrisGravity(Edge) ? 90.5 : 100.0);
    }
    Owner->Match=TetrisFixture(); Owner->Match.Tetris.Origin={8,17}; Owner->TetrisDeadline=100;
    const auto Blocked=Owner->GetMatch(); const int32 BeforeReject=Notifications;
    TestTrue(TEXT("Blocked soft drop no lock/deadline/notification"),!Owner->SubmitTetrisAt(ETetrisInput::Down,90) && Owner->GetMatch()==Blocked && Owner->TetrisDeadline==100 && Notifications==BeforeReject);
    Owner->Match=TetrisFixture(); Owner->TetrisDeadline=100;
    TestTrue(TEXT("Rotation never resets deadline"),Owner->SubmitTetrisAt(ETetrisInput::Rotate,90) && Owner->TetrisDeadline==100);
    Owner->Match.Tetris.Shape=ETetrisShape::Line; Owner->Match.Tetris.Rotation=0; Owner->Match.Tetris.Origin={18,8};
    const auto RotationBlocked=Owner->GetMatch();
    TestTrue(TEXT("Rejected rotation preserves timer/state"),!Owner->SubmitTetrisAt(ETetrisInput::Rotate,90) && Owner->TetrisDeadline==100 && Owner->GetMatch()==RotationBlocked);
    Cast(); const auto OldGeneration=Owner->TetrisTimerGeneration; const auto OldDeadline=Owner->TetrisDeadline;
    Owner->NewMatch(); const auto Fresh=Owner->GetMatch();
    TestTrue(TEXT("Restart cancels/reset timer and complete state"),!Owner->TetrisTicker.IsValid() && Owner->TetrisDeadline==0 && Fresh==FMatchState(Fresh.Random.GetInitialSeed()));
    Owner->PollTetrisGravity(OldDeadline+50,OldGeneration);
    TestTrue(TEXT("Stale callback cannot change new match"),Owner->GetMatch()==Fresh);
    Cast(); const auto NewMode=Owner->GetMatch(); const auto NewDeadline=Owner->TetrisDeadline;
    Owner->PollTetrisGravity(OldDeadline+50,OldGeneration);
    TestTrue(TEXT("Stale callback cannot move a newer Tetris block"),Owner->GetMatch()==NewMode && Owner->TetrisDeadline==NewDeadline && Owner->TetrisTicker.IsValid());
    int32 Steps=0;
    while (Owner->GetMatch().Tetris.bActive && Steps++<500) { Owner->PollTetrisGravity(Owner->TetrisDeadline,Owner->TetrisTimerGeneration); }
    TestTrue(TEXT("Six blocks cancel timer on exit without sleeping"),!Owner->GetMatch().Tetris.bActive && !Owner->TetrisTicker.IsValid() && Owner->TetrisDeadline==0 && Steps<500);
    Cast(); const auto TeardownGeneration=Owner->TetrisTimerGeneration; const auto TeardownDeadline=Owner->TetrisDeadline;
    Owner->EndPlay(EEndPlayReason::Quit); const auto EndState=Owner->GetMatch();
    TestTrue(TEXT("EndPlay cancels and invalidates callback"),!Owner->TetrisTicker.IsValid() && !Owner->PollTetrisGravity(TeardownDeadline,TeardownGeneration) && Owner->GetMatch()==EndState);
    Owner->OnMatchChanged.Clear(); World->DestroyWorld(false);
    return true;
}
#endif
