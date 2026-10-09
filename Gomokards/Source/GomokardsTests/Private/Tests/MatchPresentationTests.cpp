#include "Legacy/LegacyRules.h"
#include "Legacy/LegacyPresentation.h"
#include "Presentation/MatchPresentation.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Gomokards
{
namespace
{
constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardInput, "Gomokards.LegacyFixtures.Phase2.BoardCoordinates", Flags)
bool FBoardInput::RunTest(const FString& Parameters)
{
    for (int32 Y=0; Y<FBoard::Size; ++Y)
    for (int32 X=0; X<FBoard::Size; ++X)
    {
        const FIntPoint P(X,Y);
        const auto Hit = FBoardLayout::ToCoordinate(FBoardLayout::Center(P));
        TestTrue(TEXT("Every painted center maps to its logical point"), Hit.IsSet() && Hit.GetValue()==P);
    }
    TestTrue(TEXT("Origin belongs to first point"), FBoardLayout::ToCoordinate({0,0}).GetValue()==FIntPoint(0,0));
    TestTrue(TEXT("Last pixel belongs to last point"), FBoardLayout::ToCoordinate({569.99,569.99}).GetValue()==FIntPoint(18,18));
    TestFalse(TEXT("Negative outside does not clamp"), FBoardLayout::ToCoordinate({-.01,0}).IsSet());
    TestFalse(TEXT("Right edge outside"), FBoardLayout::ToCoordinate({570,0}).IsSet());
    TestFalse(TEXT("Bottom edge outside"), FBoardLayout::ToCoordinate({0,570}).IsSet());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetInput, "Gomokards.LegacyFixtures.Phase2.TargetingIntent", Flags)
bool FTargetInput::RunTest(const FString& Parameters)
{
    FMatchState State(103);
    State.Players[0].Hand.Add(ECardId::TacticalNuke);
    const FMatchState Before = State;
    FTargetSelection Selection;
    TestTrue(TEXT("Select owned Nuke"), Selection.Toggle(State,ECardId::TacticalNuke));
    TestTrue(TEXT("Selection does not mutate gameplay or RNG"), State==Before);
    TestTrue(TEXT("Reselect cancels"), Selection.Toggle(State,ECardId::TacticalNuke) && !Selection.IsActive());
    Selection.Toggle(State,ECardId::TacticalNuke);
    Selection.Clear();
    TestTrue(TEXT("Cancel does not mutate gameplay or RNG"), State==Before && !Selection.IsActive());
    Selection.Toggle(State,ECardId::TacticalNuke);
    const auto Rejected = ResolveAction(State,Selection.BoardRequest(State,{-1,0}));
    TestFalse(TEXT("Invalid target rejected"), Rejected.IsAccepted());
    TestTrue(TEXT("Rejected target preserves match and local selection"), State==Before && Selection.IsActive());
    TestTrue(TEXT("Complete targeted request accepted"), ResolveAction(State,Selection.BoardRequest(State,{3,4})).IsAccepted());
    const FMatchState After = State;
    TestFalse(TEXT("Stale targeting cannot act as next player"), ResolveAction(State,Selection.BoardRequest(State,{4,4})).IsAccepted());
    TestTrue(TEXT("Stale request atomic"), State==After);
    Selection.Clear(); // The view clears local intent on every accepted change/new match.
    const auto Place = Selection.BoardRequest(State,{5,6});
    TestTrue(TEXT("Unselected board intent uses authoritative current player"), Place.Type==EActionType::PlaceStone && Place.Player==State.Players[State.CurrentPlayerIndex].Id && Place.Coordinate==FIntPoint(5,6));
    State.Result.Status=EMatchStatus::AwaitingRuleDecision;
    TestTrue(TEXT("Unresolved state explicitly named"), ResultLabel(State).Contains(TEXT("Awaiting rule decision")));
    TestFalse(TEXT("Stopped match cannot enter targeting"), Selection.Toggle(State,ECardId::TacticalNuke));
    State.Result.Status=EMatchStatus::Won; State.Result.WinningStone=EStone::White;
    TestTrue(TEXT("Winner label follows result"), ResultLabel(State).StartsWith(TEXT("White wins")));
    return true;
}
}
#endif
