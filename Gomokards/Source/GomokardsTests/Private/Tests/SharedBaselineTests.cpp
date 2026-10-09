#include "Core/BoardEffects.h"
#include "Core/MatchRules.h"
#include "Presentation/MatchPresentation.h"
#include "Presentation/DemoPresentation.h"
#include "Presentation/SDemoCards.h"
#include "Presentation/SMatchBoard.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/ConfigCacheIni.h"
#include "Fonts/CompositeFont.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace Gomokards;
namespace { constexpr EAutomationTestFlags SharedFlags=EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedPlacement,"Gomokards.Shared.PlacementWithoutReward",SharedFlags)
bool FSharedPlacement::RunTest(const FString&)
{
    FMatchState State(5751);
    State.Players[0].Id=71; State.Players[1].Id=32;
    State.Board.At({1,0}).Stone=EStone::White;
    State.Board.At({2,0}).Stone=EStone::Black;
    const auto Before=State;
    const auto Result=TryPlaceStone(State.Board,{0,0},EStone::Black);
    TestTrue(TEXT("Successful block exposed without resource/turn policy"),Result.IsAccepted() && Result.bSuccessfulBlock && !Result.bWinningLine);
    auto Expected=Before; Expected.Board.At({0,0}).Stone=EStone::Black;
    TestTrue(TEXT("Only board changes; no hand reward, identity/turn/effect/RNG mutation"),State==Expected);
    State.Board.At({4,4}).bForbidden=true;
    for (const auto Point : {FIntPoint(-1,0),FIntPoint(19,0),FIntPoint(0,0),FIntPoint(4,4)})
    {
        const auto Snapshot=State;
        TestFalse(TEXT("Invalid placement rejected"),TryPlaceStone(State.Board,Point,EStone::Black).IsAccepted());
        TestTrue(TEXT("Entire state and RNG preserved"),State==Snapshot);
    }
    const auto Snapshot=State;
    TestFalse(TEXT("Empty is not a placement stone"),TryPlaceStone(State.Board,{5,5},EStone::Empty).IsAccepted());
    TestFalse(TEXT("Malformed stone rejected"),TryPlaceStone(State.Board,{5,5},static_cast<EStone>(255)).IsAccepted());
    TestTrue(TEXT("Invalid stone atomic"),State==Snapshot);
    FBoard Win; for (int32 X=0;X<4;++X) { Win.At({X,8}).Stone=EStone::Black; }
    TestTrue(TEXT("Shared placement reports win"),TryPlaceStone(Win,{4,8},EStone::Black).bWinningLine);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedEffects,"Gomokards.Shared.BoardEffectsWithoutHand",SharedFlags)
bool FSharedEffects::RunTest(const FString&)
{
    FBoard Board; Board.At({4,4}).Stone=EStone::Black; Board.At({5,5}).Stone=EStone::White;
    TestTrue(TEXT("Nuke has no actor/hand prerequisite"),ApplyTacticalNuke(Board,{4,4}));
    TestTrue(TEXT("Nuke empties and permanently forbids"),Board.At({4,4})==FCell{EStone::Empty,true});
    TestTrue(TEXT("Barrier is unique and non-destructive"),ApplyBarrier(Board,{4,4}) && ApplyBarrier(Board,{4,4}) && Board.Barriers.Num()==1);
    const auto Before=Board;
    TestTrue(TEXT("Polarity applies without card ownership"),ApplyPolarity(Board,{4,4}));
    auto Expected=Before; Expected.At({5,5}).Stone=EStone::Black;
    TestTrue(TEXT("Only colors flip; empty forbidden cell and barrier history persist"),Board==Expected);
    TestTrue(TEXT("Barrier blocks six links"),Board.IsLinkBlocked({4,4},{5,5}) && Board.IsLinkBlocked({4,4},{5,4}));
    for (const auto Point : {FIntPoint(-1,0),FIntPoint(19,19)})
    {
        const auto Snapshot=Board;
        TestFalse(TEXT("Invalid Nuke rejected"),ApplyTacticalNuke(Board,Point));
        TestFalse(TEXT("Invalid Polarity rejected"),ApplyPolarity(Board,Point));
        TestFalse(TEXT("Invalid Barrier rejected"),ApplyBarrier(Board,Point));
        TestTrue(TEXT("All invalid effects atomic"),Board==Snapshot);
    }
    TestFalse(TEXT("Last intersection cannot anchor 2x2"),ApplyPolarity(Board,{18,18}) || ApplyBarrier(Board,{18,18}));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedTargetGeometry,"Gomokards.Shared.TargetGeometry",SharedFlags)
bool FSharedTargetGeometry::RunTest(const FString&)
{
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
    const double NaN=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Nonfinite pointer rejected"),FBoardLayout::TargetAt({NaN,30},ECardId::Barrier).IsSet());
    TestFalse(TEXT("Nonfinite pointer rejected for intersection"),FBoardLayout::ToCoordinate({30,NaN}).IsSet());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedAssets,"Gomokards.Shared.FontArtAndStartupAssets",SharedFlags)
