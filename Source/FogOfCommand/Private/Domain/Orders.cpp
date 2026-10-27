#include "Domain/Orders.h"

#include "Math/UnrealMathUtility.h"

namespace FogOfCommand::Domain
{
namespace
{
bool IsActive(EOrderStatus Status)
{
	return Status == EOrderStatus::Pending || Status == EOrderStatus::Executing;
}

bool IsValidRecord(const FOrderSnapshot& Order)
{
	if (FUnitOrders::ValidateRequest(Order.Request) != EOrderResult::Success
		|| Order.CreatedAt < 0 || Order.UpdatedAt < Order.CreatedAt)
	{
		return false;
	}
	if (Order.StartedAt.IsSet()
		&& (Order.StartedAt.GetValue() < Order.CreatedAt || Order.StartedAt.GetValue() > Order.UpdatedAt))
	{
		return false;
	}
	if (Order.FinishedAt.IsSet() && Order.FinishedAt.GetValue() != Order.UpdatedAt)
	{
		return false;
	}
	switch (Order.Status)
	{
	case EOrderStatus::Pending:
		return !Order.StartedAt.IsSet() && !Order.FinishedAt.IsSet()
			&& Order.EndReason == EOrderEndReason::None && Order.UpdatedAt == Order.CreatedAt;
	case EOrderStatus::Executing:
		return Order.StartedAt.IsSet() && !Order.FinishedAt.IsSet()
			&& Order.EndReason == EOrderEndReason::None && Order.UpdatedAt == Order.StartedAt.GetValue();
	case EOrderStatus::Completed:
		return Order.StartedAt.IsSet() && Order.FinishedAt.IsSet() && Order.EndReason == EOrderEndReason::Completed;
	case EOrderStatus::Cancelled:
		return Order.FinishedAt.IsSet() && (Order.EndReason == EOrderEndReason::CancelledByRequest
			|| Order.EndReason == EOrderEndReason::Replaced || Order.EndReason == EOrderEndReason::UnitDestroyed);
	case EOrderStatus::Failed:
		return Order.FinishedAt.IsSet() && Order.EndReason == EOrderEndReason::ExecutionFailed;
	default:
		return false;
	}
}

FOrderSnapshot PendingOrder(const FOrderRequest& Request, int64 At)
{
	FOrderSnapshot Result;
	Result.Request = Request;
	Result.CreatedAt = At;
	Result.UpdatedAt = At;
	return Result;
}

void SetTerminal(FOrderSnapshot& Order, EOrderStatus Status, EOrderEndReason Reason, int64 At)
{
	Order.Status = Status;
	Order.EndReason = Reason;
	Order.UpdatedAt = At;
	Order.FinishedAt = At;
}
}

bool FOrderRequest::operator==(const FOrderRequest& Other) const
{
	return OrderId == Other.OrderId && UnitId == Other.UnitId && Kind == Other.Kind && TargetPosition == Other.TargetPosition;
}

bool FOrderSnapshot::IsTerminal() const
{
	return Status == EOrderStatus::Completed || Status == EOrderStatus::Cancelled || Status == EOrderStatus::Failed;
}

bool FOrderSnapshot::operator==(const FOrderSnapshot& Other) const
{
	return Request == Other.Request && Status == Other.Status && CreatedAt == Other.CreatedAt
		&& StartedAt == Other.StartedAt && UpdatedAt == Other.UpdatedAt && FinishedAt == Other.FinishedAt
		&& EndReason == Other.EndReason;
}

EOrderResult FUnitOrders::Initialize(const FUnitStateSnapshot& Initial, int64 At)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	if (FUnitState::ValidateSnapshot(Initial) != EUnitStateResult::Success || Initial.CurrentOrderId.IsSet())
	{
		return EOrderResult::InvalidUnitState;
	}
	switch (Unit.Initialize(Initial))
	{
	case EUnitStateResult::Success:
		RecordUnitEvent(ESimulationEventType::UnitCreated, ESimulationEventReason::Created, At,
			TOptional<FUnitStateSnapshot>());
		return EOrderResult::Success;
	case EUnitStateResult::AlreadyExists:
		return EOrderResult::AlreadyExists;
	case EUnitStateResult::IdentityConflict:
		return EOrderResult::UnknownUnit;
	default:
		return EOrderResult::InvalidUnitState;
	}
}

