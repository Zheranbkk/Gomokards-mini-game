#include "Core/MatchRules.h"
#include "Presentation/MatchPresentation.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Gomokards
{
namespace
{
constexpr EAutomationTestFlags StandardCardsFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
FPlayerId Current3(const FMatchState& S) { return S.Players[S.CurrentPlayerIndex].Id; }
FActionResult Place3(FMatchState& S, FIntPoint P) { return ResolveAction(S,FActionRequest::Place(Current3(S),P)); }
FActionResult Play3(FMatchState& S, ECardId Card, TOptional<FIntPoint> Target={})
{ return ResolveAction(S,FActionRequest::Play(Current3(S),Card,Target)); }
void Reject3(FAutomationTestBase& Test, FMatchState& S, const FActionRequest& Request, EActionError Error)
{
    const FMatchState Before=S;
    const auto Result=ResolveAction(S,Request);
    Test.TestTrue(TEXT("Expected error and no reward"),Result.Error==Error && !Result.bBlockingReward);
    Test.TestTrue(TEXT("Atomic rejection includes all metadata and RNG"),S==Before);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolarityResults,"Gomokards.Phase3A.PolarityAndDraw",StandardCardsFlags)
bool FPolarityResults::RunTest(const FString& Parameters)
{
    // Swap the two row colors to reverse which winning color a row-major scan encounters first.
    for (EStone First : {EStone::Black,EStone::White})
    {
        FMatchState S(27);
        S.Players[0].Hand={ECardId::Polarity,ECardId::Restock};
        for (int32 X=0; X<5; ++X)
        {
            S.Board.At({X,2}).Stone=X<3 ? First : OppositeStone(First);
            S.Board.At({X,3}).Stone=X<3 ? OppositeStone(First) : First;
        }
        TestTrue(TEXT("No pre-existing win"),EvaluateBoardResult(S.Board).Status==EMatchStatus::InProgress);
        const auto R=Play3(S,ECardId::Polarity,FIntPoint(3,2));
        TestTrue(TEXT("Polarity commits without placement reward"),R.IsAccepted() && !R.bBlockingReward);
        TestTrue(TEXT("Both colors draw regardless of scan order"),S.Result.Status==EMatchStatus::Draw && S.Result.WinningStone==EStone::Empty && S.Result.Decision==EDecisionReason::None);
        TestTrue(TEXT("Only played card consumed"),S.Players[0].Hand==TArray<ECardId>{ECardId::Restock});
        TestEqual(TEXT("Terminal action completes exactly once"),S.CompletedActions,uint64(1));
        TestEqual(TEXT("Terminal does not transfer"),S.CurrentPlayerIndex,0);
        TestEqual(TEXT("Polarity has no RNG"),S.Random.GetCurrentSeed(),27);
        Reject3(*this,S,FActionRequest::Place(0,{10,10}),EActionError::MatchStopped);
        Reject3(*this,S,FActionRequest::Play(0,ECardId::Restock),EActionError::MatchStopped);
        TestTrue(TEXT("Draw visibly distinct"),ResultLabel(S).StartsWith(TEXT("Draw:")));
        S.Reset(27); TestTrue(TEXT("Restart clears draw and metadata"),S==FMatchState(27));
    }
    for (EStone Winner : {EStone::Black,EStone::White})
    {
        FMatchState S;
        S.Players[0].Hand={ECardId::Polarity};
        for (int32 X=0; X<5; ++X) { S.Board.At({X,2}).Stone=X<3 ? Winner : OppositeStone(Winner); }
        Play3(S,ECardId::Polarity,FIntPoint(3,2));
        TestTrue(TEXT("Single-color card win"),S.Result.Status==EMatchStatus::Won && S.Result.WinningStone==Winner);
    }
    FMatchState S(91);
    S.Players[0].Hand={ECardId::Polarity};
    S.Board.At({17,17})={EStone::Black,true}; S.Board.At({18,17}).Stone=EStone::White;
    S.Board.At({18,18}).bForbidden=true; S.Board.Barriers.Add({17,17});
    for (FIntPoint Invalid : {FIntPoint(-1,0),FIntPoint(18,0),FIntPoint(0,18),FIntPoint(19,19)})
    { Reject3(*this,S,FActionRequest::Play(0,ECardId::Polarity,Invalid),EActionError::InvalidTarget); }
    Play3(S,ECardId::Polarity,FIntPoint(17,17));
    TestTrue(TEXT("Boundary inversion preserves empty and forbidden cells"),S.Board.At({17,17})==FCell{EStone::White,true} && S.Board.At({18,17}).Stone==EStone::Black && S.Board.At({18,18})==FCell{EStone::Empty,true});
    TestTrue(TEXT("No win continues with barrier retained"),S.Result.Status==EMatchStatus::InProgress && S.CurrentPlayerIndex==1 && S.Board.Barriers.Num()==1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfusionLifetime,"Gomokards.Phase3A.ConfusionLifetime",StandardCardsFlags)
bool FConfusionLifetime::RunTest(const FString& Parameters)
{
    FMatchState S(54);
    S.Players[0].Hand={ECardId::Confusion,ECardId::Restock};
    S.Players[1].Hand={ECardId::TacticalNuke};
    TestTrue(TEXT("Cast succeeds"),Play3(S,ECardId::Confusion).IsAccepted());
    TestEqual(TEXT("Cast leaves exactly two FUTURE successful actions"),S.ConfusionActionsRemaining,2);
    TestEqual(TEXT("Casting action completes once"),S.CompletedActions,uint64(1));
    const FMatchState Before=S;
    FTargetSelection Selection;
    TestTrue(TEXT("Nuke can target while confused"),Selection.Toggle(S,ECardId::TacticalNuke));
    Selection.Clear();
    TestTrue(TEXT("Selection/cancel consume neither duration nor RNG"),S==Before);
    Reject3(*this,S,FActionRequest::Place(1,{-1,0}),EActionError::InvalidCoordinate);
    Reject3(*this,S,FActionRequest::Play(1,ECardId::TacticalNuke),EActionError::InvalidTarget);
    Reject3(*this,S,FActionRequest::Play(1,ECardId::Restock),EActionError::CardNotOwned);
    Place3(S,{0,0});
    TestTrue(TEXT("First affected placement flips White's stone only"),S.Board.At({0,0}).Stone==EStone::Black && S.Players[1].AssignedStone==EStone::White);
    TestEqual(TEXT("First action uses one duration"),S.ConfusionActionsRemaining,1);
    Reject3(*this,S,FActionRequest::Place(0,{0,0}),EActionError::Occupied);
    Play3(S,ECardId::Restock);
    TestEqual(TEXT("Second affected action can be a card and expires"),S.ConfusionActionsRemaining,0);
    Place3(S,{2,2});
    TestTrue(TEXT("Following placement is assigned color again"),S.Board.At({2,2}).Stone==EStone::White);
    // Every successful card consumes old duration; Confusion refreshes it and Basics clears it.
    for (const auto& Definition : GetPlayableCards())
    {
        FMatchState CardState(7);
        CardState.ConfusionActionsRemaining=2;
        CardState.Players[0].Hand={Definition.Id};
        const auto R=Play3(CardState,Definition.Id,Definition.RequiresTarget() ? TOptional<FIntPoint>({5,5}) : TOptional<FIntPoint>{});
        TestTrue(TEXT("All current card types complete successfully under Confusion"),R.IsAccepted());
        TestEqual(TEXT("Old duration consumed, recast refreshes, Basics clears"),CardState.ConfusionActionsRemaining,
            Definition.Id==ECardId::Confusion ? 2 : (Definition.Id==ECardId::BackToBasics ? 0 : 1));
        TestEqual(TEXT("Card action completes once"),CardState.CompletedActions,uint64(1));
    }
    FMatchState Recast;
    Recast.Players[0].Hand={ECardId::Confusion}; Recast.Players[1].Hand={ECardId::Confusion};
    Play3(Recast,ECardId::Confusion); Play3(Recast,ECardId::Confusion);
    TestEqual(TEXT("Recast replaces active effect with two future actions"),Recast.ConfusionActionsRemaining,2);
    Place3(Recast,{2,3}); Place3(Recast,{4,6});
    TestTrue(TEXT("Both next placements flipped, then expires"),Recast.Board.At({2,3}).Stone==EStone::White && Recast.Board.At({4,6}).Stone==EStone::Black && Recast.ConfusionActionsRemaining==0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfusionIdentity,"Gomokards.Phase3A.ConfusionIdentityAndReward",StandardCardsFlags)
bool FConfusionIdentity::RunTest(const FString& Parameters)
{
    for (EStone Assigned : {EStone::Black,EStone::White})
    {
        FMatchState S;
        S.Players[0].Id=42; S.Players[1].Id=77;
        S.Players[0].AssignedStone=Assigned; S.Players[1].AssignedStone=OppositeStone(Assigned);
        S.ConfusionActionsRemaining=2;
        const EStone Actual=OppositeStone(Assigned);
        for (int32 X=1; X<5; ++X) { S.Board.At({X,5}).Stone=Actual; }
        S.Board.At({5,6}).Stone=Assigned; S.Board.At({5,7}).Stone=Actual;
        const auto R=Place3(S,{5,5});
        TestTrue(TEXT("Actual color wins and qualifying placement rewards"),R.IsAccepted() && R.bBlockingReward && S.Result.Status==EMatchStatus::Won && S.Result.WinningStone==Actual);
        TestTrue(TEXT("Reward belongs to acting player, identities unchanged"),S.Players[0].Hand.Num()==1 && S.Players[1].Hand.IsEmpty() && S.Players[0].Id==42 && S.Players[0].AssignedStone==Assigned);
        TestTrue(TEXT("Terminal action decrements old effect and never transfers"),S.ConfusionActionsRemaining==1 && S.CurrentPlayerIndex==0 && S.CompletedActions==1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBarrierTopology,"Gomokards.Phase3A.BarrierTopology",StandardCardsFlags)
bool FBarrierTopology::RunTest(const FString& Parameters)
{
    int32 Valid=0;
    for (int32 Y=0; Y<FBoard::Size; ++Y)
    for (int32 X=0; X<FBoard::Size; ++X)
    {
        const FIntPoint Anchor(X,Y);
        if (!FBoard::ContainsAnchor(Anchor)) { continue; }
        ++Valid;
        FBoard B; B.Barriers.Add(Anchor);
        const auto Corners=FBoard::RegionCorners(Anchor);
        for (int32 I=0; I<4; ++I)
        for (int32 J=I+1; J<4; ++J)
        { TestTrue(TEXT("All six links blocked in both directions at every valid cell"),B.IsLinkBlocked(Corners[I],Corners[J]) && B.IsLinkBlocked(Corners[J],Corners[I])); }
        TestFalse(TEXT("Unrelated link left of the cell is not blocked"),B.IsLinkBlocked(Anchor-FIntPoint(1,0),Anchor));
        TestFalse(TEXT("Unrelated link above the cell is not blocked"),B.IsLinkBlocked(Anchor-FIntPoint(0,1),Anchor));
    }
    TestEqual(TEXT("Exactly 18x18 valid anchors"),Valid,324);
    FMatchState S(23); S.Players[0].Hand={ECardId::Barrier,ECardId::Barrier}; S.Players[1].Hand={ECardId::Barrier};
    for (FIntPoint Invalid : {FIntPoint(-1,0),FIntPoint(0,-1),FIntPoint(18,0),FIntPoint(0,18)})
    { Reject3(*this,S,FActionRequest::Play(0,ECardId::Barrier,Invalid),EActionError::InvalidTarget); }
    S.Board.At({0,0}).Stone=EStone::White;
    const auto R=Play3(S,ECardId::Barrier,FIntPoint(0,0));
    TestTrue(TEXT("Barrier never removes stone or rewards"),R.IsAccepted() && !R.bBlockingReward && S.Board.At({0,0}).Stone==EStone::White);
    Play3(S,ECardId::Barrier,FIntPoint(17,17));
    TestEqual(TEXT("Multiple boundary barriers coexist"),S.Board.Barriers.Num(),2);
    Play3(S,ECardId::Barrier,FIntPoint(0,0));
    TestTrue(TEXT("Repeated deployment validly consumes action with one stored anchor"),S.Board.Barriers.Num()==2 && S.CompletedActions==3 && S.Players[0].Hand.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBarrierWins,"Gomokards.Phase3A.BarrierWinningLines",StandardCardsFlags)
bool FBarrierWins::RunTest(const FString& Parameters)
{
    const FIntPoint TestLineDirections[]={{1,0},{0,1},{1,1},{1,-1}};
    const FIntPoint Anchors[]={{6,9},{5,10},{6,10},{6,7}};
    for (int32 D=0; D<4; ++D)
    for (EStone Color : {EStone::Black,EStone::White})
    {
        FMatchState S;
        S.CurrentPlayerIndex=Color==EStone::Black ? 0 : 1;
        for (int32 I=0; I<4; ++I) { S.Board.At(FIntPoint(5,9)+TestLineDirections[D]*I).Stone=Color; }
        S.Board.Barriers.Add(Anchors[D]);
        Place3(S,FIntPoint(5,9)+TestLineDirections[D]*4);
        TestTrue(TEXT("Barrier prevents placement win in each direction/color"),S.Result.Status==EMatchStatus::InProgress);
        TestTrue(TEXT("Global scan respects barriers"),EvaluateBoardResult(S.Board).Status==EMatchStatus::InProgress);
        S.Board.Barriers.Empty();
        TestTrue(TEXT("Same stones win when connectivity restored"),EvaluateBoardResult(S.Board).WinningStone==Color);
    }
    FMatchState Block;
    Block.Board.At({6,5}).Stone=EStone::White; Block.Board.At({7,5}).Stone=EStone::Black;
    Block.Board.Barriers.Add({5,5});
    TestTrue(TEXT("Legacy blocking reward intentionally ignores barriers"),Place3(Block,{5,5}).bBlockingReward);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBasicsLock,"Gomokards.Phase3A.BackToBasics",StandardCardsFlags)
bool FBasicsLock::RunTest(const FString& Parameters)
{
    FMatchState S(65);
    S.Players[0].Hand={ECardId::TacticalNuke,ECardId::Barrier,ECardId::BackToBasics,ECardId::Restock};
    S.Players[1].Hand={ECardId::Polarity,ECardId::Confusion,ECardId::Steal};
    S.Board.At({1,1}).Stone=EStone::Black; S.Board.At({5,5}).Stone=EStone::White;
    Play3(S,ECardId::TacticalNuke,FIntPoint(1,1));
    Play3(S,ECardId::Polarity,FIntPoint(5,5));
    Play3(S,ECardId::Barrier,FIntPoint(8,8));
    Play3(S,ECardId::Confusion);
    S.Board.Barriers.Add({10,10}); S.Board.At({18,18}).bForbidden=true;
    TestEqual(TEXT("Basics starts with two active Confusion actions"),S.ConfusionActionsRemaining,2);
    const FBoard Before=S.Board;
    const int32 Seed=S.Random.GetCurrentSeed();
    const auto R=Play3(S,ECardId::BackToBasics);
    TestTrue(TEXT("Basics succeeds without reward"),R.IsAccepted() && !R.bBlockingReward);
    TestTrue(TEXT("Lock enabled; all board cells, flags and barriers preserved"),S.bCardsDisabled && S.Board==Before);
    TestEqual(TEXT("Basics clears active Confusion"),S.ConfusionActionsRemaining,0);
    TestTrue(TEXT("Actual Nuke removal and Polarity flip not undone"),S.Board.At({1,1})==FCell{EStone::Empty,true} && S.Board.At({5,5}).Stone==EStone::Black);
    TestTrue(TEXT("Unused hands retained; only Basics consumed"),S.Players[0].Hand==TArray<ECardId>{ECardId::Restock} && S.Players[1].Hand==TArray<ECardId>{ECardId::Steal});
    TestTrue(TEXT("Exactly one completion/transfer for Basics"),S.CompletedActions==5 && S.CurrentPlayerIndex==1);
    TestEqual(TEXT("Basics has no RNG"),S.Random.GetCurrentSeed(),Seed);
    for (const auto& Def : GetPlayableCards())
    { Reject3(*this,S,FActionRequest::Play(Current3(S),Def.Id),EActionError::CardsDisabled); }
    Reject3(*this,S,FActionRequest::Place(Current3(S),{1,1}),EActionError::Forbidden);
    Reject3(*this,S,FActionRequest::Place(Current3(S),{5,5}),EActionError::Occupied);
    TestTrue(TEXT("Next valid placement uses assigned color after Basics clears Confusion"),Place3(S,{8,7}).IsAccepted() && S.Board.At({8,7}).Stone==EStone::White);
    TestEqual(TEXT("Cleared Confusion stays zero on placement"),S.ConfusionActionsRemaining,0);
    TestTrue(TEXT("Following placement uses normal assigned color"),Place3(S,{9,7}).IsAccepted() && S.Board.At({9,7}).Stone==EStone::Black);
    S.Reset(65);
    TestTrue(TEXT("Restart clears entire state and card lock"),S==FMatchState(65));
    S.Players[0].Hand.Add(ECardId::Restock);
    TestTrue(TEXT("Restart restores card play"),Play3(S,ECardId::Restock).IsAccepted() && !S.bCardsDisabled);

    FMatchState Blocked;
    for (int32 X=0; X<4; ++X) { Blocked.Board.At({X,9}).Stone=EStone::White; }
    Blocked.Board.Barriers.Add({1,9});
    Blocked.Players[0].Hand={ECardId::BackToBasics};
    Play3(Blocked,ECardId::BackToBasics);
    Place3(Blocked,{4,9});
    TestTrue(TEXT("Persistent Barrier still prevents a five-stone win after Basics"),Blocked.Result.Status==EMatchStatus::InProgress && Blocked.Board.IsLinkBlocked({1,9},{2,9}) && !HasWinningLine(Blocked.Board,{4,9},EStone::White));

    // Trusted fixtures intentionally contain pre-existing wins: detect any accidental global scan.
    for (bool Both : {false,true})
    {
        FMatchState Existing;
        for (int32 X=0; X<5; ++X)
        { Existing.Board.At({X,2}).Stone=EStone::Black; if (Both) { Existing.Board.At({X,5}).Stone=EStone::White; } }
        Existing.Players[0].Hand={ECardId::BackToBasics};
        TestTrue(TEXT("Fixture would produce a global winner/draw"),EvaluateBoardResult(Existing.Board).Status==(Both ? EMatchStatus::Draw : EMatchStatus::Won));
        const FBoard ExistingBoard=Existing.Board;
        Play3(Existing,ECardId::BackToBasics);
        TestTrue(TEXT("Basics never triggers a global winner or Draw scan"),Existing.Result.Status==EMatchStatus::InProgress && Existing.Board==ExistingBoard && Existing.CurrentPlayerIndex==1 && Existing.CompletedActions==1);
    }

    FMatchState Reward(17);
    Reward.Players[0].Hand={ECardId::BackToBasics};
    Reward.Board.At({6,5}).Stone=EStone::Black; Reward.Board.At({7,5}).Stone=EStone::White;
    Reward.Board.Barriers.Add({5,5});
    Play3(Reward,ECardId::BackToBasics);
    TestTrue(TEXT("Future block still rewards exactly once despite card lock"),Place3(Reward,{5,5}).bBlockingReward && Reward.Players[1].Hand.Num()==1 && Reward.bCardsDisabled);
    Place3(Reward,{12,12});
    Reject3(*this,Reward,FActionRequest::Play(Current3(Reward),Reward.Players[1].Hand[0]),EActionError::CardsDisabled);

    // Inert cards must not masquerade as legal actions on a full non-winning board.
    FMatchState Full;
    for (int32 Y=0; Y<19; ++Y) for (int32 X=0; X<19; ++X)
    { Full.Board.At({X,Y}).Stone=(X+2*Y)%4<2 ? EStone::Black : EStone::White; }
    Full.Players[0].Hand={ECardId::BackToBasics}; Full.Players[1].Hand={ECardId::TacticalNuke};
    Play3(Full,ECardId::BackToBasics);
    TestTrue(TEXT("Full board with disabled cards remains an unresolved rule, not a draw"),Full.Result.Status==EMatchStatus::AwaitingRuleDecision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPool3,"Gomokards.Phase3A.PoolAndDeterminism",StandardCardsFlags)
bool FPool3::RunTest(const FString& Parameters)
{
    const TArray<ECardId> Expected={ECardId::Restock,ECardId::SwapHands,ECardId::Steal,ECardId::TacticalNuke,ECardId::Polarity,ECardId::Confusion,ECardId::Barrier,ECardId::BackToBasics,ECardId::Ghost,ECardId::Tetris};
    TArray<ECardId> Actual;
    for (const auto& Def : GetPlayableCards()) { Actual.Add(Def.Id); }
    TestTrue(TEXT("Pool exactly the ten approved IDs, with no duplicate entries"),Actual==Expected);
    TArray<ECardId> Seen;
    for (int32 Seed=0; Seed<128; ++Seed)
    {
        FMatchState A(Seed),B(Seed);
        A.Players[0].Hand=B.Players[0].Hand={ECardId::Restock};
        Play3(A,ECardId::Restock); Play3(B,ECardId::Restock);
        TestTrue(TEXT("Expanded pool preserves seeded equality"),A==B);
        FRandomStream ExpectedRandom(Seed);
        const ECardId First=Expected[ExpectedRandom.RandRange(0,9)], Second=Expected[ExpectedRandom.RandRange(0,9)];
        TestTrue(TEXT("Two uniform-with-replacement choices"),A.Players[0].Hand==TArray<ECardId>{First,Second});
        for (ECardId Card : A.Players[0].Hand) { TestTrue(TEXT("No unsupported draw"),Expected.Contains(Card)); Seen.AddUnique(Card); }
    }
    TestEqual(TEXT("All ten generated in sample"),Seen.Num(),10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetDomains3,"Gomokards.Phase3A.PresentationDomains",StandardCardsFlags)
bool FTargetDomains3::RunTest(const FString& Parameters)
{
    for (int32 Y=0; Y<18; ++Y) for (int32 X=0; X<18; ++X)
    {
        const FIntPoint Anchor(X,Y);
        const auto Cross=FBoardLayout::BarrierCross(Anchor);
        const FVector2D Center=(Cross[0]+Cross[1])*.5;
        TestTrue(TEXT("Every rendered cell center hits its authoritative anchor"),FBoardLayout::TargetAt(Center,ECardId::Barrier).GetValue()==Anchor);
        TestTrue(TEXT("Cross endpoints agree on same center"),(Cross[2]+Cross[3])*.5==Center);
        const FVector2D TopLeft=FBoardLayout::Center(Anchor);
        const float Extension=FBoardLayout::CellSize*.125f;
        TestTrue(TEXT("All four visual arms overhang their nearest grid line by 12.5 percent"),
            FMath::IsNearlyEqual(TopLeft.X-Cross[0].X,double(Extension)) &&
            FMath::IsNearlyEqual(Cross[1].X-(TopLeft.X+FBoardLayout::CellSize),double(Extension)) &&
            FMath::IsNearlyEqual(TopLeft.Y-Cross[2].Y,double(Extension)) &&
            FMath::IsNearlyEqual(Cross[3].Y-(TopLeft.Y+FBoardLayout::CellSize),double(Extension)));
        TestTrue(TEXT("Polarity anchor is the top-left intersection"),FBoardLayout::TargetAt(FBoardLayout::Center(Anchor),ECardId::Polarity).GetValue()==Anchor);
    }
    TestFalse(TEXT("Outside top-left cell border rejected"),FBoardLayout::TargetAt({14.99,30},ECardId::Barrier).IsSet());
    TestFalse(TEXT("Outside last cell border rejected"),FBoardLayout::TargetAt({555,30},ECardId::Barrier).IsSet());
    TestFalse(TEXT("Polarity last-column anchor rejected"),FBoardLayout::TargetAt(FBoardLayout::Center({18,0}),ECardId::Polarity).IsSet());
    TestTrue(TEXT("Nuke still accepts last intersection"),FBoardLayout::TargetAt(FBoardLayout::Center({18,18}),ECardId::TacticalNuke).GetValue()==FIntPoint(18,18));
    for (ECardId Card : {ECardId::Polarity,ECardId::Barrier,ECardId::TacticalNuke})
    {
        FMatchState S(99); S.ConfusionActionsRemaining=2; S.Players[0].Hand={Card};
        const FMatchState Before=S;
        FTargetSelection Selection;
        TestTrue(TEXT("Select supported targeted card"),Selection.Toggle(S,Card));
        const auto Intent=Selection.BoardRequest(S,{3,4});
        TestTrue(TEXT("Stable ID/owner/target passed unchanged"),Intent.Card==Card && Intent.Player==0 && Intent.Target.GetValue()==FIntPoint(3,4));
        Selection.Toggle(S,Card);
        TestTrue(TEXT("Reselect cancels without gameplay/RNG mutation"),!Selection.IsActive() && S==Before);
        S.bCardsDisabled=true;
        TestFalse(TEXT("Card lock prevents local targeting"),Selection.Toggle(S,Card));
    }
    FMatchState Labels;
    Labels.ConfusionActionsRemaining=2;
    TestTrue(TEXT("Confusion duration and effective color visible"),EffectLabel(Labels).Contains(TEXT("2 successful")) && EffectLabel(Labels).Contains(TEXT("White")));
    Labels.bCardsDisabled=true;
    TestTrue(TEXT("Card lock and continuing Confusion both visible"),EffectLabel(Labels).Contains(TEXT("cards disabled")) && EffectLabel(Labels).Contains(TEXT("2 successful")));
    TestTrue(TEXT("Barrier instructions explicitly name center"),TargetingLabel(ECardId::Barrier).Contains(TEXT("CENTER")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualReplay3,"Gomokards.Phase3A.ReachableManualSetups",StandardCardsFlags)
bool FManualReplay3::RunTest(const FString& Parameters)
{
    // Replay documentation setups exclusively through public actions, with naturally earned cards.
    FMatchState Draw(4);
    for (FIntPoint P : {FIntPoint(0,0),FIntPoint(1,0),FIntPoint(2,0),FIntPoint(18,18)})
    { TestTrue(TEXT("Opening legal"),Place3(Draw,P).IsAccepted()); }
    TestTrue(TEXT("Seed 4 opening earns Polarity for Black"),Draw.Players[0].Hand==TArray<ECardId>{ECardId::Polarity});
    for (FIntPoint P : {FIntPoint(0,2),FIntPoint(0,3),FIntPoint(1,2),FIntPoint(1,3),FIntPoint(2,2),FIntPoint(2,3),FIntPoint(3,3),FIntPoint(3,2),FIntPoint(4,3),FIntPoint(4,2)})
    { TestTrue(TEXT("Draw setup remains playable"),Place3(Draw,P).IsAccepted() && Draw.Result.Status==EMatchStatus::InProgress); }
    TestTrue(TEXT("Documented action creates reachable draw"),Play3(Draw,ECardId::Polarity,FIntPoint(3,2)).IsAccepted() && Draw.Result.Status==EMatchStatus::Draw);
    TestEqual(TEXT("Documented draw has fifteen completed actions"),Draw.CompletedActions,uint64(15));
    FMatchState Confused(7);
    for (FIntPoint P : {FIntPoint(0,0),FIntPoint(1,0),FIntPoint(2,0),FIntPoint(18,18)}) { Place3(Confused,P); }
    TestTrue(TEXT("Seed 7 naturally earns Confusion"),Play3(Confused,ECardId::Confusion).IsAccepted());
    Place3(Confused,{5,5}); Place3(Confused,{7,7}); Place3(Confused,{9,9});
    TestTrue(TEXT("Documented effective-color sequence"),Confused.Board.At({5,5}).Stone==EStone::Black && Confused.Board.At({7,7}).Stone==EStone::White && Confused.Board.At({9,9}).Stone==EStone::White && Confused.ConfusionActionsRemaining==0);
    return true;
}
}
#endif
