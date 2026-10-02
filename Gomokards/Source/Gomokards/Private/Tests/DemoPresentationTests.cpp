#include "Presentation/DemoPresentation.h"
#include "Presentation/SDemoCards.h"
#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchGameState.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags DemoPresentationTestsFlags=EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoCardTest,"Gomokards.DemoUI.ChineseCardsAndCookableAssets",DemoPresentationTestsFlags)
bool FDemoCardTest::RunTest(const FString&)
{
    using namespace Gomokards;
    const TCHAR* Names[]={TEXT("补充库存"),TEXT("战术换家"),TEXT("取之有道"),TEXT("战术核弹"),TEXT("两极反转"),TEXT("定位混淆"),TEXT("阴阳屏障"),TEXT("回归基本功"),TEXT("幽灵棋子"),TEXT("俄罗斯方块")};
    TestEqual(TEXT("Exactly ten existing presentation entries"),DemoCards().Num(),10);
    TSet<FString> Art;
    for (int32 I=0;I<10;++I)
    {
        const auto* Card=DemoCard(uint8(I+1));
        if (!TestNotNull(TEXT("Stable gameplay ID maps to presentation"),Card)) { continue; }
        TestEqual(TEXT("Frozen Simplified Chinese name"),FString(Card->Name),FString(Names[I]));
        TestFalse(TEXT("Chinese description is nonempty"),FString(Card->Description).IsEmpty());
        TestTrue(TEXT("Description contains CJK text"),FString(Card->Description).Contains(TEXT("。")));
        TestTrue(TEXT("Stable project art path"),FString(Card->ArtPath).StartsWith(TEXT("/Game/UI/Cards/T_CardArt_")));
        Art.Add(Card->ArtPath);
        auto* Texture=LoadObject<UTexture2D>(nullptr,Card->ArtPath);
        if (TestNotNull(TEXT("Card art loads as an Unreal texture without source PNG"),Texture))
        {
#if WITH_EDITORONLY_DATA
            // NullRHI has no render resource; validate the embedded, cookable source instead.
            TestTrue(TEXT("Embedded texture source is valid"),Texture->Source.IsValid());
            TestEqual(TEXT("Neutral placeholder width"),Texture->Source.GetSizeX(),int64(4));
            TestEqual(TEXT("Neutral placeholder height"),Texture->Source.GetSizeY(),int64(4));
#endif
        }
    }
    TestEqual(TEXT("Each card has its own stable art reference"),Art.Num(),10);
    TestNotNull(TEXT("Separate cooked card back exists"),LoadObject<UTexture2D>(nullptr,DemoCardBackPath()));
    TestNull(TEXT("Unknown cards do not invent presentation"),DemoCard(255));
    TestTrue(TEXT("Bundled CJK font exists independently of installed Windows fonts"),IFileManager::Get().FileExists(*(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"))));
    FMatchState State; State.Players[0].Id=71; State.Players[1].Id=32;
    State.Players[0].Hand={ECardId::Restock}; State.Players[1].Hand={ECardId::Ghost,ECardId::Steal};
    const auto Public=MakePublicView(State,{71,32},EMatchSession::Playing,1,1);
    TestEqual(TEXT("Opponent hand construction requires public count only"),DemoOpponentCount(Public,71),2);
    TestEqual(TEXT("Unassigned viewer cannot select an opponent hand"),DemoOpponentCount(Public,INDEX_NONE),0);
    TestEqual(TEXT("Turn resolves assigned Black identity, not ID zero"),DemoPlayerStone(Public,Public.CurrentPlayerId),uint8(1));
    TestEqual(TEXT("Reversed/nonzero White identity maps correctly"),DemoPlayerStone(Public,32),uint8(2));
    TestEqual(TEXT("Chinese side labels"),DemoSide(2),FString(TEXT("白方")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoLogTest,"Gomokards.DemoUI.LogPrivacyAndDisplayPriority",DemoPresentationTestsFlags)
bool FDemoLogTest::RunTest(const FString&)
{
    using namespace Gomokards;
    FMatchState State;
    auto P=MakePublicView(State,{0,1},EMatchSession::Playing,1,1);
    auto A=MakePrivateView(State,0,true,1,1), B=MakePrivateView(State,1,false,1,1);
    FDemoGameLog LA,LB; LA.Update(P,A); LB.Update(P,B);
    ++P.Revision; ++P.CompletedActions; P.CurrentPlayerId=1; P.Seats[0].HandCount=1;
    A.Revision=B.Revision=P.Revision; A.Hand={uint8(ECardId::Polarity)};
    LA.Update(P,A); LB.Update(P,B);
    TestTrue(TEXT("Owner sees exact acquired card"),LA.Text().Contains(TEXT("你获得了【两极反转】")));
    TestTrue(TEXT("Opponent sees public count increase"),LB.Text().Contains(TEXT("黑方手牌增加了 1 张")));
    TestFalse(TEXT("Acquisition identity does not enter opponent log"),LB.Text().Contains(TEXT("两极反转")));
    TestTrue(TEXT("Successful ordinary placement logged"),LB.Text().Contains(TEXT("黑方落子")));
    const auto Lines=LB.GetLines().Num(); LB.Update(P,B);
    TestEqual(TEXT("Repeated snapshot/pose causes no duplicate log"),LB.GetLines().Num(),Lines);
    ++P.Revision; ++P.CompletedActions; P.LastPlayedCard=uint8(ECardId::Polarity); P.LastPlayedCardActor=0; P.LastPlayedCardAction=P.CompletedActions;
    A.Revision=B.Revision=P.Revision; A.Hand.Reset(); P.Seats[0].HandCount=0;
    LA.Update(P,A); LB.Update(P,B);
    TestTrue(TEXT("Actually played identity is public to both"),LB.Text().Contains(TEXT("黑方打出【两极反转】")) && LA.Text().Contains(TEXT("黑方打出【两极反转】")));
    TestEqual(TEXT("Latest public card selected"),DemoDisplayCard(P,0),uint8(ECardId::Polarity));
    P.GhostPhase=EMatchGhostPhase::Hidden;
    TestEqual(TEXT("Ghost lifecycle has priority over recent card"),DemoDisplayCard(P,0),uint8(ECardId::Ghost));
    TestEqual(TEXT("Local pending target has highest priority"),DemoDisplayCard(P,uint8(ECardId::Barrier)),uint8(ECardId::Barrier));
    P.GhostPhase=EMatchGhostPhase::None; P.bTetrisActive=true;
    TestEqual(TEXT("Tetris persists in mode display"),DemoDisplayCard(P,0),uint8(ECardId::Tetris));
    P.bTetrisActive=false;
    for (int32 I=0;I<24;++I)
    { ++P.Revision; ++P.CompletedActions; P.CurrentPlayerId=1-P.CurrentPlayerId; B.Revision=P.Revision; LB.Update(P,B); }
    TestEqual(TEXT("History bounded to latest twelve entries"),LB.GetLines().Num(),12);
    auto OldText=LB.Text(); ++P.Revision; LB.Update(P,B);
    TestEqual(TEXT("Mismatched private revision cannot log"),LB.Text(),OldText);
    ++P.Epoch; P.Revision=1; B.Epoch=P.Epoch; B.Revision=1; LB.Update(P,B);
    TestEqual(TEXT("Restart clears prior history"),LB.GetLines().Num(),1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoNetworkTest,"Gomokards.DemoUI.AcceptedPublicCardAndLocalTargeting",DemoPresentationTestsFlags)
bool FDemoNetworkTest::RunTest(const FString&)
{
    using namespace Gomokards;
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("World"),W)) { return false; }
    auto* GM=W->SpawnActor<ALocalMatchGameMode>(); auto* GS=W->SpawnActor<ALocalMatchGameState>();
    auto* A=W->SpawnActor<ALocalMatchPlayerController>(); auto* B=W->SpawnActor<ALocalMatchPlayerController>();
    if (!GM || !GS || !A || !B) { W->DestroyWorld(false); return false; }
    W->SetGameState(GS); GM->GameState=GS; GM->bSessionInitialized=true;
    GM->StartWithSeed(5751); GM->Join(A,true); GM->Join(B,false);
    const auto Reset=[&](ECardId Card)
    {
        GM->StartWithSeed(5751); GM->Match.Players[0].Hand={Card}; GM->PublishViews(); A->RefreshPresentation(); B->RefreshPresentation();
    };
    Reset(ECardId::TacticalNuke);
    const auto Before=GM->Match;
    A->ToggleTargeting(uint8(ECardId::TacticalNuke));
    TestTrue(TEXT("Local targeting only, no card consumed or publication"),A->SelectedTargetedCard==uint8(ECardId::TacticalNuke) && GM->Match==Before && GS->GetPublicView().LastPlayedCard==0);
    TestEqual(TEXT("Pending side panel uses selected card"),DemoDisplayCard(A->GetPublicView(),A->GetSelectedTargetedCard()),uint8(ECardId::TacticalNuke));
    A->CancelTargeting();
    TestTrue(TEXT("Cancel removes pending only"),A->SelectedTargetedCard==0 && GM->Match==Before && GS->GetPublicView().LastPlayedCard==0);
    TestFalse(TEXT("Invalid target rejected"),GM->TargetedCardFrom(A,GM->MatchEpoch,0,uint8(ECardId::TacticalNuke),{-1,0}).bAccepted);
    TestTrue(TEXT("Rejected target preserves full state and public play"),GM->Match==Before && GS->GetPublicView().LastPlayedCardAction==0);
    A->ToggleTargeting(uint8(ECardId::TacticalNuke));
    TestTrue(TEXT("Server accepts targeted card"),GM->TargetedCardFrom(A,GM->MatchEpoch,0,uint8(ECardId::TacticalNuke),{3,3}).bAccepted);
    const auto& Public=GS->GetPublicView();
    TestTrue(TEXT("Accepted public card, actor and action serial"),Public.LastPlayedCard==uint8(ECardId::TacticalNuke) && Public.LastPlayedCardActor==GM->Assignments[A] && Public.LastPlayedCardAction==1);
    TestTrue(TEXT("Accepted target clears selection and consumes card normally"),A->SelectedTargetedCard==0 && A->GetPrivateView().Hand.IsEmpty());
    const auto Played=Public;
    TestFalse(TEXT("Duplicate card request rejected"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(ECardId::Restock)).bAccepted);
    TestTrue(TEXT("Rejection does not publish false last-card update"),FMatchPublicView::StaticStruct()->CompareScriptStruct(&Played,&GS->GetPublicView(),0));
    for (ECardId Card : {ECardId::Restock,ECardId::Ghost,ECardId::Tetris})
    {
        Reset(Card); TestTrue(TEXT("Non-targeted accepted play"),GM->CardFrom(A,GM->MatchEpoch,0,uint8(Card)).bAccepted);
        TestEqual(TEXT("Accepted card identity persists publicly"),GS->GetPublicView().LastPlayedCard,uint8(Card));
        TestEqual(TEXT("Active/recent card visible on both projections"),DemoDisplayCard(B->GetPublicView(),0),uint8(Card));
        if (Card==ECardId::Ghost)
        { GM->PollGhostPreparation(GM->GhostDeadline,GM->GhostTimerGeneration); TestEqual(TEXT("Hidden phase retains public played Ghost"),GS->GetPublicView().LastPlayedCard,uint8(Card)); }
        if (Card==ECardId::Tetris)
        { GM->PollTetrisGravity(GM->TetrisDeadline,GM->TetrisTimerGeneration); TestEqual(TEXT("Gravity does not replace public played card"),GS->GetPublicView().LastPlayedCard,uint8(Card)); }
    }
    GM->StartWithSeed(5751);
    TestTrue(TEXT("Restart clears last played metadata"),GS->GetPublicView().LastPlayedCard==0 && GS->GetPublicView().LastPlayedCardAction==0);
    W->DestroyWorld(false);
    return true;
}
#endif
