#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Domain/Orders.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace
{
using namespace FogOfCommand::Domain;

static_assert(!std::is_copy_constructible_v<FUnitOrders>);
static_assert(!std::is_move_constructible_v<FUnitOrders>);
static_assert(!std::is_copy_assignable_v<FUnitOrders>);
static_assert(!std::is_move_assignable_v<FUnitOrders>);
static_assert(std::is_same_v<decltype(std::declval<FUnitOrders&>().GetUnit()), const FUnitState&>);
static_assert(std::is_same_v<decltype(std::declval<FUnitOrders&>().GetHistory()), const TMap<FOrderId, FOrderSnapshot>&>);

FUnitStateSnapshot MakeOrderTestUnit()
{
	FUnitStateSnapshot Unit;
	Unit.UnitId = FUnitId::Create();
	Unit.SideId = FSideId::FromValue(2);
	Unit.Position = FVector(100.0, -200.0, 30.0);
	Unit.Health = 80.0;
	Unit.MaxHealth = 100.0;
	Unit.Ammunition = 12;
	Unit.AmmoCapacity = 20;
	return Unit;
}

FOrderRequest MakeMoveRequest(const FUnitId& UnitId)
{
	FOrderRequest Request;
	Request.OrderId = FOrderId::Create();
	Request.UnitId = UnitId;
	Request.Kind = EOrderKind::Move;
	Request.TargetPosition = FVector(400.0, 250.0, 0.0);
	return Request;
}

FOrderRequest MakeHoldRequest(const FUnitId& UnitId)
{
	FOrderRequest Request;
	Request.OrderId = FOrderId::Create();
	Request.UnitId = UnitId;
	Request.Kind = EOrderKind::Hold;
	return Request;
}

bool OrderSameVector(const FVector& A, const FVector& B)
{
	return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
}

bool OrderSameUnit(const FUnitStateSnapshot& A, const FUnitStateSnapshot& B)
{
	return A.Version == B.Version && A.UnitId == B.UnitId && A.SideId == B.SideId
		&& OrderSameVector(A.Position, B.Position) && A.Health == B.Health && A.MaxHealth == B.MaxHealth
		&& A.Ammunition == B.Ammunition && A.AmmoCapacity == B.AmmoCapacity && A.Life == B.Life
		&& A.CurrentOrderId == B.CurrentOrderId;
}

bool OrderSameRecord(const FOrderSnapshot& A, const FOrderSnapshot& B)
{
	const bool bSameTarget = A.Request.TargetPosition.IsSet() == B.Request.TargetPosition.IsSet()
		&& (!A.Request.TargetPosition.IsSet() || OrderSameVector(A.Request.TargetPosition.GetValue(), B.Request.TargetPosition.GetValue()));
	return A.Request.OrderId == B.Request.OrderId && A.Request.UnitId == B.Request.UnitId
		&& A.Request.Kind == B.Request.Kind && bSameTarget && A.Status == B.Status
		&& A.CreatedAt == B.CreatedAt && A.StartedAt == B.StartedAt && A.UpdatedAt == B.UpdatedAt
		&& A.FinishedAt == B.FinishedAt && A.EndReason == B.EndReason;
}

struct FOrderTestObservation
{
	FUnitStateSnapshot Unit;
	bool bInitialized;
	bool bAlive;
	bool bDestroyed;
	TMap<FOrderId, FOrderSnapshot> History;
	TOptional<int64> LastMutationTime;
};

FOrderTestObservation ObserveOrders(const FUnitOrders& Owner)
{
	return { Owner.GetUnit().GetSnapshot(), Owner.GetUnit().IsInitialized(), Owner.GetUnit().IsAlive(),
		Owner.GetUnit().IsDestroyed(), Owner.GetHistory(), Owner.GetLastMutationTime() };
}

bool OrdersMatchObservation(const FUnitOrders& Owner, const FOrderTestObservation& Before)
{
	if (!OrderSameUnit(Owner.GetUnit().GetSnapshot(), Before.Unit)
		|| Owner.GetUnit().IsInitialized() != Before.bInitialized || Owner.GetUnit().IsAlive() != Before.bAlive
		|| Owner.GetUnit().IsDestroyed() != Before.bDestroyed || Owner.GetLastMutationTime() != Before.LastMutationTime
		|| Owner.GetHistory().Num() != Before.History.Num())
	{
		return false;
	}
	for (const auto& Pair : Before.History)
	{
		const FOrderSnapshot* Current = Owner.FindOrder(Pair.Key);
		if (!Current || !OrderSameRecord(*Current, Pair.Value)) { return false; }
	}
	return true;
}

bool OrderCheckResult(FAutomationTestBase& Test, const TCHAR* Description, EOrderResult Actual, EOrderResult Expected)
{
	return Test.TestEqual(Description, static_cast<int32>(Actual), static_cast<int32>(Expected));
}

bool OrderCheckUnitResult(FAutomationTestBase& Test, const TCHAR* Description, EUnitStateResult Actual, EUnitStateResult Expected)
{
	return Test.TestEqual(Description, static_cast<int32>(Actual), static_cast<int32>(Expected));
}

template <typename OperationType>
void OrderExpectUnchanged(FAutomationTestBase& Test, const TCHAR* Description, FUnitOrders& Owner,
	EOrderResult Expected, OperationType Operation)
{
	const FOrderTestObservation Before = ObserveOrders(Owner);
	OrderCheckResult(Test, Description, Operation(), Expected);
	Test.TestTrue(*(FString(Description) + TEXT(" preserves the full unit, history and clock")), OrdersMatchObservation(Owner, Before));
}

template <typename OperationType>
void OrderExpectUnitUnchanged(FAutomationTestBase& Test, const TCHAR* Description, FUnitState& Unit,
	EUnitStateResult Expected, OperationType Operation)
{
	const FUnitStateSnapshot Before = Unit.GetSnapshot();
	const bool bInitialized = Unit.IsInitialized();
	OrderCheckUnitResult(Test, Description, Operation(), Expected);
	Test.TestTrue(*(FString(Description) + TEXT(" preserves every unit field")),
		OrderSameUnit(Unit.GetSnapshot(), Before) && Unit.IsInitialized() == bInitialized);
}

FOrderSnapshot ReadOrderValue(FAutomationTestBase& Test, const FUnitOrders& Owner, const FOrderId& Id)
{
	const FOrderSnapshot* Record = Owner.FindOrder(Id);
	if (!Test.TestNotNull(TEXT("Expected history record exists"), Record)) { return {}; }
	// Do not retain a borrowed TMap pointer across any command that may mutate it.
	return *Record;
}

void CheckOrderInvariant(FAutomationTestBase& Test, const FUnitOrders& Owner)
{
	OrderCheckResult(Test, TEXT("Owner invariant validation"), Owner.Validate(), EOrderResult::Success);
	int32 Active = 0;
	FOrderId ActiveId;
	for (const auto& Pair : Owner.GetHistory())
	{
		Test.TestTrue(TEXT("History key equals its stable OrderId"), Pair.Key == Pair.Value.Request.OrderId);
		Test.TestTrue(TEXT("Every record belongs to the owned unit"), Pair.Value.Request.UnitId == Owner.GetUnit().GetSnapshot().UnitId);
		if (Pair.Value.Status == EOrderStatus::Pending || Pair.Value.Status == EOrderStatus::Executing)
		{
			++Active;
			ActiveId = Pair.Key;
			Test.TestFalse(TEXT("Pending/executing is nonterminal"), Pair.Value.IsTerminal());
		}
		else { Test.TestTrue(TEXT("Finished records are terminal"), Pair.Value.IsTerminal()); }
	}
	const TOptional<FOrderId>& Link = Owner.GetUnit().GetSnapshot().CurrentOrderId;
	Test.TestTrue(TEXT("At most one Pending/Executing order"), Active <= 1);
	Test.TestTrue(TEXT("Exactly one active order matches the unit link, or both are absent"),
		(Active == 0 && !Link.IsSet()) || (Active == 1 && Link.IsSet() && Link.GetValue() == ActiveId));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOrdersValidationTest, "FogOfCommand.Orders.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOrdersValidationTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = MakeOrderTestUnit();
	FUnitOrders Empty;
	const FOrderRequest ValidMove = MakeMoveRequest(Initial.UnitId);
	OrderExpectUnchanged(*this, TEXT("Submission without a unit"), Empty, EOrderResult::NotInitialized,
		[&] { return Empty.Submit(ValidMove, 0); });
	FUnitOrders Owner;
	if (!OrderCheckResult(*this, TEXT("Validation fixture"), Owner.Initialize(Initial), EOrderResult::Success)) { return false; }
	auto Reject = [&](const TCHAR* Description, const FOrderRequest& Request, EOrderResult Expected)
	{
		OrderCheckResult(*this, Description, FUnitOrders::ValidateRequest(Request), Expected);
		OrderExpectUnchanged(*this, Description, Owner, Expected, [&] { return Owner.Submit(Request, 10); });
	};
	FOrderRequest Bad = ValidMove; Bad.OrderId = FOrderId();
	Reject(TEXT("Zero OrderId"), Bad, EOrderResult::InvalidOrderId);
	Bad = ValidMove; Bad.UnitId = FUnitId();
	Reject(TEXT("Zero UnitId"), Bad, EOrderResult::InvalidUnitId);
	Bad = ValidMove; Bad.Kind = static_cast<EOrderKind>(255);
	Reject(TEXT("Unsupported order kind"), Bad, EOrderResult::InvalidKind);
	Bad = ValidMove; Bad.TargetPosition.Reset();
	Reject(TEXT("Move requires a target"), Bad, EOrderResult::InvalidTarget);
	Bad = MakeHoldRequest(Initial.UnitId); Bad.TargetPosition = FVector::ZeroVector;
	Reject(TEXT("Hold forbids even a zero target"), Bad, EOrderResult::InvalidTarget);
	for (double Value : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() })
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			FVector Target = FVector::ZeroVector;
			Target[Axis] = Value;
			Bad = ValidMove; Bad.TargetPosition = Target;
			Reject(TEXT("Nonfinite target coordinate"), Bad, EOrderResult::InvalidTarget);
		}
	}
	const FOrderRequest Foreign = MakeHoldRequest(FUnitId::Create());
	OrderExpectUnchanged(*this, TEXT("Unknown valid UnitId"), Owner, EOrderResult::UnknownUnit,
		[&] { return Owner.Submit(Foreign, 10); });
	const FOrderId UnknownOrder = FOrderId::Create();
	OrderExpectUnchanged(*this, TEXT("Unknown valid OrderId"), Owner, EOrderResult::UnknownOrder,
		[&] { return Owner.Start(Initial.UnitId, UnknownOrder, 0); });
	OrderCheckResult(*this, TEXT("Submit a valid Move"), Owner.Submit(ValidMove, 10), EOrderResult::Success);
	OrderExpectUnchanged(*this, TEXT("Known order cannot be addressed through another unit"), Owner, EOrderResult::UnknownUnit,
		[&] { return Owner.Start(Foreign.UnitId, ValidMove.OrderId, 11); });
	for (EOrderEndReason Reason : { EOrderEndReason::None, EOrderEndReason::Completed, EOrderEndReason::Replaced,
		EOrderEndReason::UnitDestroyed, EOrderEndReason::ExecutionFailed, static_cast<EOrderEndReason>(255) })
	{
		OrderExpectUnchanged(*this, TEXT("Public Cancel rejects invalid/internal reasons"), Owner, EOrderResult::InvalidReason,
			[&] { return Owner.Cancel(Initial.UnitId, ValidMove.OrderId, 11, Reason); });
	}
	for (EOrderEndReason Reason : { EOrderEndReason::None, EOrderEndReason::Completed, EOrderEndReason::CancelledByRequest,
		EOrderEndReason::Replaced, EOrderEndReason::UnitDestroyed, static_cast<EOrderEndReason>(255) })
	{
		OrderExpectUnchanged(*this, TEXT("Public Fail rejects invalid/internal reasons"), Owner, EOrderResult::InvalidReason,
			[&] { return Owner.Fail(Initial.UnitId, ValidMove.OrderId, 11, Reason); });
	}
	CheckOrderInvariant(*this, Owner);
	AddInfo(TEXT("Checked: missing/unknown identities, order kind, MOVE/HOLD target shape, NaN/Inf coordinates and invalid/internal end reasons reject without changing unit, history or clock."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOrdersTransitionsTest, "FogOfCommand.Orders.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOrdersTransitionsTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	for (EOrderKind Kind : { EOrderKind::Move, EOrderKind::Hold })
	{
		const FUnitStateSnapshot Initial = MakeOrderTestUnit();
		FUnitOrders Owner;
		OrderCheckResult(*this, TEXT("Transition fixture"), Owner.Initialize(Initial), EOrderResult::Success);
		const FOrderRequest Request = Kind == EOrderKind::Move ? MakeMoveRequest(Initial.UnitId) : MakeHoldRequest(Initial.UnitId);
		OrderCheckResult(*this, TEXT("Submit Pending"), Owner.Submit(Request, 2), EOrderResult::Success);
		FOrderSnapshot Record = ReadOrderValue(*this, Owner, Request.OrderId);
		TestTrue(TEXT("Initial Pending state/timestamps/reason"), Record.Status == EOrderStatus::Pending
			&& Record.CreatedAt == 2 && Record.UpdatedAt == 2 && !Record.StartedAt.IsSet()
			&& !Record.FinishedAt.IsSet() && Record.EndReason == EOrderEndReason::None);
		OrderExpectUnchanged(*this, TEXT("Pending cannot jump to Completed"), Owner, EOrderResult::InvalidTransition,
			[&] { return Owner.Complete(Initial.UnitId, Request.OrderId, 3); });
		OrderCheckResult(*this, TEXT("Start Executing"), Owner.Start(Initial.UnitId, Request.OrderId, 3), EOrderResult::Success);
		Record = ReadOrderValue(*this, Owner, Request.OrderId);
		TestTrue(TEXT("Executing records start time without finishing"), Record.Status == EOrderStatus::Executing
			&& Record.CreatedAt == 2 && Record.UpdatedAt == 3 && Record.StartedAt.IsSet() && Record.StartedAt.GetValue() == 3
			&& !Record.FinishedAt.IsSet() && Record.EndReason == EOrderEndReason::None);
		OrderCheckResult(*this, TEXT("Complete Executing"), Owner.Complete(Initial.UnitId, Request.OrderId, 4), EOrderResult::Success);
		Record = ReadOrderValue(*this, Owner, Request.OrderId);
		TestTrue(TEXT("Completed reason/time are explicit"), Record.Status == EOrderStatus::Completed
			&& Record.EndReason == EOrderEndReason::Completed && Record.CreatedAt == 2
			&& Record.StartedAt.IsSet() && Record.StartedAt.GetValue() == 3 && Record.UpdatedAt == 4
			&& Record.FinishedAt.IsSet() && Record.FinishedAt.GetValue() == 4 && Record.IsTerminal());
		TestTrue(TEXT("Order transitions do not move, heal or resupply the unit"), OrderSameUnit(Owner.GetUnit().GetSnapshot(), Initial));
		OrderExpectUnchanged(*this, TEXT("A completed record cannot be cancelled"), Owner, EOrderResult::TerminalConflict,
			[&] { return Owner.Cancel(Initial.UnitId, Request.OrderId, 5); });
		OrderExpectUnchanged(*this, TEXT("A completed record cannot fail later"), Owner, EOrderResult::TerminalConflict,
			[&] { return Owner.Fail(Initial.UnitId, Request.OrderId, 5); });
		CheckOrderInvariant(*this, Owner);
	}
	for (bool bFail : { false, true })
	{
		const FUnitStateSnapshot Initial = MakeOrderTestUnit();
		FUnitOrders Owner;
		OrderCheckResult(*this, TEXT("Pending terminal fixture"), Owner.Initialize(Initial), EOrderResult::Success);
		const FOrderRequest Request = MakeHoldRequest(Initial.UnitId);
		OrderCheckResult(*this, TEXT("Submit before direct cancellation/failure"), Owner.Submit(Request, 5), EOrderResult::Success);
		OrderCheckResult(*this, TEXT("Pending may cancel or fail"), bFail ? Owner.Fail(Initial.UnitId, Request.OrderId, 6)
			: Owner.Cancel(Initial.UnitId, Request.OrderId, 6), EOrderResult::Success);
		const FOrderSnapshot Record = ReadOrderValue(*this, Owner, Request.OrderId);
		TestTrue(TEXT("Pending terminal retains absent start time"), !Record.StartedAt.IsSet()
			&& Record.FinishedAt.IsSet() && Record.FinishedAt.GetValue() == 6 && Record.UpdatedAt == 6);
		TestTrue(TEXT("Typed terminal outcome/reason"), Record.Status == (bFail ? EOrderStatus::Failed : EOrderStatus::Cancelled)
			&& Record.EndReason == (bFail ? EOrderEndReason::ExecutionFailed : EOrderEndReason::CancelledByRequest));
		CheckOrderInvariant(*this, Owner);
	}
	AddInfo(TEXT("Checked: MOVE/HOLD Pending->Executing->Completed, direct Pending cancel/fail, explicit reasons/times, no navigation side effects and immutable conflicting terminal outcomes."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOrdersAssignmentTest, "FogOfCommand.Orders.Assignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOrdersAssignmentTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = MakeOrderTestUnit();
	FUnitOrders Owner;
	if (!OrderCheckResult(*this, TEXT("Assignment fixture"), Owner.Initialize(Initial), EOrderResult::Success)) { return false; }
	const FOrderRequest Move = MakeMoveRequest(Initial.UnitId);
	const FOrderRequest Hold = MakeHoldRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("Assign the first current order"), Owner.Submit(Move, 4), EOrderResult::Success);
	TestTrue(TEXT("CurrentOrderId points to the assigned stable ID"), Owner.GetUnit().GetSnapshot().CurrentOrderId.IsSet()
		&& Owner.GetUnit().GetSnapshot().CurrentOrderId.GetValue() == Move.OrderId);
	OrderExpectUnchanged(*this, TEXT("A second Submit cannot silently replace the active order"), Owner, EOrderResult::ActiveOrderExists,
		[&] { return Owner.Submit(Hold, 5); });
	OrderCheckResult(*this, TEXT("Start assigned order"), Owner.Start(Initial.UnitId, Move.OrderId, 5), EOrderResult::Success);
	const FOrderSnapshot BeforeDamage = ReadOrderValue(*this, Owner, Move.OrderId);
	OrderCheckResult(*this, TEXT("Nonlethal unit damage"), Owner.ApplyDamage(Initial.UnitId, 5.0, 6), EOrderResult::Success);
	TestEqual(TEXT("Damage applies to the same unit"), Owner.GetUnit().GetSnapshot().Health, 75.0);
	TestTrue(TEXT("Nonlethal damage does not cancel/restart/rewrite the order"), OrderSameRecord(ReadOrderValue(*this, Owner, Move.OrderId), BeforeDamage));
	TestTrue(TEXT("Identity, position and ammunition remain stable"), Owner.GetUnit().GetSnapshot().UnitId == Initial.UnitId
		&& Owner.GetUnit().GetSnapshot().SideId == Initial.SideId && OrderSameVector(Owner.GetUnit().GetSnapshot().Position, Initial.Position)
		&& Owner.GetUnit().GetSnapshot().Ammunition == Initial.Ammunition
		&& Owner.GetUnit().GetSnapshot().CurrentOrderId.IsSet()
		&& Owner.GetUnit().GetSnapshot().CurrentOrderId.GetValue() == Move.OrderId);
	OrderExpectUnchanged(*this, TEXT("Zero damage does not change the order or advance the clock"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.ApplyDamage(Initial.UnitId, 0.0, 100); });
	const FOrderTestObservation BeforeRead = ObserveOrders(Owner);
	const FUnitStateSnapshot UnitView = Owner.GetUnit().GetSnapshot();
	const FOrderSnapshot OrderView = ReadOrderValue(*this, Owner, Move.OrderId);
	TestTrue(TEXT("Controller-independent value views preserve active identity"), UnitView.CurrentOrderId.IsSet()
		&& UnitView.CurrentOrderId.GetValue() == OrderView.Request.OrderId && OrdersMatchObservation(Owner, BeforeRead));
	FUnitStateSnapshot ExpectedAfterCompletion = Owner.GetUnit().GetSnapshot();
	ExpectedAfterCompletion.CurrentOrderId.Reset();
	OrderCheckResult(*this, TEXT("Complete clears only this matching link"), Owner.Complete(Initial.UnitId, Move.OrderId, 7), EOrderResult::Success);
	TestTrue(TEXT("Completing an order preserves all other unit fields"), OrderSameUnit(Owner.GetUnit().GetSnapshot(), ExpectedAfterCompletion));
	OrderCheckResult(*this, TEXT("Next order can be assigned after completion"), Owner.Submit(Hold, 8), EOrderResult::Success);
	TestTrue(TEXT("Next current link is the new stable OrderId"), Owner.GetUnit().GetSnapshot().CurrentOrderId.IsSet()
		&& Owner.GetUnit().GetSnapshot().CurrentOrderId.GetValue() == Hold.OrderId);
	CheckOrderInvariant(*this, Owner);
	AddInfo(TEXT("Checked: one active assignment, stable UnitId/OrderId, controller-independent value reads and nonlethal damage preserve execution; completion clears only the matching current link."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOrdersReplacementTest, "FogOfCommand.Orders.Replacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOrdersReplacementTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = MakeOrderTestUnit();
	FUnitOrders Owner;
	OrderCheckResult(*this, TEXT("Replacement fixture"), Owner.Initialize(Initial), EOrderResult::Success);
	const FOrderRequest A = MakeMoveRequest(Initial.UnitId);
	const FOrderRequest B = MakeHoldRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("Submit old order"), Owner.Submit(A, 10), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("Execute old order"), Owner.Start(Initial.UnitId, A.OrderId, 11), EOrderResult::Success);
	FOrderRequest Bad = MakeMoveRequest(Initial.UnitId);
	FVector BadTarget = FVector::ZeroVector; BadTarget.X = std::numeric_limits<double>::quiet_NaN();
	Bad.TargetPosition = BadTarget;
	OrderExpectUnchanged(*this, TEXT("Invalid new target cannot partially cancel the old order"), Owner, EOrderResult::InvalidTarget,
		[&] { return Owner.Replace(Bad, A.OrderId, 12); });
	const FOrderId WrongExpected = FOrderId::Create();
	OrderExpectUnchanged(*this, TEXT("Replacement requires the expected current ID"), Owner, EOrderResult::ExpectedOrderMismatch,
		[&] { return Owner.Replace(B, WrongExpected, 12); });
	OrderExpectUnchanged(*this, TEXT("Regressing replacement cannot cancel first"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.Replace(B, A.OrderId, 10); });
	const FOrderSnapshot BeforeA = ReadOrderValue(*this, Owner, A.OrderId);
	OrderCheckResult(*this, TEXT("Atomic replacement"), Owner.Replace(B, A.OrderId, 12), EOrderResult::Success);
	const FOrderSnapshot Old = ReadOrderValue(*this, Owner, A.OrderId);
	const FOrderSnapshot New = ReadOrderValue(*this, Owner, B.OrderId);
	TestTrue(TEXT("Old execution is cancelled for Replaced at the replacement tick"), Old.Status == EOrderStatus::Cancelled
		&& Old.EndReason == EOrderEndReason::Replaced && Old.FinishedAt.IsSet() && Old.FinishedAt.GetValue() == 12
		&& Old.UpdatedAt == 12 && Old.CreatedAt == BeforeA.CreatedAt && Old.StartedAt == BeforeA.StartedAt);
	TestTrue(TEXT("New intent starts Pending at that same tick"), New.Status == EOrderStatus::Pending
		&& New.CreatedAt == 12 && New.UpdatedAt == 12 && !New.StartedAt.IsSet() && !New.FinishedAt.IsSet());
	TestTrue(TEXT("New order owns the only current link"), Owner.GetUnit().GetSnapshot().CurrentOrderId.IsSet()
		&& Owner.GetUnit().GetSnapshot().CurrentOrderId.GetValue() == B.OrderId && Owner.GetHistory().Num() == 2);
	OrderExpectUnchanged(*this, TEXT("Replacement retry survives its now-stale expected-old ID"), Owner, EOrderResult::AlreadyExists,
		[&] { return Owner.Replace(B, A.OrderId, 100); });
	OrderExpectUnchanged(*this, TEXT("Old terminal completion never clears the new link"), Owner, EOrderResult::TerminalConflict,
		[&] { return Owner.Complete(Initial.UnitId, A.OrderId, 13); });
	OrderExpectUnchanged(*this, TEXT("Old replacement reason cannot be overwritten by user cancellation"), Owner, EOrderResult::TerminalConflict,
		[&] { return Owner.Cancel(Initial.UnitId, A.OrderId, 13); });
	OrderExpectUnchanged(*this, TEXT("Old replacement cannot become a failure"), Owner, EOrderResult::TerminalConflict,
		[&] { return Owner.Fail(Initial.UnitId, A.OrderId, 13); });
	const FOrderRequest C = MakeMoveRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("A subsequent replacement still succeeds before the retry's future tick"), Owner.Replace(C, B.OrderId, 13), EOrderResult::Success);
	TestTrue(TEXT("Later replacement does not rewrite the first terminal record"), OrderSameRecord(ReadOrderValue(*this, Owner, A.OrderId), Old));
	OrderExpectUnchanged(*this, TEXT("A historical ID retry cannot become active again"), Owner, EOrderResult::AlreadyExists,
		[&] { return Owner.Submit(A, 200); });
	CheckOrderInvariant(*this, Owner);
	AddInfo(TEXT("Checked: replacement is atomic, old execution retains Replaced history, the new Pending order owns the link, invalid/stale replacements and old terminal callbacks cannot clear or overwrite it."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOrdersIdempotencyTest, "FogOfCommand.Orders.Idempotency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOrdersIdempotencyTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = MakeOrderTestUnit();
	FUnitOrders Owner;
	OrderCheckResult(*this, TEXT("Idempotency fixture"), Owner.Initialize(Initial), EOrderResult::Success);
	const FOrderRequest A = MakeMoveRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("Initial submission"), Owner.Submit(A, 10), EOrderResult::Success);
	for (int64 RetryAt : { int64(0), int64(100) })
	{
		OrderExpectUnchanged(*this, TEXT("Identical submission keeps first intent/time"), Owner, EOrderResult::AlreadyExists,
			[&] { return Owner.Submit(A, RetryAt); });
	}
	FOrderRequest Changed = A; Changed.TargetPosition = FVector(999.0, 0.0, 0.0);
	OrderExpectUnchanged(*this, TEXT("An existing ID cannot acquire another intent"), Owner, EOrderResult::IntentConflict,
		[&] { return Owner.Submit(Changed, 11); });
	OrderCheckResult(*this, TEXT("Retry did not advance the mutation clock"), Owner.Start(Initial.UnitId, A.OrderId, 11), EOrderResult::Success);
	for (int64 RetryAt : { int64(0), int64(100) })
	{
		OrderExpectUnchanged(*this, TEXT("Start retry does not rewrite StartedAt/UpdatedAt"), Owner, EOrderResult::AlreadyApplied,
			[&] { return Owner.Start(Initial.UnitId, A.OrderId, RetryAt); });
	}
	OrderCheckResult(*this, TEXT("Complete after ignored future retry"), Owner.Complete(Initial.UnitId, A.OrderId, 12), EOrderResult::Success);
	for (int64 RetryAt : { int64(0), int64(100) })
	{
		OrderExpectUnchanged(*this, TEXT("Completion retry preserves terminal timestamps"), Owner, EOrderResult::AlreadyApplied,
			[&] { return Owner.Complete(Initial.UnitId, A.OrderId, RetryAt); });
		OrderExpectUnchanged(*this, TEXT("Submission retry preserves completed history"), Owner, EOrderResult::AlreadyExists,
			[&] { return Owner.Submit(A, RetryAt); });
	}
	OrderExpectUnchanged(*this, TEXT("Changing a terminal ID's intent still fails"), Owner, EOrderResult::IntentConflict,
		[&] { return Owner.Submit(Changed, 13); });
	OrderExpectUnchanged(*this, TEXT("Owner initialization retry cannot reset history or clock"), Owner, EOrderResult::AlreadyExists,
		[&] { return Owner.Initialize(Initial); });
	const FOrderRequest B = MakeHoldRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("Submit after completed history"), Owner.Submit(B, 13), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("Cancel Pending"), Owner.Cancel(Initial.UnitId, B.OrderId, 14), EOrderResult::Success);
	OrderExpectUnchanged(*this, TEXT("Cancellation retry preserves first finish time"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.Cancel(Initial.UnitId, B.OrderId, 100); });
	const FOrderRequest C = MakeMoveRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("Next submit is not blocked by a no-op future cancellation"), Owner.Submit(C, 15), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("Fail Pending"), Owner.Fail(Initial.UnitId, C.OrderId, 16), EOrderResult::Success);
	OrderExpectUnchanged(*this, TEXT("Failure retry preserves first finish time"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.Fail(Initial.UnitId, C.OrderId, 0); });
	OrderExpectUnchanged(*this, TEXT("Failure cannot become cancellation"), Owner, EOrderResult::TerminalConflict,
		[&] { return Owner.Cancel(Initial.UnitId, C.OrderId, 17); });
	TestEqual(TEXT("Deduplication retains exactly the three original records"), Owner.GetHistory().Num(), int32(3));
	CheckOrderInvariant(*this, Owner);
	AddInfo(TEXT("Checked: stable IDs deduplicate identical Pending/Executing/terminal retries without advancing clocks, changed intent conflicts, first timestamps/outcomes survive and owner retries do not reset history."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOrdersDestructionTest, "FogOfCommand.Orders.Destruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOrdersDestructionTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	for (bool bExecuting : { false, true })
	{
		for (bool bDamage : { false, true })
		{
			const FUnitStateSnapshot Initial = MakeOrderTestUnit();
			FUnitOrders Owner;
			OrderCheckResult(*this, TEXT("Destruction fixture"), Owner.Initialize(Initial), EOrderResult::Success);
			const FOrderRequest Request = MakeMoveRequest(Initial.UnitId);
			OrderCheckResult(*this, TEXT("Submit before destruction"), Owner.Submit(Request, 10), EOrderResult::Success);
			if (bExecuting) { OrderCheckResult(*this, TEXT("Start before destruction"), Owner.Start(Initial.UnitId, Request.OrderId, 11), EOrderResult::Success); }
			for (double InvalidDamage : { -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() })
			{
				OrderExpectUnchanged(*this, TEXT("Invalid death damage cannot cancel the order or kill the unit"), Owner, EOrderResult::InvalidAmount,
					[&] { return Owner.ApplyDamage(Initial.UnitId, InvalidDamage, 12); });
			}
			OrderExpectUnchanged(*this, TEXT("Negative-time destruction is atomic"), Owner, EOrderResult::InvalidTime,
				[&] { return Owner.DestroyUnit(Initial.UnitId, -1); });
			OrderExpectUnchanged(*this, TEXT("Regressing lethal damage is atomic"), Owner, EOrderResult::TimeRegression,
				[&] { return Owner.ApplyDamage(Initial.UnitId, Initial.Health, 9); });
			const FUnitId OtherUnit = FUnitId::Create();
			OrderExpectUnchanged(*this, TEXT("Destroy cannot address another unit"), Owner, EOrderResult::UnknownUnit,
				[&] { return Owner.DestroyUnit(OtherUnit, 12); });
			OrderCheckResult(*this, TEXT("Coordinated explicit/lethal destruction"), bDamage ? Owner.ApplyDamage(Initial.UnitId, Initial.Health, 12)
				: Owner.DestroyUnit(Initial.UnitId, 12), EOrderResult::Success);
			const FOrderSnapshot Record = ReadOrderValue(*this, Owner, Request.OrderId);
			TestTrue(TEXT("Active order is cancelled for UnitDestroyed exactly at death"), Record.Status == EOrderStatus::Cancelled
				&& Record.EndReason == EOrderEndReason::UnitDestroyed && Record.UpdatedAt == 12
				&& Record.FinishedAt.IsSet() && Record.FinishedAt.GetValue() == 12);
			FUnitStateSnapshot Expected = Initial;
			Expected.Health = 0.0; Expected.Life = EUnitLifeState::Destroyed; Expected.CurrentOrderId.Reset();
			TestTrue(TEXT("Death and link clear commit together, preserving other unit fields"),
				Owner.GetUnit().IsDestroyed() && OrderSameUnit(Owner.GetUnit().GetSnapshot(), Expected));
			OrderExpectUnchanged(*this, TEXT("Repeated destruction cannot rewrite the first death/cancellation"), Owner, EOrderResult::AlreadyApplied,
				[&] { return Owner.DestroyUnit(Initial.UnitId, 100); });
			OrderExpectUnchanged(*this, TEXT("Damage cannot revive or rewrite a destroyed unit"), Owner, EOrderResult::UnitDestroyed,
				[&] { return Owner.ApplyDamage(Initial.UnitId, 0.0, 13); });
			const FOrderRequest Later = MakeHoldRequest(Initial.UnitId);
			OrderExpectUnchanged(*this, TEXT("Destroyed unit cannot accept a new order"), Owner, EOrderResult::UnitDestroyed,
				[&] { return Owner.Submit(Later, 13); });
			OrderExpectUnchanged(*this, TEXT("Owner initialization cannot resurrect or erase the cancelled history"), Owner, EOrderResult::AlreadyExists,
				[&] { return Owner.Initialize(Initial); });
			CheckOrderInvariant(*this, Owner);
		}
	}
	FUnitOrders Idle;
	const FUnitStateSnapshot IdleUnit = MakeOrderTestUnit();
	OrderCheckResult(*this, TEXT("Idle destruction fixture"), Idle.Initialize(IdleUnit), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("Destroy without active order"), Idle.DestroyUnit(IdleUnit.UnitId, 0), EOrderResult::Success);
	TestEqual(TEXT("Idle destruction creates no phantom history"), Idle.GetHistory().Num(), int32(0));
	CheckOrderInvariant(*this, Idle);
	AddInfo(TEXT("Checked: Pending/Executing cancellation and unit death commit together for explicit/lethal destruction, invalid death arguments preserve all state, repeated destruction does not rewrite history and dead units cannot receive new orders."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOrdersTimestampsTest, "FogOfCommand.Orders.Timestamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOrdersTimestampsTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = MakeOrderTestUnit();
	FUnitOrders Owner;
	OrderCheckResult(*this, TEXT("Time fixture"), Owner.Initialize(Initial), EOrderResult::Success);
	TestFalse(TEXT("Initialization alone does not invent simulation time"), Owner.GetLastMutationTime().IsSet());
	const FOrderRequest A = MakeHoldRequest(Initial.UnitId);
	for (int64 At : { int64(-1), std::numeric_limits<int64>::min() })
	{
		OrderExpectUnchanged(*this, TEXT("Negative simulation time"), Owner, EOrderResult::InvalidTime, [&] { return Owner.Submit(A, At); });
	}
	OrderCheckResult(*this, TEXT("Time zero is valid"), Owner.Submit(A, 0), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("Same-tick start is valid"), Owner.Start(Initial.UnitId, A.OrderId, 0), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("Same-tick completion is valid"), Owner.Complete(Initial.UnitId, A.OrderId, 0), EOrderResult::Success);
	const FOrderSnapshot Zero = ReadOrderValue(*this, Owner, A.OrderId);
	TestTrue(TEXT("Zero timestamps are present values, not unset sentinels"), Zero.CreatedAt == 0 && Zero.UpdatedAt == 0
		&& Zero.StartedAt.IsSet() && Zero.StartedAt.GetValue() == 0 && Zero.FinishedAt.IsSet() && Zero.FinishedAt.GetValue() == 0);
	const FOrderRequest B = MakeMoveRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("Advance to tick ten"), Owner.Submit(B, 10), EOrderResult::Success);
	OrderExpectUnchanged(*this, TEXT("Regressing start"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.Start(Initial.UnitId, B.OrderId, 9); });
	OrderExpectUnchanged(*this, TEXT("Regressing cancellation"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.Cancel(Initial.UnitId, B.OrderId, 9); });
	OrderExpectUnchanged(*this, TEXT("Regressing failure"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.Fail(Initial.UnitId, B.OrderId, 9); });
	OrderExpectUnchanged(*this, TEXT("Regressing unit damage"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.ApplyDamage(Initial.UnitId, 1.0, 9); });
	OrderCheckResult(*this, TEXT("Valid start advances clock"), Owner.Start(Initial.UnitId, B.OrderId, 11), EOrderResult::Success);
	OrderExpectUnchanged(*this, TEXT("Negative retry time is still invalid"), Owner, EOrderResult::InvalidTime,
		[&] { return Owner.Start(Initial.UnitId, B.OrderId, -1); });
	OrderExpectUnchanged(*this, TEXT("Negative submission retry time is invalid"), Owner, EOrderResult::InvalidTime,
		[&] { return Owner.Submit(B, -1); });
	OrderCheckResult(*this, TEXT("Complete at tick twelve"), Owner.Complete(Initial.UnitId, B.OrderId, 12), EOrderResult::Success);
	const int64 Maximum = std::numeric_limits<int64>::max();
	const FOrderRequest C = MakeHoldRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("INT64_MAX submission"), Owner.Submit(C, Maximum), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("INT64_MAX start without increment/overflow"), Owner.Start(Initial.UnitId, C.OrderId, Maximum), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("INT64_MAX completion without overflow"), Owner.Complete(Initial.UnitId, C.OrderId, Maximum), EOrderResult::Success);
	const FOrderSnapshot MaxRecord = ReadOrderValue(*this, Owner, C.OrderId);
	TestTrue(TEXT("Maximum timestamps are preserved exactly"), MaxRecord.CreatedAt == Maximum && MaxRecord.UpdatedAt == Maximum
		&& MaxRecord.StartedAt.IsSet() && MaxRecord.StartedAt.GetValue() == Maximum
		&& MaxRecord.FinishedAt.IsSet() && MaxRecord.FinishedAt.GetValue() == Maximum
		&& Owner.GetLastMutationTime().IsSet() && Owner.GetLastMutationTime().GetValue() == Maximum);
	const FOrderRequest D = MakeMoveRequest(Initial.UnitId);
	OrderExpectUnchanged(*this, TEXT("New mutations cannot regress from INT64_MAX"), Owner, EOrderResult::TimeRegression,
		[&] { return Owner.Submit(D, Maximum - 1); });
	OrderExpectUnchanged(*this, TEXT("Old nonnegative terminal retry is a no-op, not a clock regression"), Owner, EOrderResult::AlreadyApplied,
		[&] { return Owner.Complete(Initial.UnitId, C.OrderId, 0); });
	CheckOrderInvariant(*this, Owner);
	AddInfo(TEXT("Checked: simulation time is explicit/nonnegative/nondecreasing for mutations; zero/equal/INT64_MAX ticks are valid, regressions and negative retries reject atomically, and no-op retries preserve timestamps."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOrdersSerializationTest, "FogOfCommand.Orders.Serialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOrdersSerializationTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Initial = MakeOrderTestUnit();
	FUnitOrders Owner;
	OrderCheckResult(*this, TEXT("Legacy snapshot fixture"), Owner.Initialize(Initial), EOrderResult::Success);
	const FOrderRequest A = MakeMoveRequest(Initial.UnitId);
	const FOrderRequest B = MakeHoldRequest(Initial.UnitId);
	OrderCheckResult(*this, TEXT("Submit snapshot-linked order"), Owner.Submit(A, 1), EOrderResult::Success);
	OrderCheckResult(*this, TEXT("Start snapshot-linked order"), Owner.Start(Initial.UnitId, A.OrderId, 2), EOrderResult::Success);
	const FOrderTestObservation BeforeSerialization = ObserveOrders(Owner);
	TArray<uint8> Bytes;
	OrderCheckUnitResult(*this, TEXT("Existing UnitState v1 serializer still works"), Owner.GetUnit().Serialize(Bytes), EUnitStateResult::Success);
	TestEqual(TEXT("UnitState wire version remains v1"), Owner.GetUnit().GetSnapshot().Version, uint32(1));
	TestEqual(TEXT("UnitState wire remains 90 bytes"), Bytes.Num(), int32(90));
	FUnitState Loaded;
	OrderCheckUnitResult(*this, TEXT("Decode the linked UnitState value"), Loaded.Deserialize(Bytes), EUnitStateResult::Success);
	TestTrue(TEXT("Legacy round trip preserves unit identity/resources and the active opaque link"), OrderSameUnit(Loaded.GetSnapshot(), Owner.GetUnit().GetSnapshot())
		&& Loaded.GetSnapshot().CurrentOrderId.IsSet() && Loaded.GetSnapshot().CurrentOrderId.GetValue() == A.OrderId);
	TestTrue(TEXT("Serialization did not mutate authoritative owner/history"), OrdersMatchObservation(Owner, BeforeSerialization));
	FUnitOrders Orphan;
	OrderExpectUnchanged(*this, TEXT("A UnitState link is not enough to restore order history"), Orphan, EOrderResult::InvalidUnitState,
		[&] { return Orphan.Initialize(Loaded.GetSnapshot()); });
	OrderCheckResult(*this, TEXT("Replace after serialization"), Owner.Replace(B, A.OrderId, 3), EOrderResult::Success);
	const FOrderTestObservation BeforeDetachedMutation = ObserveOrders(Owner);
	OrderCheckUnitResult(*this, TEXT("Detached snapshot metadata can be cleared with an exact CAS"),
		Loaded.CompareExchangeCurrentOrderId(Initial.UnitId, TOptional<FOrderId>(A.OrderId), TOptional<FOrderId>()), EUnitStateResult::Success);
	TestTrue(TEXT("Mutating a detached value does not clear the owner's new current order"), OrdersMatchObservation(Owner, BeforeDetachedMutation));

	FUnitState Link;
	OrderCheckUnitResult(*this, TEXT("Standalone guarded-link fixture"), Link.Initialize(Initial), EUnitStateResult::Success);
	const TOptional<FOrderId> None;
	const TOptional<FOrderId> LinkA(A.OrderId);
	const TOptional<FOrderId> LinkB(B.OrderId);
	OrderCheckUnitResult(*this, TEXT("CAS assign A"), Link.CompareExchangeCurrentOrderId(Initial.UnitId, None, LinkA), EUnitStateResult::Success);
	OrderCheckUnitResult(*this, TEXT("CAS atomically replace A with B"), Link.CompareExchangeCurrentOrderId(Initial.UnitId, LinkA, LinkB), EUnitStateResult::Success);
	OrderExpectUnitUnchanged(*this, TEXT("Wrong expected-old must never clear a new link"), Link, EUnitStateResult::OrderConflict,
		[&] { return Link.CompareExchangeCurrentOrderId(Initial.UnitId, LinkA, None); });
	const FUnitId Foreign = FUnitId::Create();
	OrderExpectUnitUnchanged(*this, TEXT("CAS validates unit identity"), Link, EUnitStateResult::IdentityConflict,
		[&] { return Link.CompareExchangeCurrentOrderId(Foreign, LinkB, None); });
	OrderExpectUnitUnchanged(*this, TEXT("CAS rejects zero unit identity"), Link, EUnitStateResult::InvalidUnitId,
		[&] { return Link.CompareExchangeCurrentOrderId(FUnitId(), LinkB, None); });
	const TOptional<FOrderId> InvalidId{ FOrderId() };
	OrderExpectUnitUnchanged(*this, TEXT("CAS rejects invalid desired identity"), Link, EUnitStateResult::InvalidOrderId,
		[&] { return Link.CompareExchangeCurrentOrderId(Initial.UnitId, LinkB, InvalidId); });
	OrderExpectUnitUnchanged(*this, TEXT("CAS rejects invalid expected identity"), Link, EUnitStateResult::InvalidOrderId,
		[&] { return Link.CompareExchangeCurrentOrderId(Initial.UnitId, InvalidId, None); });
	OrderExpectUnitUnchanged(*this, TEXT("CAS same-value retry is a no-op"), Link, EUnitStateResult::Success,
		[&] { return Link.CompareExchangeCurrentOrderId(Initial.UnitId, LinkB, LinkB); });
	OrderCheckUnitResult(*this, TEXT("CAS clears exactly B"), Link.CompareExchangeCurrentOrderId(Initial.UnitId, LinkB, None), EUnitStateResult::Success);
	TestTrue(TEXT("CAS changes only opaque metadata"), OrderSameUnit(Link.GetSnapshot(), Initial));
	OrderCheckUnitResult(*this, TEXT("Destroy standalone link fixture"), Link.Destroy(), EUnitStateResult::Success);
	OrderExpectUnitUnchanged(*this, TEXT("CAS cannot add an order to a tombstone"), Link, EUnitStateResult::Destroyed,
		[&] { return Link.CompareExchangeCurrentOrderId(Initial.UnitId, None, LinkA); });

	TArray<uint8> ReplacementBytes;
	OrderCheckUnitResult(*this, TEXT("Serialize replaced link"), Owner.GetUnit().Serialize(ReplacementBytes), EUnitStateResult::Success);
	FUnitState ReplacementLoaded;
	OrderCheckUnitResult(*this, TEXT("Decode replaced link"), ReplacementLoaded.Deserialize(ReplacementBytes), EUnitStateResult::Success);
	TestTrue(TEXT("Replacement ID survives existing v1 encoding"), ReplacementLoaded.GetSnapshot().CurrentOrderId.IsSet()
		&& ReplacementLoaded.GetSnapshot().CurrentOrderId.GetValue() == B.OrderId);
	OrderCheckResult(*this, TEXT("Cancel before encoding cleared link"), Owner.Cancel(Initial.UnitId, B.OrderId, 4), EOrderResult::Success);
	TArray<uint8> ClearedBytes;
	OrderCheckUnitResult(*this, TEXT("Serialize cleared link"), Owner.GetUnit().Serialize(ClearedBytes), EUnitStateResult::Success);
	FUnitState ClearedLoaded;
	OrderCheckUnitResult(*this, TEXT("Decode cleared link"), ClearedLoaded.Deserialize(ClearedBytes), EUnitStateResult::Success);
	TestFalse(TEXT("Cleared link remains absent after v1 round trip"), ClearedLoaded.GetSnapshot().CurrentOrderId.IsSet());
	CheckOrderInvariant(*this, Owner);
	AddInfo(TEXT("Checked: existing UnitState v1 bytes round-trip active/replaced/cleared OrderIds and resources; guarded CAS rejects stale/invalid identities, detached data cannot mutate the owner, and orphan links cannot pretend to restore order history."));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
