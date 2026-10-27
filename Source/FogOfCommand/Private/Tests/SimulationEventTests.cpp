#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Domain/Orders.h"
#include "Domain/SimulationEvents.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace
{
using namespace FogOfCommand::Domain;

static_assert(std::is_same_v<decltype(std::declval<const FUnitOrders&>().GetEventLog()), const FSimulationEventLog&>);
static_assert(std::is_same_v<decltype(std::declval<const FSimulationEventLog&>().GetEvents()), TArray<FSimulationEvent>>);

FUnitStateSnapshot EventTestUnit()
{
	FUnitStateSnapshot Unit;
	Unit.UnitId = FUnitId::Create();
	Unit.SideId = FSideId::FromValue(3);
	Unit.Position = FVector(10.0, 20.0, 30.0);
	Unit.Health = 80.0;
	Unit.MaxHealth = 100.0;
	Unit.Ammunition = 12;
	Unit.AmmoCapacity = 20;
	return Unit;
}

FOrderRequest EventTestOrder(const FUnitId& UnitId, bool bMove = false)
{
	FOrderRequest Request;
	Request.UnitId = UnitId;
	Request.OrderId = FOrderId::Create();
	Request.Kind = bMove ? EOrderKind::Move : EOrderKind::Hold;
	if (bMove) { Request.TargetPosition = FVector(400.0, 500.0, 0.0); }
	return Request;
}

bool EventResult(FAutomationTestBase& Test, const TCHAR* Description, EOrderResult Actual, EOrderResult Expected = EOrderResult::Success)
{
	return Test.TestEqual(Description, static_cast<int32>(Actual), static_cast<int32>(Expected));
}

struct FEventObservation
{
	bool bInitialized;
	FUnitStateSnapshot Unit;
	TMap<FOrderId, FOrderSnapshot> History;
	TOptional<int64> LastMutation;
	TOptional<int64> LastEventTick;
	TArray<FSimulationEvent> Events;
	uint64 Total;
	uint64 Evicted;
};

FEventObservation ObserveEvents(const FUnitOrders& Owner)
{
	return { Owner.GetUnit().IsInitialized(), Owner.GetUnit().GetSnapshot(), Owner.GetHistory(),
		Owner.GetLastMutationTime(), Owner.GetEventLog().GetLastSimulationTick(), Owner.GetEventLog().GetEvents(), Owner.GetEventLog().GetTotalRecorded(),
		Owner.GetEventLog().GetEvictedCount() };
}

bool MatchesEvents(const FUnitOrders& Owner, const FEventObservation& Before)
{
	if (Owner.GetUnit().IsInitialized() != Before.bInitialized || Owner.GetUnit().GetSnapshot() != Before.Unit
		|| Owner.GetHistory().Num() != Before.History.Num() || Owner.GetLastMutationTime() != Before.LastMutation
		|| Owner.GetEventLog().GetLastSimulationTick() != Before.LastEventTick
		|| Owner.GetEventLog().GetEvents() != Before.Events || Owner.GetEventLog().GetTotalRecorded() != Before.Total
		|| Owner.GetEventLog().GetEvictedCount() != Before.Evicted)
	{
		return false;
	}
	for (const auto& Pair : Before.History)
	{
		const FOrderSnapshot* Current = Owner.FindOrder(Pair.Key);
		if (!Current || *Current != Pair.Value) { return false; }
	}
	return true;
}

template <typename OperationType>
void EventUnchanged(FAutomationTestBase& Test, const TCHAR* Description, FUnitOrders& Owner,
	EOrderResult Expected, OperationType Operation)
{
	const FEventObservation Before = ObserveEvents(Owner);
	EventResult(Test, Description, Operation(), Expected);
	Test.TestTrue(*(FString(Description) + TEXT(" preserves unit, history, clock and complete journal")), MatchesEvents(Owner, Before));
}

void EventIdentity(FAutomationTestBase& Test, const FSimulationEvent& Event, ESimulationEventType Type,
	uint64 Sequence, int64 At, const FUnitId& UnitId)
{
	Test.TestTrue(TEXT("Event type matches the actual committed transition"), Event.Type == Type);
	Test.TestEqual(TEXT("Sequence identifies deterministic append order"), Event.Sequence, Sequence);
	Test.TestEqual(TEXT("Simulation tick is the supplied logical time"), Event.SimulationTick, At);
	Test.TestTrue(TEXT("Event identifies the same stable unit"), Event.UnitId == UnitId);
}

void EventOrderId(FAutomationTestBase& Test, const FSimulationEvent& Event, const FOrderId& OrderId)
{
	Test.TestTrue(TEXT("Order event carries the exact strong OrderId"), Event.OrderId.IsSet() && Event.OrderId.GetValue() == OrderId);
}

void EventAudit(FAutomationTestBase& Test, const FUnitOrders& Owner)
{
	EventResult(Test, TEXT("Full explicit owner invariant audit"), Owner.Validate());
	const TArray<FSimulationEvent> Events = Owner.GetEventLog().GetEvents();
	Test.TestEqual(TEXT("Ordered copy and retained count agree"), Events.Num(), Owner.GetEventLog().Num());
	for (int32 Index = 1; Index < Events.Num(); ++Index)
	{
		Test.TestTrue(TEXT("Retained sequence numbers are contiguous"), Events[Index].Sequence == Events[Index - 1].Sequence + 1);
		Test.TestTrue(TEXT("Journal simulation ticks never regress"), Events[Index].SimulationTick >= Events[Index - 1].SimulationTick);
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogEventsTransitionsTest, "FogOfCommand.Events.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogEventsTransitionsTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = EventTestUnit();
	FUnitOrders Owner;
	EventResult(*this, TEXT("Create journalled unit"), Owner.Initialize(Initial, 5));
	EventResult(*this, TEXT("Move a value-state position"), Owner.MoveUnit(Initial.UnitId, FVector(100.0, 20.0, 30.0), 6));
	EventResult(*this, TEXT("Apply nonlethal damage"), Owner.ApplyDamage(Initial.UnitId, 10.0, 7));
	EventResult(*this, TEXT("Consume available ammunition"), Owner.ConsumeAmmunition(Initial.UnitId, 2, 8));
	const FOrderRequest A = EventTestOrder(Initial.UnitId, true);
	EventResult(*this, TEXT("Submit MOVE"), Owner.Submit(A, 9));
	EventResult(*this, TEXT("Start MOVE"), Owner.Start(Initial.UnitId, A.OrderId, 10));
	EventResult(*this, TEXT("Record external completion"), Owner.Complete(Initial.UnitId, A.OrderId, 11));
	const FOrderRequest B = EventTestOrder(Initial.UnitId);
	EventResult(*this, TEXT("Submit HOLD for cancellation"), Owner.Submit(B, 12));
	EventResult(*this, TEXT("Cancel HOLD"), Owner.Cancel(Initial.UnitId, B.OrderId, 13));
	const FOrderRequest C = EventTestOrder(Initial.UnitId);
	EventResult(*this, TEXT("Submit HOLD for explicit failure"), Owner.Submit(C, 14));
	EventResult(*this, TEXT("Record external execution failure"), Owner.Fail(Initial.UnitId, C.OrderId, 15));
	EventResult(*this, TEXT("Destroy idle unit explicitly"), Owner.DestroyUnit(Initial.UnitId, 16));
	const TArray<FSimulationEvent> Events = Owner.GetEventLog().GetEvents();
	if (!TestEqual(TEXT("Each real transition produces its specified event"), Events.Num(), int32(12))) { return false; }
	const ESimulationEventType Types[] = {
		ESimulationEventType::UnitCreated, ESimulationEventType::UnitMoved, ESimulationEventType::UnitDamaged,
		ESimulationEventType::AmmunitionConsumed, ESimulationEventType::OrderSubmitted, ESimulationEventType::OrderStarted,
		ESimulationEventType::OrderCompleted, ESimulationEventType::OrderSubmitted, ESimulationEventType::OrderCancelled,
		ESimulationEventType::OrderSubmitted, ESimulationEventType::OrderFailed, ESimulationEventType::UnitDestroyed };
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		EventIdentity(*this, Events[Index], Types[Index], uint64(Index + 1), int64(Index + 5), Initial.UnitId);
	}
	TestTrue(TEXT("Creation has no fabricated previous unit and preserves all initial values"),
		!Events[0].UnitBefore.IsSet() && Events[0].UnitAfter.IsSet() && Events[0].UnitAfter.GetValue() == Initial
		&& Events[0].Reason == ESimulationEventReason::Created);
	TestTrue(TEXT("Move carries the exact old and new position"), Events[1].UnitBefore.IsSet() && Events[1].UnitAfter.IsSet()
		&& Events[1].UnitBefore.GetValue().Position == Initial.Position
		&& Events[1].UnitAfter.GetValue().Position == FVector(100.0, 20.0, 30.0)
		&& Events[1].Reason == ESimulationEventReason::PositionChanged);
	TestTrue(TEXT("Damage carries actual health delta"), Events[2].UnitBefore.IsSet() && Events[2].UnitAfter.IsSet()
		&& Events[2].UnitBefore.GetValue().Health == 80.0 && Events[2].UnitAfter.GetValue().Health == 70.0
		&& Events[2].Reason == ESimulationEventReason::DamageApplied);
	TestTrue(TEXT("Ammunition carries actual stock delta"), Events[3].UnitBefore.IsSet() && Events[3].UnitAfter.IsSet()
		&& Events[3].UnitBefore.GetValue().Ammunition == 12 && Events[3].UnitAfter.GetValue().Ammunition == 10
		&& Events[3].Reason == ESimulationEventReason::AmmunitionSpent);
	for (int32 Index = 0; Index < 4; ++Index) { TestFalse(TEXT("Idle unit events do not invent an order ID"), Events[Index].OrderId.IsSet()); }
	for (int32 Index : { 4, 5, 6 }) { EventOrderId(*this, Events[Index], A.OrderId); }
	for (int32 Index : { 7, 8 }) { EventOrderId(*this, Events[Index], B.OrderId); }
	for (int32 Index : { 9, 10 }) { EventOrderId(*this, Events[Index], C.OrderId); }
	TestTrue(TEXT("MOVE submission preserves kind and validated target"), Events[4].OrderKind.IsSet()
		&& Events[4].OrderKind.GetValue() == EOrderKind::Move && Events[4].TargetPosition == A.TargetPosition
		&& !Events[4].PreviousOrderStatus.IsSet() && Events[4].OrderStatus == EOrderStatus::Pending);
	TestTrue(TEXT("Start records Pending to Executing"), Events[5].PreviousOrderStatus == EOrderStatus::Pending
		&& Events[5].OrderStatus == EOrderStatus::Executing);
	TestTrue(TEXT("Completion records Executing to Completed and reason"), Events[6].PreviousOrderStatus == EOrderStatus::Executing
		&& Events[6].OrderStatus == EOrderStatus::Completed && Events[6].Reason == ESimulationEventReason::Completed);
	TestTrue(TEXT("Explicit cancellation reason is retained"), Events[8].OrderStatus == EOrderStatus::Cancelled
		&& Events[8].Reason == ESimulationEventReason::CancelledByRequest);
	TestTrue(TEXT("Explicit execution failure is a committed Failed transition"), Events[10].OrderStatus == EOrderStatus::Failed
		&& Events[10].Reason == ESimulationEventReason::ExecutionFailed);
	TestTrue(TEXT("Explicit destruction retains a tombstone and distinct reason"), Events[11].UnitBefore.IsSet() && Events[11].UnitAfter.IsSet()
		&& Events[11].UnitBefore.GetValue().Health == 70.0 && Events[11].UnitAfter.GetValue().Health == 0.0
		&& Events[11].UnitAfter.GetValue().Life == EUnitLifeState::Destroyed
		&& Events[11].Reason == ESimulationEventReason::ExplicitDestruction);
	EventAudit(*this, Owner);
	AddInfo(TEXT("Checked: structured creation, movement, damage, ammunition and order lifecycle events preserve exact ticks, stable IDs, typed reasons and actual before/after values."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogEventsOrderingTest, "FogOfCommand.Events.Ordering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogEventsOrderingTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = EventTestUnit();
	FUnitOrders Owner;
	EventResult(*this, TEXT("Creation at an explicit nonzero epoch"), Owner.Initialize(Initial, 7));
	EventUnchanged(*this, TEXT("New state cannot precede creation"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.MoveUnit(Initial.UnitId, FVector(0.0, 0.0, 0.0), 6); });
	const FOrderRequest A = EventTestOrder(Initial.UnitId);
	EventResult(*this, TEXT("Same-tick submission"), Owner.Submit(A, 7));
	EventResult(*this, TEXT("Same-tick start"), Owner.Start(Initial.UnitId, A.OrderId, 7));
	EventResult(*this, TEXT("Same-tick movement"), Owner.MoveUnit(Initial.UnitId, FVector(40.0, 20.0, 30.0), 7));
	EventResult(*this, TEXT("Same-tick damage"), Owner.ApplyDamage(Initial.UnitId, 1.0, 7));
	EventResult(*this, TEXT("Same-tick consumption"), Owner.ConsumeAmmunition(Initial.UnitId, 1, 7));
	EventResult(*this, TEXT("Same-tick completion"), Owner.Complete(Initial.UnitId, A.OrderId, 7));
	const int64 Maximum = std::numeric_limits<int64>::max();
	const FOrderRequest B = EventTestOrder(Initial.UnitId, true);
	EventResult(*this, TEXT("Maximum-tick submission"), Owner.Submit(B, Maximum));
	EventResult(*this, TEXT("Maximum-tick start"), Owner.Start(Initial.UnitId, B.OrderId, Maximum));
	EventResult(*this, TEXT("Maximum-tick completion"), Owner.Complete(Initial.UnitId, B.OrderId, Maximum));
	const TArray<FSimulationEvent> Events = Owner.GetEventLog().GetEvents();
	if (!TestEqual(TEXT("Ordered stream retains all ten changes"), Events.Num(), int32(10))) { return false; }
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		TestEqual(TEXT("Sequence disambiguates equal ticks"), Events[Index].Sequence, uint64(Index + 1));
		TestEqual(TEXT("Ticks do not increment internally or overflow"), Events[Index].SimulationTick, Index < 7 ? int64(7) : Maximum);
	}
	for (int32 Index : { 3, 4, 5 }) { EventOrderId(*this, Events[Index], A.OrderId); }
	EventUnchanged(*this, TEXT("Mutation cannot regress from maximum tick"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.MoveUnit(Initial.UnitId, FVector(90.0, 20.0, 30.0), Maximum - 1); });
	EventUnchanged(*this, TEXT("Older valid terminal retry is not a new event"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.Complete(Initial.UnitId, B.OrderId, 0); });
	EventAudit(*this, Owner);
	AddInfo(TEXT("Checked: explicit creation epoch rejects earlier changes, equal-tick events retain deterministic sequence, active OrderId accompanies resource/movement changes and INT64_MAX ticks remain exact without increments."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogEventsAtomicityTest, "FogOfCommand.Events.Atomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogEventsAtomicityTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = EventTestUnit();
	FUnitOrders Owner;
	EventUnchanged(*this, TEXT("Negative creation time"), Owner, EOrderResult::InvalidTime,
		[&] { return Owner.Initialize(Initial, -1); });
	FUnitStateSnapshot InvalidUnit = Initial; InvalidUnit.Health = -1.0;
	EventUnchanged(*this, TEXT("Invalid creation data"), Owner, EOrderResult::InvalidUnitState,
		[&] { return Owner.Initialize(InvalidUnit, 10); });
	EventResult(*this, TEXT("Atomicity fixture"), Owner.Initialize(Initial, 10));
	const FOrderRequest A = EventTestOrder(Initial.UnitId, true);
	EventResult(*this, TEXT("Active intent before refusal cases"), Owner.Submit(A, 11));
	EventResult(*this, TEXT("Executing intent before refusal cases"), Owner.Start(Initial.UnitId, A.OrderId, 12));
	const FOrderRequest B = EventTestOrder(Initial.UnitId);
	EventUnchanged(*this, TEXT("Replacement expected-ID mismatch"), Owner, EOrderResult::ExpectedOrderMismatch,
		[&] { return Owner.Replace(B, FOrderId::Create(), 13); });
	FOrderRequest Invalid = B; Invalid.Kind = EOrderKind::Move;
	EventUnchanged(*this, TEXT("Replacement without target"), Owner, EOrderResult::InvalidTarget,
		[&] { return Owner.Replace(Invalid, A.OrderId, 13); });
	FVector BadPosition = FVector::ZeroVector;
	BadPosition.X = std::numeric_limits<double>::quiet_NaN();
	EventUnchanged(*this, TEXT("Nonfinite position"), Owner, EOrderResult::InvalidPosition,
		[&] { return Owner.MoveUnit(Initial.UnitId, BadPosition, 13); });
	for (double Amount : { -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() })
	{
		EventUnchanged(*this, TEXT("Invalid damage cannot publish events"), Owner, EOrderResult::InvalidAmount,
			[&] { return Owner.ApplyDamage(Initial.UnitId, Amount, 13); });
	}
	EventUnchanged(*this, TEXT("Invalid ammunition consumption"), Owner, EOrderResult::InvalidAmount,
		[&] { return Owner.ConsumeAmmunition(Initial.UnitId, -1, 13); });
	EventUnchanged(*this, TEXT("Overspending ammunition"), Owner, EOrderResult::InsufficientAmmunition,
		[&] { return Owner.ConsumeAmmunition(Initial.UnitId, 13, 13); });
	EventUnchanged(*this, TEXT("Foreign unit destruction"), Owner, EOrderResult::UnknownUnit,
		[&] { return Owner.DestroyUnit(FUnitId::Create(), 13); });
	EventUnchanged(*this, TEXT("Unknown order completion"), Owner, EOrderResult::UnknownOrder,
		[&] { return Owner.Complete(Initial.UnitId, FOrderId::Create(), 13); });
	EventUnchanged(*this, TEXT("Internal cancellation reason cannot be requested"), Owner, EOrderResult::InvalidReason,
		[&] { return Owner.Cancel(Initial.UnitId, A.OrderId, 13, EOrderEndReason::UnitDestroyed); });
	EventUnchanged(*this, TEXT("Invalid failure reason"), Owner, EOrderResult::InvalidReason,
		[&] { return Owner.Fail(Initial.UnitId, A.OrderId, 13, EOrderEndReason::Completed); });
	EventUnchanged(*this, TEXT("Regressing lethal damage"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.ApplyDamage(Initial.UnitId, 1000.0, 11); });
	EventUnchanged(*this, TEXT("Regressing destruction"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.DestroyUnit(Initial.UnitId, 11); });
	EventUnchanged(*this, TEXT("Regressing replacement"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.Replace(B, A.OrderId, 11); });
	EventUnchanged(*this, TEXT("Regressing position change"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.MoveUnit(Initial.UnitId, FVector::ZeroVector, 11); });
	EventUnchanged(*this, TEXT("Regressing ammunition change"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.ConsumeAmmunition(Initial.UnitId, 1, 11); });
	EventUnchanged(*this, TEXT("Negative-time retry"), Owner, EOrderResult::InvalidTime,
		[&] { return Owner.Start(Initial.UnitId, A.OrderId, -1); });
	EventResult(*this, TEXT("All refused future operations leave tick thirteen available"), Owner.Complete(Initial.UnitId, A.OrderId, 13));
	EventUnchanged(*this, TEXT("Conflicting terminal outcome"), Owner, EOrderResult::TerminalConflict,
		[&] { return Owner.Fail(Initial.UnitId, A.OrderId, 14); });
	TestEqual(TEXT("Only create/submit/start/complete were recorded"), Owner.GetEventLog().GetTotalRecorded(), uint64(4));
	EventAudit(*this, Owner);
	AddInfo(TEXT("Checked: invalid IDs, geometry, damage, consumption, reasons, transitions and regressing time preserve all unit/order fields, mutation clock, retained events, sequence and eviction counters."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogEventsReplacementAndDestructionTest, "FogOfCommand.Events.ReplacementAndDestruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogEventsReplacementAndDestructionTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	for (bool bExecuting : { false, true })
	{
		for (bool bLethalDamage : { false, true })
		{
			const FUnitStateSnapshot Initial = EventTestUnit();
			FUnitOrders Owner;
			EventResult(*this, TEXT("Compound-operation fixture"), Owner.Initialize(Initial));
			const FOrderRequest A = EventTestOrder(Initial.UnitId, true);
			const FOrderRequest B = EventTestOrder(Initial.UnitId);
			EventResult(*this, TEXT("Submit old intent"), Owner.Submit(A, 1));
			if (bExecuting) { EventResult(*this, TEXT("Start old intent"), Owner.Start(Initial.UnitId, A.OrderId, 2)); }
			const int32 BeforeReplacement = Owner.GetEventLog().Num();
			// Passing a borrowed ID must remain safe when the CAS replaces its storage.
			EventResult(*this, TEXT("Atomically replace using a borrowed current ID"),
				Owner.Replace(B, Owner.GetUnit().GetSnapshot().CurrentOrderId.GetValue(), 3));
			TArray<FSimulationEvent> Events = Owner.GetEventLog().GetEvents();
			if (!TestEqual(TEXT("Replacement publishes exactly its three ordered records"), Events.Num(), BeforeReplacement + 3)) { return false; }
			const FSimulationEvent& Cancelled = Events[BeforeReplacement];
			const FSimulationEvent& Submitted = Events[BeforeReplacement + 1];
			const FSimulationEvent& Replaced = Events[BeforeReplacement + 2];
			EventIdentity(*this, Cancelled, ESimulationEventType::OrderCancelled, uint64(BeforeReplacement + 1), 3, Initial.UnitId);
			EventIdentity(*this, Submitted, ESimulationEventType::OrderSubmitted, uint64(BeforeReplacement + 2), 3, Initial.UnitId);
			EventIdentity(*this, Replaced, ESimulationEventType::OrderReplaced, uint64(BeforeReplacement + 3), 3, Initial.UnitId);
			EventOrderId(*this, Cancelled, A.OrderId);
			EventOrderId(*this, Submitted, B.OrderId);
			EventOrderId(*this, Replaced, A.OrderId);
			TestTrue(TEXT("Replacement connects old and new stable order IDs"), Replaced.RelatedOrderId == B.OrderId
				&& Replaced.Reason == ESimulationEventReason::Replaced && Cancelled.Reason == ESimulationEventReason::Replaced);
			TestTrue(TEXT("Cancellation records the actual old status"), Cancelled.PreviousOrderStatus
				== (bExecuting ? EOrderStatus::Executing : EOrderStatus::Pending) && Cancelled.OrderStatus == EOrderStatus::Cancelled);
			EventUnchanged(*this, TEXT("Replacement retry is not another cancellation/submission"), Owner, EOrderResult::AlreadyExists,
				[&] { return Owner.Replace(B, A.OrderId, 100); });
			if (bExecuting) { EventResult(*this, TEXT("Start replacement before death"), Owner.Start(Initial.UnitId, B.OrderId, 4)); }
			const int32 BeforeDeath = Owner.GetEventLog().Num();
			EventResult(*this, TEXT("Commit death with active order"), bLethalDamage
				? Owner.ApplyDamage(Initial.UnitId, 1000.0, 5) : Owner.DestroyUnit(Initial.UnitId, 5));
			Events = Owner.GetEventLog().GetEvents();
			const int32 Added = bLethalDamage ? 3 : 2;
			if (!TestEqual(TEXT("Death publishes only actual damage/cancellation/destruction"), Events.Num(), BeforeDeath + Added)) { return false; }
			if (bLethalDamage)
			{
				EventIdentity(*this, Events[BeforeDeath], ESimulationEventType::UnitDamaged, uint64(BeforeDeath + 1), 5, Initial.UnitId);
				EventOrderId(*this, Events[BeforeDeath], B.OrderId);
				TestTrue(TEXT("Lethal damage logs the real health reduction, not requested overshoot"), Events[BeforeDeath].UnitBefore.IsSet()
					&& Events[BeforeDeath].UnitAfter.IsSet() && Events[BeforeDeath].UnitBefore.GetValue().Health == 80.0
					&& Events[BeforeDeath].UnitAfter.GetValue().Health == 0.0);
			}
			const int32 CancellationIndex = BeforeDeath + (bLethalDamage ? 1 : 0);
			EventIdentity(*this, Events[CancellationIndex], ESimulationEventType::OrderCancelled, uint64(CancellationIndex + 1), 5, Initial.UnitId);
			EventOrderId(*this, Events[CancellationIndex], B.OrderId);
			TestTrue(TEXT("Death cancellation has a distinct typed reason"), Events[CancellationIndex].Reason == ESimulationEventReason::UnitDestroyed);
			const FSimulationEvent& Death = Events.Last();
			EventIdentity(*this, Death, ESimulationEventType::UnitDestroyed, uint64(Events.Num()), 5, Initial.UnitId);
			TestTrue(TEXT("Destruction cause distinguishes explicit from damage"), Death.Reason == (bLethalDamage
				? ESimulationEventReason::DamageApplied : ESimulationEventReason::ExplicitDestruction));
			TestTrue(TEXT("Tombstone and active-link clear commit together"), Owner.GetUnit().IsDestroyed()
				&& !Owner.GetUnit().GetSnapshot().CurrentOrderId.IsSet() && Owner.FindOrder(B.OrderId)
				&& Owner.FindOrder(B.OrderId)->EndReason == EOrderEndReason::UnitDestroyed);
			EventUnchanged(*this, TEXT("Repeated death cannot append another terminal event"), Owner, EOrderResult::AlreadyApplied,
				[&] { return Owner.DestroyUnit(Initial.UnitId, 100); });
			EventAudit(*this, Owner);
		}
	}
	AddInfo(TEXT("Checked: Pending/Executing replacement emits old cancellation then new submission then linked replacement; explicit and lethal destruction cancel the active order before the terminal death event, with stable reasons and atomic retries."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogEventsRepeatedOperationsTest, "FogOfCommand.Events.RepeatedOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogEventsRepeatedOperationsTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = EventTestUnit();
	FUnitOrders Owner;
	EventResult(*this, TEXT("No-op fixture"), Owner.Initialize(Initial));
	FUnitStateSnapshot Refill = Initial; Refill.Ammunition = 20; Refill.Health = 100.0;
	EventUnchanged(*this, TEXT("Initialization retry cannot refill or log a second creation"), Owner, EOrderResult::AlreadyExists,
		[&] { return Owner.Initialize(Refill, 100); });
	for (int32 Index = 0; Index < 64; ++Index)
	{
		EventUnchanged(*this, TEXT("Unchanged position does not spam events"), Owner, EOrderResult::AlreadyApplied,
			[&] { return Owner.MoveUnit(Initial.UnitId, Initial.Position, 100); });
		EventUnchanged(*this, TEXT("Zero damage does not spam events"), Owner, EOrderResult::AlreadyApplied,
			[&] { return Owner.ApplyDamage(Initial.UnitId, 0.0, 100); });
		EventUnchanged(*this, TEXT("Rounded-away positive damage is still a no-op"), Owner, EOrderResult::AlreadyApplied,
			[&] { return Owner.ApplyDamage(Initial.UnitId, 1.0e-300, 100); });
		EventUnchanged(*this, TEXT("Zero consumption does not spam events"), Owner, EOrderResult::AlreadyApplied,
			[&] { return Owner.ConsumeAmmunition(Initial.UnitId, 0, 100); });
	}
	TestEqual(TEXT("Repeated no-op commands leave only the creation event"), Owner.GetEventLog().Num(), int32(1));
	const FOrderRequest A = EventTestOrder(Initial.UnitId, true);
	EventResult(*this, TEXT("No-op future requests did not advance mutation time"), Owner.Submit(A, 1));
	for (int64 RetryAt : { int64(0), int64(100) })
	{
		EventUnchanged(*this, TEXT("Duplicate intent has no event"), Owner, EOrderResult::AlreadyExists,
			[&] { return Owner.Submit(A, RetryAt); });
	}
	EventResult(*this, TEXT("Start the single intent"), Owner.Start(Initial.UnitId, A.OrderId, 2));
	EventUnchanged(*this, TEXT("Duplicate start has no event"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.Start(Initial.UnitId, A.OrderId, 100); });
	EventResult(*this, TEXT("Complete once"), Owner.Complete(Initial.UnitId, A.OrderId, 3));
	const FOrderRequest B = EventTestOrder(Initial.UnitId);
	EventResult(*this, TEXT("Submit a newer order"), Owner.Submit(B, 4));
	EventUnchanged(*this, TEXT("Late completion cannot clear or log against the new link"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.Complete(Initial.UnitId, A.OrderId, 100); });
	EventResult(*this, TEXT("Cancel newer order once"), Owner.Cancel(Initial.UnitId, B.OrderId, 5));
	EventUnchanged(*this, TEXT("Repeated cancellation has no event"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.Cancel(Initial.UnitId, B.OrderId, 100); });
	const FOrderRequest C = EventTestOrder(Initial.UnitId);
	EventResult(*this, TEXT("Submit final intent"), Owner.Submit(C, 6));
	EventResult(*this, TEXT("Fail once"), Owner.Fail(Initial.UnitId, C.OrderId, 7));
	EventUnchanged(*this, TEXT("Repeated failure has no event"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.Fail(Initial.UnitId, C.OrderId, 100); });
	const FEventObservation BeforeRead = ObserveEvents(Owner);
	TArray<uint8> Bytes;
	TestTrue(TEXT("Existing v1 serializer remains a read operation"), Owner.GetUnit().Serialize(Bytes) == EUnitStateResult::Success
		&& Bytes.Num() == FUnitState::SerializedSize && MatchesEvents(Owner, BeforeRead));
	EventResult(*this, TEXT("Explicit destruction once"), Owner.DestroyUnit(Initial.UnitId, 8));
	EventUnchanged(*this, TEXT("Repeated destruction has no event"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.DestroyUnit(Initial.UnitId, 100); });
	TestEqual(TEXT("Only nine actual transitions were recorded"), Owner.GetEventLog().GetTotalRecorded(), uint64(9));
	EventAudit(*this, Owner);
	AddInfo(TEXT("Checked: repeated initialization, unchanged movement, zero/rounded damage, zero consumption, submission/start/terminal retries and read-only serialization append nothing and never advance time or clear a newer order."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogEventsRetentionTest, "FogOfCommand.Events.Retention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogEventsRetentionTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = EventTestUnit();
	FUnitOrders Owner;
	EventResult(*this, TEXT("Retention fixture"), Owner.Initialize(Initial));
	const int32 Moves = FSimulationEventLog::Capacity + 32;
	for (int32 Index = 1; Index <= Moves; ++Index)
	{
		EventResult(*this, TEXT("Append a real change into the bounded ring"), Owner.MoveUnit(Initial.UnitId, FVector(double(Index), 0.0, 0.0), Index));
		TestTrue(TEXT("Retention remains bounded during append"), Owner.GetEventLog().Num() <= FSimulationEventLog::Capacity);
	}
	const uint64 Total = uint64(Moves + 1);
	TArray<FSimulationEvent> Events = Owner.GetEventLog().GetEvents();
	if (!TestEqual(TEXT("Ring retains exactly its configured capacity"), Events.Num(), int32(FSimulationEventLog::Capacity))) { return false; }
	TestEqual(TEXT("Total records survive eviction accounting"), Owner.GetEventLog().GetTotalRecorded(), Total);
	TestEqual(TEXT("Evicted count is explicit"), Owner.GetEventLog().GetEvictedCount(), Total - uint64(FSimulationEventLog::Capacity));
	TestEqual(TEXT("Oldest retained sequence makes dropped records detectable"), Events[0].Sequence, Total - uint64(FSimulationEventLog::Capacity) + 1);
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		const uint64 ExpectedSequence = Total - uint64(FSimulationEventLog::Capacity) + uint64(Index) + 1;
		EventIdentity(*this, Events[Index], ESimulationEventType::UnitMoved, ExpectedSequence, int64(ExpectedSequence - 1), Initial.UnitId);
		TestTrue(TEXT("Retained payload is a stable value, not an alias to the latest unit"), Events[Index].UnitAfter.IsSet()
			&& Events[Index].UnitAfter.GetValue().Position == FVector(double(ExpectedSequence - 1), 0.0, 0.0));
	}
	Events[0].SimulationTick = -100;
	Events[0].UnitId = FUnitId();
	Events.Reset();
	TestTrue(TEXT("Editing a returned copy cannot edit the journal"), Owner.GetEventLog().GetEvents()[0].SimulationTick >= 0
		&& Owner.GetEventLog().GetEvents()[0].UnitId == Initial.UnitId && Owner.GetEventLog().Num() == FSimulationEventLog::Capacity);
	EventUnchanged(*this, TEXT("A no-op cannot evict another retained event"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.MoveUnit(Initial.UnitId, Owner.GetUnit().GetSnapshot().Position, Moves + 100); });
	EventUnchanged(*this, TEXT("A rejected command cannot evict another retained event"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.MoveUnit(Initial.UnitId, FVector::ZeroVector, Moves - 1); });
	EventAudit(*this, Owner);
	AddInfo(TEXT("Checked: ring storage retains only its bounded latest events, reports explicit eviction/total counts, preserves monotonic sequences and copied payloads, and refused/no-op commands cannot append or evict."));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
