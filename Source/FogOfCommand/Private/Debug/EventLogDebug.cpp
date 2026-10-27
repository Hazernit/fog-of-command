#include "Debug/EventLogDebug.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Domain/Orders.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Templates/UniquePtr.h"

namespace FogOfCommand::Debug
{
namespace
{
using namespace FogOfCommand::Domain;

// This session is deliberately unrelated to any world, Actor or player unit.
TUniquePtr<FUnitOrders> Session;
TUniquePtr<FAutoConsoleCommand> ScenarioCommand;
TUniquePtr<FAutoConsoleCommand> DumpCommand;

const TCHAR* EventName(ESimulationEventType Type)
{
	switch (Type)
	{
	case ESimulationEventType::UnitCreated: return TEXT("UnitCreated");
	case ESimulationEventType::UnitMoved: return TEXT("UnitMoved");
	case ESimulationEventType::UnitDamaged: return TEXT("UnitDamaged");
	case ESimulationEventType::UnitDestroyed: return TEXT("UnitDestroyed");
	case ESimulationEventType::AmmunitionConsumed: return TEXT("AmmunitionConsumed");
	case ESimulationEventType::OrderSubmitted: return TEXT("OrderSubmitted");
	case ESimulationEventType::OrderStarted: return TEXT("OrderStarted");
	case ESimulationEventType::OrderCompleted: return TEXT("OrderCompleted");
	case ESimulationEventType::OrderCancelled: return TEXT("OrderCancelled");
	case ESimulationEventType::OrderFailed: return TEXT("OrderFailed");
	case ESimulationEventType::OrderReplaced: return TEXT("OrderReplaced");
	default: return TEXT("Unknown");
	}
}

const TCHAR* ReasonName(ESimulationEventReason Reason)
{
	switch (Reason)
	{
	case ESimulationEventReason::None: return TEXT("None");
	case ESimulationEventReason::Created: return TEXT("Created");
	case ESimulationEventReason::PositionChanged: return TEXT("PositionChanged");
	case ESimulationEventReason::DamageApplied: return TEXT("DamageApplied");
	case ESimulationEventReason::AmmunitionSpent: return TEXT("AmmunitionSpent");
	case ESimulationEventReason::ExplicitDestruction: return TEXT("ExplicitDestruction");
	case ESimulationEventReason::Completed: return TEXT("Completed");
	case ESimulationEventReason::CancelledByRequest: return TEXT("CancelledByRequest");
	case ESimulationEventReason::Replaced: return TEXT("Replaced");
	case ESimulationEventReason::UnitDestroyed: return TEXT("UnitDestroyed");
	case ESimulationEventReason::ExecutionFailed: return TEXT("ExecutionFailed");
	default: return TEXT("Unknown");
	}
}

const TCHAR* StatusName(const TOptional<EOrderStatus>& Status)
{
	if (!Status.IsSet()) { return TEXT("None"); }
	switch (Status.GetValue())
	{
	case EOrderStatus::Pending: return TEXT("Pending");
	case EOrderStatus::Executing: return TEXT("Executing");
	case EOrderStatus::Completed: return TEXT("Completed");
	case EOrderStatus::Cancelled: return TEXT("Cancelled");
	case EOrderStatus::Failed: return TEXT("Failed");
	default: return TEXT("Unknown");
	}
}

FString OrderText(const TOptional<FOrderId>& Id)
{
	return Id.IsSet() ? Id.GetValue().GetValue().ToString() : TEXT("None");
}

void RunScenario(const TArray<FString>& Args, FOutputDevice& Output)
{
	if (!Args.IsEmpty())
	{
		Output.Log(TEXT("FOC_EVENT_DEBUG REJECTED: this command takes no arguments."));
		return;
	}
	auto Candidate = MakeUnique<FUnitOrders>();
	FUnitStateSnapshot Initial;
	Initial.UnitId = FUnitId::Create();
	Initial.SideId = FSideId::FromValue(1);
	Initial.Ammunition = 10;
	Initial.AmmoCapacity = 10;
	const FUnitId UnitId = Initial.UnitId;
	auto Accepted = [&Output](EOrderResult Result)
	{
		if (Result == EOrderResult::Success) { return true; }
		Output.Log(TEXT("FOC_EVENT_DEBUG FAIL: validated scenario operation rejected."));
		return false;
	};
	auto Request = [UnitId](EOrderKind Kind)
	{
		FOrderRequest Order;
		Order.UnitId = UnitId;
		Order.OrderId = FOrderId::Create();
		Order.Kind = Kind;
		if (Kind == EOrderKind::Move) { Order.TargetPosition = FVector(500.0, 0.0, 0.0); }
		return Order;
	};
	if (!Accepted(Candidate->Initialize(Initial, 0))
		|| !Accepted(Candidate->MoveUnit(UnitId, FVector(100.0, 0.0, 0.0), 1))
		|| !Accepted(Candidate->ConsumeAmmunition(UnitId, 2, 2))
		|| !Accepted(Candidate->ApplyDamage(UnitId, 10.0, 3))) { return; }
	const FOrderRequest First = Request(EOrderKind::Move);
	if (!Accepted(Candidate->Submit(First, 4))
		|| !Accepted(Candidate->Start(UnitId, First.OrderId, 5))
		|| !Accepted(Candidate->Complete(UnitId, First.OrderId, 6))) { return; }
	const FOrderRequest Old = Request(EOrderKind::Hold);
	const FOrderRequest Replacement = Request(EOrderKind::Move);
	if (!Accepted(Candidate->Submit(Old, 7))
		|| !Accepted(Candidate->Start(UnitId, Old.OrderId, 8))
		|| !Accepted(Candidate->Replace(Replacement, Old.OrderId, 9))
		|| !Accepted(Candidate->Start(UnitId, Replacement.OrderId, 10))
		|| !Accepted(Candidate->Fail(UnitId, Replacement.OrderId, 11))) { return; }
	const FOrderRequest Cancelled = Request(EOrderKind::Hold);
	if (!Accepted(Candidate->Submit(Cancelled, 12))
		|| !Accepted(Candidate->Cancel(UnitId, Cancelled.OrderId, 13))) { return; }
	const FOrderRequest Final = Request(EOrderKind::Hold);
	if (!Accepted(Candidate->Submit(Final, 14))
		|| !Accepted(Candidate->Start(UnitId, Final.OrderId, 15))
		|| !Accepted(Candidate->ApplyDamage(UnitId, 1000.0, 16))) { return; }
	const uint64 Count = Candidate->GetEventLog().GetTotalRecorded();
	if (Count != 21 || Candidate->Validate() != EOrderResult::Success
		|| Candidate->DestroyUnit(UnitId, 17) != EOrderResult::AlreadyApplied
		|| Candidate->GetEventLog().GetTotalRecorded() != Count)
	{
		Output.Log(TEXT("FOC_EVENT_DEBUG FAIL: final event/state invariants."));
		return;
	}
	Session = MoveTemp(Candidate);
	Output.Log(TEXT("FOC_EVENT_DEBUG PASS scenario: 21 events, 11 event kinds, isolated unit destroyed, repeat emitted 0 events."));
}

void Dump(const TArray<FString>& Args, FOutputDevice& Output)
{
	if (!Args.IsEmpty())
	{
		Output.Log(TEXT("FOC_EVENT_DEBUG REJECTED: this command takes no arguments."));
		return;
	}
	if (!Session)
	{
		Output.Log(TEXT("FOC_EVENT_DEBUG EMPTY: run foc.EventLog.RunScenario first."));
		return;
	}
	const FSimulationEventLog& Log = Session->GetEventLog();
	Output.Logf(TEXT("FOC_EVENT_DEBUG DUMP retained=%d total=%llu evicted=%llu"),
		Log.Num(), static_cast<unsigned long long>(Log.GetTotalRecorded()),
		static_cast<unsigned long long>(Log.GetEvictedCount()));
	for (const FSimulationEvent& Event : Log.GetEvents())
	{
		Output.Logf(TEXT("FOC_EVENT seq=%llu tick=%lld type=%s unit=%s order=%s related=%s reason=%s status=%s->%s"),
			static_cast<unsigned long long>(Event.Sequence), static_cast<long long>(Event.SimulationTick),
			EventName(Event.Type), *Event.UnitId.GetValue().ToString(), *OrderText(Event.OrderId),
			*OrderText(Event.RelatedOrderId), ReasonName(Event.Reason),
			StatusName(Event.PreviousOrderStatus), StatusName(Event.OrderStatus));
		if (Event.UnitAfter.IsSet())
		{
			const FUnitStateSnapshot& After = Event.UnitAfter.GetValue();
			Output.Logf(TEXT("FOC_EVENT_AFTER health=%.17g ammo=%d position=(%.17g,%.17g,%.17g) life=%s current=%s"),
				After.Health, After.Ammunition, After.Position.X, After.Position.Y, After.Position.Z,
				After.Life == EUnitLifeState::Alive ? TEXT("Alive") : TEXT("Destroyed"), *OrderText(After.CurrentOrderId));
		}
	}
}
}

void RegisterEventLogDebugCommands()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("FocEventDebug"))) { return; }
	ScenarioCommand = MakeUnique<FAutoConsoleCommand>(TEXT("foc.EventLog.RunScenario"),
		TEXT("Run a bounded validated scenario on an isolated developer unit; no arguments."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(&RunScenario), ECVF_Cheat);
	DumpCommand = MakeUnique<FAutoConsoleCommand>(TEXT("foc.EventLog.Dump"),
		TEXT("Dump at most 256 retained isolated domain events; no arguments."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(&Dump), ECVF_Cheat);
}

void UnregisterEventLogDebugCommands()
{
	DumpCommand.Reset();
	ScenarioCommand.Reset();
	Session.Reset();
}
}

#endif