EOrderResult FUnitOrders::ValidateRequest(const FOrderRequest& Request)
{
	if (!Request.UnitId.IsValid())
	{
		return EOrderResult::InvalidUnitId;
	}
	if (!Request.OrderId.IsValid())
	{
		return EOrderResult::InvalidOrderId;
	}
	if (Request.Kind == EOrderKind::Hold)
	{
		return Request.TargetPosition.IsSet() ? EOrderResult::InvalidTarget : EOrderResult::Success;
	}
	if (Request.Kind != EOrderKind::Move)
	{
		return EOrderResult::InvalidKind;
	}
	if (!Request.TargetPosition.IsSet())
	{
		return EOrderResult::InvalidTarget;
	}
	const FVector& Target = Request.TargetPosition.GetValue();
	return FMath::IsFinite(Target.X) && FMath::IsFinite(Target.Y) && FMath::IsFinite(Target.Z)
		? EOrderResult::Success : EOrderResult::InvalidTarget;
}

EOrderResult FUnitOrders::CheckUnit(const FUnitId& UnitId) const
{
	if (!Unit.IsInitialized())
	{
		return EOrderResult::NotInitialized;
	}
	if (!UnitId.IsValid())
	{
		return EOrderResult::InvalidUnitId;
	}
	return UnitId == Unit.GetSnapshot().UnitId ? EOrderResult::Success : EOrderResult::UnknownUnit;
}

EOrderResult FUnitOrders::CheckMutationTime(int64 At) const
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	return EventLog.GetLastSimulationTick().IsSet() && At < EventLog.GetLastSimulationTick().GetValue()
		? EOrderResult::TimeRegression : EOrderResult::Success;
}

