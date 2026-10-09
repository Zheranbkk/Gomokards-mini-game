#include "Singleplayer/SingleplayerBattle.h"
#include "Core/BoardEffects.h"

namespace Gomokards
{
namespace
{
void Shuffle(TArray<FSingleplayerCard>& Cards, FRandomStream& Random)
{
    for (int32 I = Cards.Num() - 1; I > 0; --I) { Cards.Swap(I, Random.RandRange(0, I)); }
}
bool DrawOne(FSingleplayerBattleState& State)
{
    auto& Deck = State.PlayerDeck;
    if (Deck.Hand.Num() >= FSingleplayerDeckState::HandLimit) { return false; }
    if (Deck.DrawPile.IsEmpty())
    {
        if (Deck.DiscardPile.IsEmpty()) { return false; }
        Deck.DrawPile = MoveTemp(Deck.DiscardPile);
        Deck.DiscardPile.Reset();
        Shuffle(Deck.DrawPile, State.DeckRandom);
    }
    Deck.Hand.Add(Deck.DrawPile.Pop(EAllowShrinking::No));
    return true;
}
ESingleplayerError PlacementError(EPlacementError Error)
{
    switch (Error)
    {
    case EPlacementError::None: return ESingleplayerError::None;
    case EPlacementError::Occupied: return ESingleplayerError::Occupied;
    case EPlacementError::Forbidden: return ESingleplayerError::Forbidden;
    default: return ESingleplayerError::InvalidTarget;
    }
}
FSingleplayerActionResult Rejected(const FSingleplayerBattleState& State, ESingleplayerError Error)
{
    FSingleplayerActionResult Result;
    Result.Error = Error;
    Result.Result = State.Match.Result;
    return Result;
}
void CompleteAction(FSingleplayerBattleState& Candidate, FSingleplayerActionResult& Result)
{
    ++Candidate.Match.CompletedActions;
    if (Candidate.Match.Result.Status == EMatchStatus::InProgress)
    { Candidate.Match.CurrentPlayerIndex = 1 - Candidate.Match.CurrentPlayerIndex; }
    Result.Result = Candidate.Match.Result;
}
}

FSingleplayerBattleState::FSingleplayerBattleState(int32 Seed) : Match(Seed), DeckRandom(Seed)
{
    int32 Instance = 0;
    for (ECardId Card : {ECardId::Polarity, ECardId::Barrier, ECardId::TacticalNuke, ECardId::Restock})
    {
        for (int32 Copy = 0; Copy < 2; ++Copy) { PlayerDeck.DrawPile.Add({Instance++, Card}); }
    }
    Shuffle(PlayerDeck.DrawPile, DeckRandom);
    for (int32 I = 0; I < 3; ++I) { DrawOne(*this); }
}
void FSingleplayerBattleState::Reset(int32 Seed) { *this = FSingleplayerBattleState(Seed); }
bool FSingleplayerBattleState::operator==(const FSingleplayerBattleState& Other) const
{
    return Match == Other.Match && PlayerDeck == Other.PlayerDeck
        && DeckRandom.GetInitialSeed() == Other.DeckRandom.GetInitialSeed()
        && DeckRandom.GetCurrentSeed() == Other.DeckRandom.GetCurrentSeed();
}
bool IsSingleplayerCard(ECardId Id)
{
    return Id == ECardId::Polarity || Id == ECardId::Barrier || Id == ECardId::TacticalNuke || Id == ECardId::Restock;
}
bool HasLegalPlacement(const FBoard& Board)
{
    for (const auto& Cell : Board.Cells)
    { if (Cell.Stone == EStone::Empty && !Cell.bForbidden) { return true; } }
    return false;
}
bool HasPlayerMainAction(const FSingleplayerBattleState& State)
{
    if (State.Match.Result.Status != EMatchStatus::InProgress || State.Match.CurrentPlayerIndex != 0) { return false; }
    if (HasLegalPlacement(State.Match.Board)) { return true; }
    // Every supported card has a legal target even on a full board; Restock may validly draw zero.
    for (const auto& Card : State.PlayerDeck.Hand) { if (IsSingleplayerCard(Card.Id)) { return true; } }
    return State.PlayerDeck.Hand.Num() <= FSingleplayerDeckState::HandLimit
        && (!State.PlayerDeck.DrawPile.IsEmpty() || !State.PlayerDeck.DiscardPile.IsEmpty());
}
FSingleplayerActionResult ResolvePlayerAction(FSingleplayerBattleState& State, const FSingleplayerActionRequest& Request)
{
    const auto Fail = [&](ESingleplayerError Error) { return Rejected(State, Error); };
    if (State.Match.Result.Status != EMatchStatus::InProgress) { return Fail(ESingleplayerError::Stopped); }
    if (State.Match.CurrentPlayerIndex != 0) { return Fail(ESingleplayerError::WrongTurn); }
    if (Request.ExpectedActions != State.Match.CompletedActions) { return Fail(ESingleplayerError::StaleAction); }
    FSingleplayerBattleState Candidate = State;
    auto& Deck = Candidate.PlayerDeck;
    FSingleplayerActionResult Result;
    switch (Request.Type)
    {
    case ESingleplayerActionType::PlaceStone:
    {
        if (!Request.Target.IsSet() || Request.CardInstanceId != INDEX_NONE) { return Fail(ESingleplayerError::InvalidRequest); }
        const auto Placement = TryPlaceStone(Candidate.Match.Board, Request.Target.GetValue(), EStone::Black);
        if (!Placement.IsAccepted()) { return Fail(PlacementError(Placement.Error)); }
        Result.bSuccessfulBlock = Placement.bSuccessfulBlock;
        if (Placement.bWinningLine) { Candidate.Match.Result = {EMatchStatus::Won, EStone::Black}; }
        break;
    }
    case ESingleplayerActionType::Draw:
        if (Request.Target.IsSet() || Request.CardInstanceId != INDEX_NONE) { return Fail(ESingleplayerError::InvalidRequest); }
        if (Deck.Hand.Num() >= FSingleplayerDeckState::HandLimit) { return Fail(ESingleplayerError::HandSize); }
        if (!DrawOne(Candidate)) { return Fail(ESingleplayerError::NoDrawableCard); }
        Result.CardsDrawn = 1;
        break;
    case ESingleplayerActionType::Replace:
    case ESingleplayerActionType::PlayCard:
    {
        const int32 Index = Deck.Hand.IndexOfByPredicate([&](const FSingleplayerCard& Card) { return Card.InstanceId == Request.CardInstanceId; });
        if (Index == INDEX_NONE) { return Fail(ESingleplayerError::CardNotOwned); }
        const FSingleplayerCard Resolving = Deck.Hand[Index];
        if (Request.Type == ESingleplayerActionType::Replace)
        {
            if (Request.Target.IsSet()) { return Fail(ESingleplayerError::InvalidRequest); }
            if (Deck.Hand.Num() != FSingleplayerDeckState::HandLimit) { return Fail(ESingleplayerError::HandSize); }
            Deck.Hand.RemoveAt(Index);
            if (!DrawOne(Candidate)) { return Fail(ESingleplayerError::NoDrawableCard); }
            Result.CardsDrawn = 1;
        }
        else
        {
            if (!IsSingleplayerCard(Resolving.Id)) { return Fail(ESingleplayerError::UnsupportedCard); }
            const bool bTargeted = BoardEffectTarget(Resolving.Id) != ECardTarget::None;
            if (bTargeted != Request.Target.IsSet()) { return Fail(ESingleplayerError::InvalidTarget); }
            Deck.Hand.RemoveAt(Index);
            bool bApplied = true;
            switch (Resolving.Id)
            {
            case ECardId::Polarity:
                bApplied = ApplyPolarity(Candidate.Match.Board, Request.Target.GetValue());
                if (bApplied) { Candidate.Match.Result = EvaluateBoardResult(Candidate.Match.Board); }
                break;
            case ECardId::Barrier: bApplied = ApplyBarrier(Candidate.Match.Board, Request.Target.GetValue()); break;
            case ECardId::TacticalNuke: bApplied = ApplyTacticalNuke(Candidate.Match.Board, Request.Target.GetValue()); break;
            case ECardId::Restock:
                while (Result.CardsDrawn < 3 && DrawOne(Candidate)) { ++Result.CardsDrawn; }
                break;
            default: return Fail(ESingleplayerError::UnsupportedCard);
            }
            if (!bApplied) { return Fail(ESingleplayerError::InvalidTarget); }
            Result.PlayedCard = Resolving.Id;
        }
        // Resolving card exists only in this transaction's local value until all draws finish.
        Deck.DiscardPile.Add(Resolving);
        break;
    }
    default: return Fail(ESingleplayerError::InvalidRequest);
    }
    CompleteAction(Candidate, Result);
    State = MoveTemp(Candidate);
    return Result;
}
FSingleplayerActionResult ResolveAIPlacement(FSingleplayerBattleState& State, TOptional<FIntPoint> Intent, uint64 ExpectedActions)
{
    if (State.Match.Result.Status != EMatchStatus::InProgress) { return Rejected(State, ESingleplayerError::Stopped); }
    if (State.Match.CurrentPlayerIndex != 1) { return Rejected(State, ESingleplayerError::WrongTurn); }
    if (ExpectedActions != State.Match.CompletedActions) { return Rejected(State, ESingleplayerError::StaleAction); }
    FSingleplayerBattleState Candidate = State;
    FSingleplayerActionResult Result;
    if (!Intent.IsSet())
    {
        if (HasLegalPlacement(Candidate.Match.Board)) { return Rejected(State, ESingleplayerError::InvalidRequest); }
        Candidate.Match.Result = EvaluateBoardResult(Candidate.Match.Board);
        if (Candidate.Match.Result.Status == EMatchStatus::InProgress) { Candidate.Match.Result = {EMatchStatus::Draw}; }
        Result.Result = Candidate.Match.Result;
    }
    else
    {
        const auto Placement = TryPlaceStone(Candidate.Match.Board, Intent.GetValue(), EStone::White);
        if (!Placement.IsAccepted()) { return Rejected(State, PlacementError(Placement.Error)); }
        Result.bSuccessfulBlock = Placement.bSuccessfulBlock;
        if (Placement.bWinningLine) { Candidate.Match.Result = {EMatchStatus::Won, EStone::White}; }
        CompleteAction(Candidate, Result);
    }
    State = MoveTemp(Candidate);
    return Result;
}
}
