#pragma once

#include "Domain/UnitState.h"

namespace FogOfCommand::Domain
{
enum class EOrderKind : uint8;
enum class EOrderStatus : uint8;
class FUnitOrders;

enum class ESimulationEventType : uint8
{
	UnitCreated, UnitMoved, UnitDamaged, UnitDestroyed, AmmunitionConsumed,
	OrderSubmitted, OrderStarted, OrderCompleted, OrderCancelled, OrderFailed, OrderReplaced
};

enum class ESimulationEventReason : uint8
{
	None, Created, PositionChanged, DamageApplied, AmmunitionSpent, ExplicitDestruction,
	Completed, CancelledByRequest, Replaced, UnitDestroyed, ExecutionFailed
};

/** Copied domain values only: no free text, wall clock, object pointers or user data. */
struct FOGOFCOMMAND_API FSimulationEvent
{
	uint64 Sequence = 0;
	int64 SimulationTick = 0;
	ESimulationEventType Type = ESimulationEventType::UnitCreated;
	FUnitId UnitId;
	TOptional<FOrderId> OrderId;
	TOptional<FOrderId> RelatedOrderId;
	ESimulationEventReason Reason = ESimulationEventReason::None;
	TOptional<FUnitStateSnapshot> UnitBefore;
	TOptional<FUnitStateSnapshot> UnitAfter;
	TOptional<EOrderStatus> PreviousOrderStatus;
	TOptional<EOrderStatus> OrderStatus;
	TOptional<EOrderKind> OrderKind;
	TOptional<FVector> TargetPosition;
	bool operator==(const FSimulationEvent& Other) const;
	bool operator!=(const FSimulationEvent& Other) const { return !(*this == Other); }
};

/**
 * Fixed-retention per-owner journal, not a persistent replay/event-sourcing store.
 * Reads return chronological copies; only the validated owner can append.
 * A serial operation publishes its complete batch after committing state. Allocation
 * failure is a process-level failure, not a recoverable domain refusal.
 */
class FOGOFCOMMAND_API FSimulationEventLog
{
public:
	static constexpr int32 Capacity = 256;
	int32 Num() const { return Entries.Num(); }
	uint64 GetTotalRecorded() const { return TotalRecorded; }
	uint64 GetEvictedCount() const { return TotalRecorded - uint64(Entries.Num()); }
	TOptional<int64> GetLastSimulationTick() const { return LastSimulationTick; }
	TArray<FSimulationEvent> GetEvents() const;

private:
	friend class FUnitOrders;
	bool CanAppend(int32 Count) const;
	void Append(FSimulationEvent&& Event);
	TArray<FSimulationEvent> Entries;
	int32 NextIndex = 0;
	uint64 TotalRecorded = 0;
	TOptional<int64> LastSimulationTick;
};
}
