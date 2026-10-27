#pragma once

#include "Containers/Map.h"
#include "Domain/SimulationEvents.h"

namespace FogOfCommand::Domain
{
enum class EOrderKind : uint8
{
	Move,
	Hold
};

enum class EOrderStatus : uint8
{
	Pending,
	Executing,
	Completed,
	Cancelled,
	Failed
};

enum class EOrderEndReason : uint8
{
	None,
	Completed,
	CancelledByRequest,
	Replaced,
	UnitDestroyed,
	ExecutionFailed
};

enum class EOrderResult : uint8
{
	Success,
	AlreadyExists,
	AlreadyApplied,
	NotInitialized,
	InvalidUnitState,
	InvalidUnitId,
	InvalidOrderId,
	UnknownUnit,
	UnknownOrder,
	InvalidKind,
	InvalidTarget,
	InvalidTime,
	TimeRegression,
	InvalidReason,
	ActiveOrderExists,
	ExpectedOrderMismatch,
	IntentConflict,
	InvalidTransition,
	TerminalConflict,
	UnitDestroyed,
	InvalidAmount,
	InvariantViolation,
	InvalidPosition,
	InsufficientAmmunition,
	EventSequenceExhausted
};

/** Immutable intent after submission. Move requires a finite target; Hold requires no target. */
struct FOGOFCOMMAND_API FOrderRequest
{
	FOrderId OrderId;
	FUnitId UnitId;
	EOrderKind Kind = EOrderKind::Hold;
	TOptional<FVector> TargetPosition;
	bool operator==(const FOrderRequest& Other) const;
	bool operator!=(const FOrderRequest& Other) const { return !(*this == Other); }
};

/** Value snapshot of one order, with simulation ticks rather than wall-clock time. */
struct FOGOFCOMMAND_API FOrderSnapshot
{
	FOrderRequest Request;
	EOrderStatus Status = EOrderStatus::Pending;
	int64 CreatedAt = 0;
	TOptional<int64> StartedAt;
	int64 UpdatedAt = 0;
	TOptional<int64> FinishedAt;
	EOrderEndReason EndReason = EOrderEndReason::None;
	bool IsTerminal() const;
	bool operator==(const FOrderSnapshot& Other) const;
	bool operator!=(const FOrderSnapshot& Other) const { return !(*this == Other); }
};

/**
 * Single-unit owner of state and local order history. Not a world registry, queue,
 * navigation/execution system or SaveGame. It has no mutable unit/history escape.
 * Terminal records remain for local OrderId deduplication for this owner's lifetime.
 * Serial callers only: atomic rejection is a domain guarantee, not thread synchronization.
 */
class FOGOFCOMMAND_API FUnitOrders
{
public:
	FUnitOrders() = default;
	FUnitOrders(const FUnitOrders&) = delete;
	FUnitOrders(FUnitOrders&&) = delete;
	FUnitOrders& operator=(const FUnitOrders&) = delete;
	FUnitOrders& operator=(FUnitOrders&&) = delete;

	/**
	 * No history restore: any initial CurrentOrderId is rejected as an orphan.
	 * First adoption emits UnitCreated at nonnegative At (including a valid tombstone).
	 * A retry never emits or rewinds time. Later changes cannot precede creation.
	 */
	EOrderResult Initialize(const FUnitStateSnapshot& Initial, int64 At = 0);
	const FUnitState& GetUnit() const { return Unit; }
	const TMap<FOrderId, FOrderSnapshot>& GetHistory() const { return History; }
	/** The pointer is borrowed and may be invalidated by the next successful command. */
	const FOrderSnapshot* FindOrder(const FOrderId& OrderId) const { return History.Find(OrderId); }
	/** Legacy post-creation mutation clock; creation alone leaves this unset. */
	TOptional<int64> GetLastMutationTime() const { return LastMutationTime; }
	/** Its last simulation tick includes creation and is the effective mutation-time boundary. */
	const FSimulationEventLog& GetEventLog() const { return EventLog; }
	static EOrderResult ValidateRequest(const FOrderRequest& Request);
	/** Check lifecycle, timestamps, ownership and exactly one matching active link (or none). */
	EOrderResult Validate() const;

	/**
	 * At must be nonnegative. Actual mutations require At >= the last mutation time;
	 * identical retries do not advance that clock, even when their valid At is older.
	 * Reusing an OrderId with changed intent fails, including after terminal completion.
	 */
	EOrderResult Submit(const FOrderRequest& Request, int64 At);
	/** New intent replaces exactly ExpectedCurrent. Existing identical new ID is a no-op retry. */
	EOrderResult Replace(const FOrderRequest& Request, const FOrderId& ExpectedCurrent, int64 At);
	EOrderResult Start(const FUnitId& UnitId, const FOrderId& OrderId, int64 At);
	/** Complete requires Executing; cancel/fail also accept Pending. */
	EOrderResult Complete(const FUnitId& UnitId, const FOrderId& OrderId, int64 At);
	/** Only CancelledByRequest is public; replacement/destruction reasons are coordinated internally. */
	EOrderResult Cancel(const FUnitId& UnitId, const FOrderId& OrderId, int64 At,
		EOrderEndReason Reason = EOrderEndReason::CancelledByRequest);
	EOrderResult Fail(const FUnitId& UnitId, const FOrderId& OrderId, int64 At,
		EOrderEndReason Reason = EOrderEndReason::ExecutionFailed);
	/** Coordinate the unit tombstone and active cancellation before committing either. */
	EOrderResult DestroyUnit(const FUnitId& UnitId, int64 At);
	EOrderResult ApplyDamage(const FUnitId& UnitId, double Amount, int64 At);
	/** Value-state movement only: no navigation, scene update or automatic order completion. */
	EOrderResult MoveUnit(const FUnitId& UnitId, const FVector& Position, int64 At);
	EOrderResult ConsumeAmmunition(const FUnitId& UnitId, int32 Amount, int64 At);

private:
	EOrderResult CheckUnit(const FUnitId& UnitId) const;
	EOrderResult CheckMutationTime(int64 At) const;
	/** Constant-time checks for the private active link; Validate remains the explicit full audit. */
	EOrderResult CheckActiveInvariant() const;
	void RecordUnitEvent(ESimulationEventType Type, ESimulationEventReason Reason, int64 At,
		const TOptional<FUnitStateSnapshot>& Before);
	void RecordOrderEvent(ESimulationEventType Type, const FOrderSnapshot& Order, int64 At,
		const TOptional<EOrderStatus>& PreviousStatus,
		const TOptional<FOrderId>& RelatedOrderId = TOptional<FOrderId>());
	EOrderResult Finish(const FUnitId& UnitId, const FOrderId& OrderId, int64 At,
		EOrderStatus Status, EOrderEndReason Reason);
	FUnitState Unit;
	TMap<FOrderId, FOrderSnapshot> History;
	TOptional<int64> LastMutationTime;
	FSimulationEventLog EventLog;
};
}
