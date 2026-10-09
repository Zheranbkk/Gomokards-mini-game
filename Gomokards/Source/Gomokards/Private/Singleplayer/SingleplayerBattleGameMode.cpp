#include "Singleplayer/SingleplayerBattleGameMode.h"
#include "Singleplayer/SingleplayerBattlePlayerController.h"
#include "Singleplayer/SingleplayerAI.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
using namespace Gomokards;

ASingleplayerBattleGameMode::ASingleplayerBattleGameMode()
{
    PlayerControllerClass = ASingleplayerBattlePlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
}
void ASingleplayerBattleGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    BattleSeed = UGameplayStatics::HasOption(Options, TEXT("Seed"))
        ? UGameplayStatics::GetIntOption(Options, TEXT("Seed"), 0) : FMath::Rand();
    RestartBattle();
}
FSingleplayerView ASingleplayerBattleGameMode::GetPresentation() const
{
    auto View = MakeSingleplayerView(Battle, Generation);
    View.bReady = bReady;
    return View;
}
void ASingleplayerBattleGameMode::AddLog(FString Line)
{
    if (Line.IsEmpty()) { return; }
    Log.Add(MoveTemp(Line));
    if (Log.Num() > 12) { Log.RemoveAt(0); }
}
void ASingleplayerBattleGameMode::RestartBattle()
{
    GetWorldTimerManager().ClearTimer(AITimer);
    ++Generation;
    bAIPending = false;
    Battle.Reset(BattleSeed);
    Log.Reset();
    AddLog(TEXT("对局开始。玩家执黑，先行。"));
    bReady = true;
    OnBattleChanged.Broadcast();
}
FSingleplayerActionResult ASingleplayerBattleGameMode::SubmitPlayer(const FSingleplayerActionRequest& Request, uint64 ExpectedGeneration)
{
    if (!bReady || ExpectedGeneration != Generation)
    {
        FSingleplayerActionResult Result;
        Result.Error = ESingleplayerError::StaleAction;
        Result.Result = Battle.Match.Result;
        return Result;
    }
    const auto Result = ResolvePlayerAction(Battle, Request);
    if (!Result.IsAccepted()) { return Result; }
    switch (Request.Type)
    {
    case ESingleplayerActionType::PlaceStone: AddLog(TEXT("玩家落子。")); break;
    case ESingleplayerActionType::Draw: AddLog(TEXT("玩家抽取了 1 张牌。")); break;
    case ESingleplayerActionType::Replace: AddLog(TEXT("玩家替换了 1 张手牌。")); break;
    case ESingleplayerActionType::PlayCard:
        AddLog(FString::Printf(TEXT("玩家打出【%s】。"), *SingleplayerCardPresentation(Result.PlayedCard).Name)); break;
    }
    AddLog(SingleplayerResultText(Battle.Match.Result));
    ScheduleAI();
    OnBattleChanged.Broadcast();
    return Result;
}
void ASingleplayerBattleGameMode::ScheduleAI()
{
    if (!bReady || bAIPending || Battle.Match.Result.Status != EMatchStatus::InProgress || Battle.Match.CurrentPlayerIndex != 1) { return; }
    bAIPending = true;
    const uint64 ExpectedGeneration = Generation, ExpectedActions = Battle.Match.CompletedActions;
    AITimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, ExpectedGeneration, ExpectedActions]
    { ExecutePendingAI(ExpectedGeneration, ExpectedActions); }));
}
bool ASingleplayerBattleGameMode::ExecutePendingAI(uint64 ExpectedGeneration, uint64 ExpectedActions)
{
    if (!bReady || !bAIPending || ExpectedGeneration != Generation || ExpectedActions != Battle.Match.CompletedActions
        || Battle.Match.CurrentPlayerIndex != 1 || Battle.Match.Result.Status != EMatchStatus::InProgress) { return false; }
    GetWorldTimerManager().ClearTimer(AITimer);
    bAIPending = false;
    const auto Intent = ChooseSingleplayerMove(Battle.Match.Board);
    const auto Result = ResolveAIPlacement(Battle, Intent, ExpectedActions);
    if (!Result.IsAccepted())
    {
        UE_LOG(LogTemp, Error, TEXT("SP1 AI intent rejected: %d"), int32(Result.Error));
        AddLog(TEXT("AI 行动无效，请重新开始。"));
    }
    else
    {
        if (Intent.IsSet()) { AddLog(TEXT("AI 落子。")); }
        AddLog(SingleplayerResultText(Battle.Match.Result));
    }
    OnBattleChanged.Broadcast();
    return Result.IsAccepted();
}
void ASingleplayerBattleGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    bReady = false;
    bAIPending = false;
    ++Generation;
    GetWorldTimerManager().ClearTimer(AITimer);
    OnBattleChanged.Clear();
    Super::EndPlay(Reason);
}
