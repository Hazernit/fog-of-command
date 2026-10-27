#include "Domain/SimulationEvents.h"

namespace FogOfCommand::Domain
{
bool FSimulationEvent::operator==(const FSimulationEvent& Other) const
{
	return Sequence == Other.Sequence && SimulationTick == Other.SimulationTick
		&& Type == Other.Type && UnitId == Other.UnitId && OrderId == Other.OrderId
		&& RelatedOrderId == Other.RelatedOrderId && Reason == Other.Reason
		&& UnitBefore == Other.UnitBefore && UnitAfter == Other.UnitAfter
		&& PreviousOrderStatus == Other.PreviousOrderStatus && OrderStatus == Other.OrderStatus
		&& OrderKind == Other.OrderKind && TargetPosition == Other.TargetPosition;
}

bool FSimulationEventLog::CanAppend(int32 Count) const
{
	return Count > 0 && uint64(Count) <= MAX_uint64 - TotalRecorded;
}

void FSimulationEventLog::Append(FSimulationEvent&& Event)
{
	Event.Sequence = ++TotalRecorded;
	LastSimulationTick = Event.SimulationTick;
	if (Entries.Num() < Capacity)
	{
		Entries.Add(MoveTemp(Event));
	}
	else
	{
		Entries[NextIndex] = MoveTemp(Event);
	}
	NextIndex = (NextIndex + 1) % Capacity;
}

TArray<FSimulationEvent> FSimulationEventLog::GetEvents() const
{
	TArray<FSimulationEvent> Ordered;
	Ordered.Reserve(Entries.Num());
	const int32 First = Entries.Num() == Capacity ? NextIndex : 0;
	for (int32 Offset = 0; Offset < Entries.Num(); ++Offset)
	{
		Ordered.Add(Entries[(First + Offset) % Entries.Num()]);
	}
	return Ordered;
}
}
