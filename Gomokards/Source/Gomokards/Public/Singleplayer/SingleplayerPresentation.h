#pragma once
#include "Singleplayer/SingleplayerBattle.h"
#include "Presentation/DemoPresentation.h"
#include "InputCoreTypes.h"
namespace Gomokards
{
struct FSingleplayerView
{
    FBoard Board;
    FMatchResult Result;
    int32 CurrentSide = 0;
    uint64 Actions = 0;
    uint64 Generation = 0;
    TArray<FSingleplayerCard> Hand;
    int32 DrawCount = 0, DiscardCount = 0, ExhaustCount = 0;
    bool bReady = false;
    bool CanAct() const { return bReady && Result.Status == EMatchStatus::InProgress && CurrentSide == 0; }
    bool IsReplace() const { return Hand.Num() == FSingleplayerDeckState::HandLimit; }
    bool CanDraw() const { return CanAct() && DrawCount + DiscardCount > 0; }
};
GOMOKARDS_API FSingleplayerView MakeSingleplayerView(const FSingleplayerBattleState& State, uint64 Generation);
GOMOKARDS_API FDemoCardPresentation SingleplayerCardPresentation(ECardId Id);
GOMOKARDS_API FString SingleplayerResultText(const FMatchResult& Result);
GOMOKARDS_API FString SingleplayerErrorText(ESingleplayerError Error);
GOMOKARDS_API bool IsSingleplayerExitKey(const FKey& Key);

// Controller-owned interaction state, never battle state. Requests retain the snapshot serial.
struct GOMOKARDS_API FSingleplayerSelection
{
    int32 TargetInstance = INDEX_NONE;
    ECardId TargetCard = ECardId::Invalid;
    bool bReplacing = false;
    void Clear() { *this = {}; }
    TOptional<FSingleplayerActionRequest> Draw(const FSingleplayerView& View);
    TOptional<FSingleplayerActionRequest> HandClick(const FSingleplayerView& View, int32 Index);
    TOptional<FSingleplayerActionRequest> BoardClick(const FSingleplayerView& View, FIntPoint Point) const;
    FString Instruction() const;
};
}
