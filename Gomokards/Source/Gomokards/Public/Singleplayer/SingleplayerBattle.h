#pragma once
#include "Core/MatchRules.h"

namespace Gomokards
{
struct FSingleplayerCard
{
    int32 InstanceId = INDEX_NONE;
    ECardId Id = ECardId::Invalid;
    bool operator==(const FSingleplayerCard&) const = default;
};

struct FSingleplayerDeckState
{
    static constexpr int32 HandLimit = 5;
    TArray<FSingleplayerCard> DrawPile; // Last element is the top.
    TArray<FSingleplayerCard> Hand;
    TArray<FSingleplayerCard> DiscardPile;
    TArray<FSingleplayerCard> ExhaustPile;
    bool operator==(const FSingleplayerDeckState&) const = default;
};

// The runtime owns one live value. Public fields support pure test fixtures only.
// All live mutations go through reset or the resolvers below; UI receives a projection.
struct GOMOKARDS_API FSingleplayerBattleState
{
    FMatchState Match;
    FSingleplayerDeckState PlayerDeck;
    FRandomStream DeckRandom;
    explicit FSingleplayerBattleState(int32 Seed = 0);
    void Reset(int32 Seed);
    bool operator==(const FSingleplayerBattleState& Other) const;
};

enum class ESingleplayerActionType : uint8 { PlaceStone, PlayCard, Draw, Replace };
struct FSingleplayerActionRequest
{
    ESingleplayerActionType Type = ESingleplayerActionType::PlaceStone;
    uint64 ExpectedActions = 0;
    int32 CardInstanceId = INDEX_NONE;
    TOptional<FIntPoint> Target;
};
enum class ESingleplayerError : uint8
{
    None, Stopped, WrongTurn, StaleAction, InvalidRequest, CardNotOwned,
    UnsupportedCard, InvalidTarget, Occupied, Forbidden, HandSize, NoDrawableCard
};
struct FSingleplayerActionResult
{
    ESingleplayerError Error = ESingleplayerError::None;
    bool bSuccessfulBlock = false;
    int32 CardsDrawn = 0;
    ECardId PlayedCard = ECardId::Invalid;
    FMatchResult Result;
    bool IsAccepted() const { return Error == ESingleplayerError::None; }
};

GOMOKARDS_API bool IsSingleplayerCard(ECardId Id);
GOMOKARDS_API bool HasLegalPlacement(const FBoard& Board);
GOMOKARDS_API bool HasPlayerMainAction(const FSingleplayerBattleState& State);
GOMOKARDS_API FSingleplayerActionResult ResolvePlayerAction(FSingleplayerBattleState& State, const FSingleplayerActionRequest& Request);
// An absent intent is accepted only when no legal AI placement exists. That ends in Draw,
// without inventing a completed placement. Policy never gets a writable state.
GOMOKARDS_API FSingleplayerActionResult ResolveAIPlacement(FSingleplayerBattleState& State, TOptional<FIntPoint> Intent, uint64 ExpectedActions);
}