EOrderResult FUnitOrders::CheckActiveInvariant() const
{
	if (!Unit.IsInitialized() || FUnitState::ValidateSnapshot(Unit.GetSnapshot()) != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	const TOptional<FOrderId>& Current = Unit.GetSnapshot().CurrentOrderId;
	if (Current.IsSet())
	{
		const FOrderSnapshot* Active = History.Find(Current.GetValue());
		if (!Unit.IsAlive() || !Active || !IsActive(Active->Status) || !IsValidRecord(*Active)
			|| Active->Request.UnitId != Unit.GetSnapshot().UnitId || Active->Request.OrderId != Current.GetValue())
		{
			return EOrderResult::InvariantViolation;
		}
	}
	return EOrderResult::Success;
}

void FUnitOrders::RecordUnitEvent(ESimulationEventType Type, ESimulationEventReason Reason, int64 At,
	const TOptional<FUnitStateSnapshot>& Before)
{
	FSimulationEvent Event;
	Event.Type = Type;
	Event.SimulationTick = At;
	Event.UnitId = Unit.GetSnapshot().UnitId;
	Event.UnitBefore = Before;
	Event.UnitAfter = Unit.GetSnapshot();
	Event.OrderId = Before.IsSet() ? Before.GetValue().CurrentOrderId : Unit.GetSnapshot().CurrentOrderId;
	Event.Reason = Reason;
	EventLog.Append(MoveTemp(Event));
}

void FUnitOrders::RecordOrderEvent(ESimulationEventType Type, const FOrderSnapshot& Order, int64 At,
	const TOptional<EOrderStatus>& PreviousStatus, const TOptional<FOrderId>& RelatedOrderId)
{
	FSimulationEvent Event;
	Event.Type = Type;
	Event.SimulationTick = At;
	Event.UnitId = Order.Request.UnitId;
	Event.OrderId = Order.Request.OrderId;
	Event.RelatedOrderId = RelatedOrderId;
	Event.PreviousOrderStatus = PreviousStatus;
	Event.OrderStatus = Order.Status;
	Event.OrderKind = Order.Request.Kind;
	Event.TargetPosition = Order.Request.TargetPosition;
	switch (Order.EndReason)
	{
	case EOrderEndReason::Completed: Event.Reason = ESimulationEventReason::Completed; break;
	case EOrderEndReason::CancelledByRequest: Event.Reason = ESimulationEventReason::CancelledByRequest; break;
	case EOrderEndReason::Replaced: Event.Reason = ESimulationEventReason::Replaced; break;
	case EOrderEndReason::UnitDestroyed: Event.Reason = ESimulationEventReason::UnitDestroyed; break;
	case EOrderEndReason::ExecutionFailed: Event.Reason = ESimulationEventReason::ExecutionFailed; break;
	default: Event.Reason = ESimulationEventReason::None; break;
	}
	EventLog.Append(MoveTemp(Event));
}

EOrderResult FUnitOrders::Validate() const
{
	if (!Unit.IsInitialized())
	{
		return EOrderResult::NotInitialized;
	}
	if (FUnitState::ValidateSnapshot(Unit.GetSnapshot()) != EUnitStateResult::Success)
	{
		return EOrderResult::InvalidUnitState;
	}
	if ((LastMutationTime.IsSet() && LastMutationTime.GetValue() < 0) || (!History.IsEmpty() && !LastMutationTime.IsSet()))
	{
		return EOrderResult::InvariantViolation;
	}
	int32 ActiveCount = 0;
	const TOptional<FOrderId>& Current = Unit.GetSnapshot().CurrentOrderId;
	for (const TPair<FOrderId, FOrderSnapshot>& Pair : History)
	{
		const FOrderSnapshot& Order = Pair.Value;
		if (Pair.Key != Order.Request.OrderId || Order.Request.UnitId != Unit.GetSnapshot().UnitId || !IsValidRecord(Order)
			|| (LastMutationTime.IsSet() && Order.UpdatedAt > LastMutationTime.GetValue()))
		{
			return EOrderResult::InvariantViolation;
		}
		if (IsActive(Order.Status))
		{
			++ActiveCount;
			if (!Unit.IsAlive() || !Current.IsSet() || Current.GetValue() != Pair.Key)
			{
				return EOrderResult::InvariantViolation;
			}
		}
		if (Order.EndReason == EOrderEndReason::UnitDestroyed && !Unit.IsDestroyed())
		{
			return EOrderResult::InvariantViolation;
		}
	}
	if (ActiveCount != (Current.IsSet() ? 1 : 0))
	{
		return EOrderResult::InvariantViolation;
	}
	return EOrderResult::Success;
}

EOrderResult FUnitOrders::Submit(const FOrderRequest& Request, int64 At)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	const EOrderResult RequestValidation = ValidateRequest(Request);
	if (RequestValidation != EOrderResult::Success)
	{
		return RequestValidation;
	}
	if (!Unit.IsInitialized())
	{
		return EOrderResult::NotInitialized;
	}
	if (const FOrderSnapshot* Existing = History.Find(Request.OrderId))
	{
		return Existing->Request == Request ? EOrderResult::AlreadyExists : EOrderResult::IntentConflict;
	}
	const EOrderResult UnitValidation = CheckUnit(Request.UnitId);
	if (UnitValidation != EOrderResult::Success)
	{
		return UnitValidation;
	}
	if (Unit.IsDestroyed())
	{
		return EOrderResult::UnitDestroyed;
	}
	if (CheckActiveInvariant() != EOrderResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	if (Unit.GetSnapshot().CurrentOrderId.IsSet())
	{
		return EOrderResult::ActiveOrderExists;
	}
	const EOrderResult TimeValidation = CheckMutationTime(At);
	if (TimeValidation != EOrderResult::Success)
	{
		return TimeValidation;
	}
	FOrderSnapshot NewOrder = PendingOrder(Request, At);
	const FOrderId NewId = Request.OrderId;
	if (!EventLog.CanAppend(1))
	{
		return EOrderResult::EventSequenceExhausted;
	}
	if (Unit.CompareExchangeCurrentOrderId(Request.UnitId, TOptional<FOrderId>(), NewId) != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	History.Add(NewId, MoveTemp(NewOrder));
	LastMutationTime = At;
	RecordOrderEvent(ESimulationEventType::OrderSubmitted, History.FindChecked(NewId), At, TOptional<EOrderStatus>());
	return EOrderResult::Success;
}

EOrderResult FUnitOrders::Replace(const FOrderRequest& Request, const FOrderId& ExpectedCurrent, int64 At)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	if (!ExpectedCurrent.IsValid())
	{
		return EOrderResult::InvalidOrderId;
	}
	const EOrderResult RequestValidation = ValidateRequest(Request);
	if (RequestValidation != EOrderResult::Success)
	{
		return RequestValidation;
	}
	if (!Unit.IsInitialized())
	{
		return EOrderResult::NotInitialized;
	}
	if (const FOrderSnapshot* Existing = History.Find(Request.OrderId))
	{
		return Existing->Request == Request ? EOrderResult::AlreadyExists : EOrderResult::IntentConflict;
	}
	const EOrderResult UnitValidation = CheckUnit(Request.UnitId);
	if (UnitValidation != EOrderResult::Success)
	{
		return UnitValidation;
	}
	if (Unit.IsDestroyed())
	{
		return EOrderResult::UnitDestroyed;
	}
	if (CheckActiveInvariant() != EOrderResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	const TOptional<FOrderId>& Current = Unit.GetSnapshot().CurrentOrderId;
	if (!Current.IsSet() || Current.GetValue() != ExpectedCurrent)
	{
		return EOrderResult::ExpectedOrderMismatch;
	}
	const EOrderResult TimeValidation = CheckMutationTime(At);
	if (TimeValidation != EOrderResult::Success)
	{
		return TimeValidation;
	}
	FOrderSnapshot* OldOrder = History.Find(ExpectedCurrent);
	// ExpectedCurrent may be borrowed from the unit's link, which CAS replaces below.
	const FOrderId OldId = ExpectedCurrent;
	const EOrderStatus PreviousStatus = OldOrder->Status;
	FOrderSnapshot Cancelled = *OldOrder;
	SetTerminal(Cancelled, EOrderStatus::Cancelled, EOrderEndReason::Replaced, At);
	FOrderSnapshot NewOrder = PendingOrder(Request, At);
	const FOrderId NewId = Request.OrderId;
	if (!EventLog.CanAppend(3))
	{
		return EOrderResult::EventSequenceExhausted;
	}
	if (Unit.CompareExchangeCurrentOrderId(Request.UnitId, ExpectedCurrent, NewId) != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	// Commit the old value before Add can reallocate the map; do not reuse OldOrder afterwards.
	*OldOrder = MoveTemp(Cancelled);
	History.Add(NewId, MoveTemp(NewOrder));
	LastMutationTime = At;
	RecordOrderEvent(ESimulationEventType::OrderCancelled, History.FindChecked(OldId), At, PreviousStatus);
	RecordOrderEvent(ESimulationEventType::OrderSubmitted, History.FindChecked(NewId), At, TOptional<EOrderStatus>());
	RecordOrderEvent(ESimulationEventType::OrderReplaced, History.FindChecked(OldId), At, PreviousStatus, NewId);
	return EOrderResult::Success;
}

EOrderResult FUnitOrders::Start(const FUnitId& UnitId, const FOrderId& OrderId, int64 At)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	const EOrderResult UnitValidation = CheckUnit(UnitId);
	if (UnitValidation != EOrderResult::Success)
	{
		return UnitValidation;
	}
	if (!OrderId.IsValid())
	{
		return EOrderResult::InvalidOrderId;
	}
	FOrderSnapshot* Order = History.Find(OrderId);
	if (!Order)
	{
		return EOrderResult::UnknownOrder;
	}
	if (Order->Status == EOrderStatus::Executing)
	{
		return EOrderResult::AlreadyApplied;
	}
	if (Order->Status != EOrderStatus::Pending)
	{
		return EOrderResult::InvalidTransition;
	}
	if (Unit.IsDestroyed())
	{
		return EOrderResult::UnitDestroyed;
	}
	if (CheckActiveInvariant() != EOrderResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	const EOrderResult TimeValidation = CheckMutationTime(At);
	if (TimeValidation != EOrderResult::Success)
	{
		return TimeValidation;
	}
	if (!EventLog.CanAppend(1))
	{
		return EOrderResult::EventSequenceExhausted;
	}
	Order->Status = EOrderStatus::Executing;
	Order->StartedAt = At;
	Order->UpdatedAt = At;
	LastMutationTime = At;
	RecordOrderEvent(ESimulationEventType::OrderStarted, *Order, At, EOrderStatus::Pending);
	return EOrderResult::Success;
}

EOrderResult FUnitOrders::Complete(const FUnitId& UnitId, const FOrderId& OrderId, int64 At)
{
	return Finish(UnitId, OrderId, At, EOrderStatus::Completed, EOrderEndReason::Completed);
}

EOrderResult FUnitOrders::Cancel(const FUnitId& UnitId, const FOrderId& OrderId, int64 At, EOrderEndReason Reason)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	if (Reason != EOrderEndReason::CancelledByRequest)
	{
		return EOrderResult::InvalidReason;
	}
	return Finish(UnitId, OrderId, At, EOrderStatus::Cancelled, Reason);
}

EOrderResult FUnitOrders::Fail(const FUnitId& UnitId, const FOrderId& OrderId, int64 At, EOrderEndReason Reason)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	if (Reason != EOrderEndReason::ExecutionFailed)
	{
		return EOrderResult::InvalidReason;
	}
	return Finish(UnitId, OrderId, At, EOrderStatus::Failed, Reason);
}

EOrderResult FUnitOrders::Finish(const FUnitId& UnitId, const FOrderId& OrderId, int64 At,
	EOrderStatus Status, EOrderEndReason Reason)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	const EOrderResult UnitValidation = CheckUnit(UnitId);
	if (UnitValidation != EOrderResult::Success)
	{
		return UnitValidation;
	}
	if (!OrderId.IsValid())
	{
		return EOrderResult::InvalidOrderId;
	}
	FOrderSnapshot* Order = History.Find(OrderId);
	if (!Order)
	{
		return EOrderResult::UnknownOrder;
	}
	if (Order->IsTerminal())
	{
		return Order->Status == Status && Order->EndReason == Reason ? EOrderResult::AlreadyApplied : EOrderResult::TerminalConflict;
	}
	if (Unit.IsDestroyed())
	{
		return EOrderResult::UnitDestroyed;
	}
	if (!IsActive(Order->Status) || (Status == EOrderStatus::Completed && Order->Status != EOrderStatus::Executing))
	{
		return EOrderResult::InvalidTransition;
	}
	if (CheckActiveInvariant() != EOrderResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	const EOrderResult TimeValidation = CheckMutationTime(At);
	if (TimeValidation != EOrderResult::Success)
	{
		return TimeValidation;
	}
	FOrderSnapshot Finished = *Order;
	const EOrderStatus PreviousStatus = Order->Status;
	SetTerminal(Finished, Status, Reason, At);
	if (!EventLog.CanAppend(1))
	{
		return EOrderResult::EventSequenceExhausted;
	}
	if (Unit.CompareExchangeCurrentOrderId(UnitId, OrderId, TOptional<FOrderId>()) != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	*Order = MoveTemp(Finished);
	LastMutationTime = At;
	const ESimulationEventType Type = Status == EOrderStatus::Completed ? ESimulationEventType::OrderCompleted
		: Status == EOrderStatus::Cancelled ? ESimulationEventType::OrderCancelled : ESimulationEventType::OrderFailed;
	RecordOrderEvent(Type, *Order, At, PreviousStatus);
	return EOrderResult::Success;
}

EOrderResult FUnitOrders::DestroyUnit(const FUnitId& UnitId, int64 At)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	const EOrderResult UnitValidation = CheckUnit(UnitId);
	if (UnitValidation != EOrderResult::Success)
	{
		return UnitValidation;
	}
	if (Unit.IsDestroyed())
	{
		return EOrderResult::AlreadyApplied;
	}
	if (CheckActiveInvariant() != EOrderResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	const EOrderResult TimeValidation = CheckMutationTime(At);
	if (TimeValidation != EOrderResult::Success)
	{
		return TimeValidation;
	}
	FOrderSnapshot* Active = Unit.GetSnapshot().CurrentOrderId.IsSet()
		? History.Find(Unit.GetSnapshot().CurrentOrderId.GetValue()) : nullptr;
	const FUnitStateSnapshot Before = Unit.GetSnapshot();
	TOptional<EOrderStatus> PreviousStatus;
	TOptional<FOrderSnapshot> Cancelled;
	if (Active)
	{
		PreviousStatus = Active->Status;
		Cancelled = *Active;
		SetTerminal(Cancelled.GetValue(), EOrderStatus::Cancelled, EOrderEndReason::UnitDestroyed, At);
	}
	if (!EventLog.CanAppend(Active ? 2 : 1))
	{
		return EOrderResult::EventSequenceExhausted;
	}
	if (Unit.Destroy() != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	if (Active)
	{
		*Active = MoveTemp(Cancelled.GetValue());
	}
	LastMutationTime = At;
	if (Active)
	{
		RecordOrderEvent(ESimulationEventType::OrderCancelled, *Active, At, PreviousStatus);
	}
	RecordUnitEvent(ESimulationEventType::UnitDestroyed, ESimulationEventReason::ExplicitDestruction, At, Before);
	return EOrderResult::Success;
}

EOrderResult FUnitOrders::ApplyDamage(const FUnitId& UnitId, double Amount, int64 At)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	const EOrderResult UnitValidation = CheckUnit(UnitId);
	if (UnitValidation != EOrderResult::Success)
	{
		return UnitValidation;
	}
	if (Unit.IsDestroyed())
	{
		return EOrderResult::UnitDestroyed;
	}
	if (!FMath::IsFinite(Amount) || Amount < 0.0)
	{
		return EOrderResult::InvalidAmount;
	}
	if (CheckActiveInvariant() != EOrderResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	// Preflight through the accepted unit transition, without duplicating its health arithmetic.
	FUnitState Preview(Unit);
	if (Preview.ApplyDamage(Amount) != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	if (Preview.GetSnapshot() == Unit.GetSnapshot())
	{
		return EOrderResult::AlreadyApplied;
	}
	const EOrderResult TimeValidation = CheckMutationTime(At);
	if (TimeValidation != EOrderResult::Success)
	{
		return TimeValidation;
	}
	FOrderSnapshot* Active = Unit.GetSnapshot().CurrentOrderId.IsSet()
		? History.Find(Unit.GetSnapshot().CurrentOrderId.GetValue()) : nullptr;
	const FUnitStateSnapshot Before = Unit.GetSnapshot();
	TOptional<EOrderStatus> PreviousStatus;
	TOptional<FOrderSnapshot> Cancelled;
	if (Preview.IsDestroyed() && Active)
	{
		PreviousStatus = Active->Status;
		Cancelled = *Active;
		SetTerminal(Cancelled.GetValue(), EOrderStatus::Cancelled, EOrderEndReason::UnitDestroyed, At);
	}
	if (!EventLog.CanAppend(1 + (Preview.IsDestroyed() ? 1 : 0) + (Cancelled.IsSet() ? 1 : 0)))
	{
		return EOrderResult::EventSequenceExhausted;
	}
	if (Unit.ApplyDamage(Amount) != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	if (Cancelled.IsSet())
	{
		*Active = MoveTemp(Cancelled.GetValue());
	}
	LastMutationTime = At;
	RecordUnitEvent(ESimulationEventType::UnitDamaged, ESimulationEventReason::DamageApplied, At, Before);
	if (Cancelled.IsSet())
	{
		RecordOrderEvent(ESimulationEventType::OrderCancelled, *Active, At, PreviousStatus);
	}
	if (Unit.IsDestroyed())
	{
		RecordUnitEvent(ESimulationEventType::UnitDestroyed, ESimulationEventReason::DamageApplied, At, Before);
	}
	return EOrderResult::Success;
}

EOrderResult FUnitOrders::MoveUnit(const FUnitId& UnitId, const FVector& Position, int64 At)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	const EOrderResult UnitValidation = CheckUnit(UnitId);
	if (UnitValidation != EOrderResult::Success)
	{
		return UnitValidation;
	}
	if (Unit.IsDestroyed())
	{
		return EOrderResult::UnitDestroyed;
	}
	if (CheckActiveInvariant() != EOrderResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	FUnitState Preview(Unit);
	if (Preview.MoveTo(Position) != EUnitStateResult::Success)
	{
		return EOrderResult::InvalidPosition;
	}
	if (Preview.GetSnapshot() == Unit.GetSnapshot())
	{
		return EOrderResult::AlreadyApplied;
	}
	const EOrderResult TimeValidation = CheckMutationTime(At);
	if (TimeValidation != EOrderResult::Success)
	{
		return TimeValidation;
	}
	if (!EventLog.CanAppend(1))
	{
		return EOrderResult::EventSequenceExhausted;
	}
	const FUnitStateSnapshot Before = Unit.GetSnapshot();
	if (Unit.MoveTo(Position) != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	LastMutationTime = At;
	RecordUnitEvent(ESimulationEventType::UnitMoved, ESimulationEventReason::PositionChanged, At, Before);
	return EOrderResult::Success;
}

EOrderResult FUnitOrders::ConsumeAmmunition(const FUnitId& UnitId, int32 Amount, int64 At)
{
	if (At < 0)
	{
		return EOrderResult::InvalidTime;
	}
	const EOrderResult UnitValidation = CheckUnit(UnitId);
	if (UnitValidation != EOrderResult::Success)
	{
		return UnitValidation;
	}
	if (Unit.IsDestroyed())
	{
		return EOrderResult::UnitDestroyed;
	}
	if (CheckActiveInvariant() != EOrderResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	FUnitState Preview(Unit);
	const EUnitStateResult PreviewResult = Preview.ConsumeAmmunition(Amount);
	if (PreviewResult != EUnitStateResult::Success)
	{
		return PreviewResult == EUnitStateResult::InsufficientAmmunition
			? EOrderResult::InsufficientAmmunition : EOrderResult::InvalidAmount;
	}
	if (Preview.GetSnapshot() == Unit.GetSnapshot())
	{
		return EOrderResult::AlreadyApplied;
	}
	const EOrderResult TimeValidation = CheckMutationTime(At);
	if (TimeValidation != EOrderResult::Success)
	{
		return TimeValidation;
	}
	if (!EventLog.CanAppend(1))
	{
		return EOrderResult::EventSequenceExhausted;
	}
	const FUnitStateSnapshot Before = Unit.GetSnapshot();
	if (Unit.ConsumeAmmunition(Amount) != EUnitStateResult::Success)
	{
		return EOrderResult::InvariantViolation;
	}
	LastMutationTime = At;
	RecordUnitEvent(ESimulationEventType::AmmunitionConsumed, ESimulationEventReason::AmmunitionSpent, At, Before);
	return EOrderResult::Success;
}
}