bool FSharedAssets::RunTest(const FString&)
{
    TSet<FString> Paths;
    for (const auto& Reference : CardArtReferences())
    {
        Paths.Add(Reference.Path);
        auto* Texture=LoadObject<UTexture2D>(nullptr,Reference.Path);
        if (TestNotNull(TEXT("Historical placeholder texture loads"),Texture))
        {
            TestTrue(TEXT("Embedded cookable source"),Texture->Source.IsValid());
            TestEqual(TEXT("Placeholder width"),Texture->Source.GetSizeX(),int64(4));
            TestEqual(TEXT("Placeholder height"),Texture->Source.GetSizeY(),int64(4));
        }
    }
    TestEqual(TEXT("Ten distinct historical art paths, not a playable catalog"),Paths.Num(),10);
    TestNotNull(TEXT("Card back retained"),LoadObject<UTexture2D>(nullptr,DemoCardBackPath()));
    FDemoCardArt Art;
    TestNull(TEXT("Unknown art is safely absent"),Art.Brush(255));
    const auto Font=DemoFont(18);
    TestTrue(TEXT("CJK fallback remains in actual font configuration"),Font.GetCompositeFont()->FallbackTypeface.Typeface.Fonts.Num()>0);
    TestTrue(TEXT("Bundled CJK font exists"),IFileManager::Get().FileExists(*(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"))));
    FString Mode;
    GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"),TEXT("GlobalDefaultGameMode"),Mode,GEngineIni);
    TestEqual(TEXT("Empty engine startup owner, no project runtime"),Mode,FString(TEXT("/Script/Engine.GameModeBase")));
    auto* Map=LoadObject<UWorld>(nullptr,TEXT("/Game/Maps/LocalMatch.LocalMatch"));
    if (TestNotNull(TEXT("Unchanged startup map loads after removal"),Map))
    {
        const auto* Settings=Map->GetWorldSettings();
        TestTrue(TEXT("Map has no removed project GameMode override"),Settings && (!Settings->DefaultGameMode || Settings->DefaultGameMode==AGameModeBase::StaticClass()));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedWidgets,"Gomokards.Shared.PresentationIntentCallbacks",SharedFlags)
bool FSharedWidgets::RunTest(const FString&)
{
    FBoardPresentation Display; FIntPoint Click(-1,-1); int32 Cancels=0;
    auto Widget=SNew(SMatchBoard).Display(Display)
        .OnBoardClicked_Lambda([&](FIntPoint P){Click=P;}).OnCancel_Lambda([&](){++Cancels;});
    const auto Geometry=FGeometry::MakeRoot(FVector2D(570),FSlateLayoutTransform());
    const FPointerEvent Left(0,FVector2D(45,75),FVector2D(45,75),TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
    Widget->OnMouseButtonDown(Geometry,Left);
    TestTrue(TEXT("Board sends normalized intent only"),Click==FIntPoint(1,2) && Display.Board==FBoard());
    const FPointerEvent Right(0,FVector2D(45,75),FVector2D(45,75),TSet<FKey>{EKeys::RightMouseButton},EKeys::RightMouseButton,0,FModifierKeysState());
    Widget->OnMouseButtonDown(Geometry,Right);
    TestEqual(TEXT("Cancellation forwarded without gameplay state"),Cancels,1);
    int32 CardIndex=INDEX_NONE;
    TArray<FDemoCardPresentation> Cards={{uint8(ECardId::Polarity),TEXT("两极反转"),TEXT("显示数据"),true}};
    auto Hand=SNew(SDemoHand).Cards(Cards).OnCardClicked_Lambda([&](int32 Index){CardIndex=Index;});
    const FPointerEvent CardClick(0,FVector2D(400,100),FVector2D(400,100),TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
    Hand->OnMouseButtonDown(FGeometry::MakeRoot(FVector2D(800,244),FSlateLayoutTransform()),CardClick);
    TestEqual(TEXT("Hand forwards exact slot, with no controller/ownership lookup"),CardIndex,0);
    TestEqual(TEXT("Supplied display list unchanged"),Cards.Num(),1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedTetrisStep,"Gomokards.Shared.TetrisPhysicalStep",SharedFlags)
bool FSharedTetrisStep::RunTest(const FString&)
{
    FMatchState State(5751);
    State.Tetris.bActive=true; State.Tetris.Shape=ETetrisShape::Square;
    State.Tetris.Edge=ETetrisEdge::Top; State.Tetris.Origin={8,16}; State.Tetris.Stone=EStone::White;
    State.Board.Barriers.Add({1,1}); State.Board.At({3,3}).bForbidden=true;
    const auto Before=State;
    TestTrue(TEXT("Physical gravity moves when clear"),StepTetrisPiece(State)==ETetrisStep::Moved);
    auto Moved=Before; Moved.Tetris.Origin={8,17};
    TestTrue(TEXT("Move changes origin only"),State==Moved);
    TestTrue(TEXT("Blocked automatic gravity locks"),StepTetrisPiece(State)==ETetrisStep::Locked);
    auto Locked=Moved; Locked.Tetris.bActive=false;
    for (auto Offset : TetrisOffsets(ETetrisShape::Square)) { Locked.Board.At(FIntPoint(8,17)+Offset).Stone=EStone::White; }
    TestTrue(TEXT("Lock changes cells/active flag only: no next actor, RNG, action or card policy"),State==Locked);
    TestTrue(TEXT("Inactive piece cannot lock twice"),StepTetrisPiece(State)==ETetrisStep::Rejected && State==Locked);
    TestFalse(TEXT("Inactive piece rejects manual input"),ApplyTetrisInput(State,ETetrisInput::Left));
    return true;
}
#endif
