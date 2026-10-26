#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Containers/Set.h"
#include "Domain/UnitIdentifiers.h"
#include "Domain/UnitState.h"

#include <limits>
#include <type_traits>

namespace
{
using namespace FogOfCommand::Domain;

static_assert(!std::is_same_v<FUnitId, FOrderId>);
static_assert(!std::is_same_v<FUnitId, FSideId>);
static_assert(!std::is_convertible_v<FUnitId, FOrderId>);
static_assert(!std::is_convertible_v<FOrderId, FUnitId>);
static_assert(!std::is_copy_assignable_v<FUnitState>);
static_assert(!std::is_move_assignable_v<FUnitState>);

FUnitStateSnapshot MakeInitialState()
{
	FUnitStateSnapshot Initial;
	Initial.UnitId = FUnitId::Create();
	Initial.SideId = FSideId::FromValue(2);
	Initial.Position = FVector(125.0, -250.0, 5.0);
	Initial.Health = 73.5;
	Initial.MaxHealth = 100.0;
	Initial.Ammunition = 17;
	Initial.AmmoCapacity = 40;
	Initial.CurrentOrderId = FOrderId::Create();
	return Initial;
}

bool SameFields(const FUnitStateSnapshot& Actual, const FUnitStateSnapshot& Expected)
{
	// Compare every public field independently of the domain equality operator.
	const bool bSameOrder = Actual.CurrentOrderId.IsSet() == Expected.CurrentOrderId.IsSet()
		&& (!Actual.CurrentOrderId.IsSet() || Actual.CurrentOrderId.GetValue() == Expected.CurrentOrderId.GetValue());
	return Actual.Version == Expected.Version && Actual.UnitId == Expected.UnitId && Actual.SideId == Expected.SideId
		&& Actual.Position.X == Expected.Position.X && Actual.Position.Y == Expected.Position.Y
		&& Actual.Position.Z == Expected.Position.Z && Actual.Health == Expected.Health
		&& Actual.MaxHealth == Expected.MaxHealth && Actual.Ammunition == Expected.Ammunition
		&& Actual.AmmoCapacity == Expected.AmmoCapacity && Actual.Life == Expected.Life && bSameOrder;
}

bool CheckResult(FAutomationTestBase& Test, const TCHAR* Description,
	EUnitStateResult Actual, EUnitStateResult Expected)
{
	return Test.TestEqual(Description, static_cast<int32>(Actual), static_cast<int32>(Expected));
}

template <typename OperationType>
void ExpectUnchanged(FAutomationTestBase& Test, const TCHAR* Description, FUnitState& State,
	EUnitStateResult ExpectedResult, OperationType Operation)
{
	const FUnitStateSnapshot Before = State.GetSnapshot();
	const bool bInitialized = State.IsInitialized();
	const bool bAlive = State.IsAlive();
	const bool bDestroyed = State.IsDestroyed();
	CheckResult(Test, Description, Operation(), ExpectedResult);
	Test.TestTrue(*(FString(Description) + TEXT(" leaves the entire state unchanged")),
		SameFields(State.GetSnapshot(), Before) && State.IsInitialized() == bInitialized
		&& State.IsAlive() == bAlive && State.IsDestroyed() == bDestroyed);
}

void CheckRoundTripFields(FAutomationTestBase& Test,
	const FUnitStateSnapshot& Actual, const FUnitStateSnapshot& Expected)
{
	Test.TestEqual(TEXT("Round-trip schema version"), Actual.Version, Expected.Version);
	Test.TestTrue(TEXT("Round-trip UnitId"), Actual.UnitId == Expected.UnitId);
	Test.TestTrue(TEXT("Round-trip SideId"), Actual.SideId == Expected.SideId);
	Test.TestTrue(TEXT("Round-trip exact position"), Actual.Position.X == Expected.Position.X
		&& Actual.Position.Y == Expected.Position.Y && Actual.Position.Z == Expected.Position.Z);
	Test.TestEqual(TEXT("Round-trip health"), Actual.Health, Expected.Health);
	Test.TestEqual(TEXT("Round-trip maximum health"), Actual.MaxHealth, Expected.MaxHealth);
	Test.TestEqual(TEXT("Round-trip ammunition"), Actual.Ammunition, Expected.Ammunition);
	Test.TestEqual(TEXT("Round-trip ammunition capacity"), Actual.AmmoCapacity, Expected.AmmoCapacity);
	Test.TestEqual(TEXT("Round-trip life state"), static_cast<int32>(Actual.Life), static_cast<int32>(Expected.Life));
	Test.TestTrue(TEXT("Round-trip optional order presence/value"),
		Actual.CurrentOrderId.IsSet() == Expected.CurrentOrderId.IsSet()
		&& (!Expected.CurrentOrderId.IsSet() || Actual.CurrentOrderId.GetValue() == Expected.CurrentOrderId.GetValue()));
}

void WriteWireUint32(TArray<uint8>& Bytes, int32 Offset, uint32 Value)
{
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Bytes[Offset + Index] = static_cast<uint8>(Value >> (8 * Index));
	}
}

void WriteWireUint64(TArray<uint8>& Bytes, int32 Offset, uint64 Value)
{
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Bytes[Offset + Index] = static_cast<uint8>(Value >> (8 * Index));
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogUnitIdentifiersTest,
	"FogOfCommand.UnitState.Identifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogUnitIdentifiersTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	TestFalse(TEXT("Default UnitId is invalid"), FUnitId().IsValid());
	TestFalse(TEXT("Default OrderId is invalid"), FOrderId().IsValid());
	TestFalse(TEXT("Default SideId is invalid"), FSideId().IsValid());
	TestFalse(TEXT("Zero imported UnitId is invalid"), FUnitId::FromGuid(FGuid()).IsValid());
	TestFalse(TEXT("Zero imported OrderId is invalid"), FOrderId::FromGuid(FGuid()).IsValid());
	TestFalse(TEXT("Negative side is invalid"), FSideId::FromValue(-1).IsValid());
	TestFalse(TEXT("Minimum integer side is invalid"), FSideId::FromValue(std::numeric_limits<int32>::min()).IsValid());
	TestTrue(TEXT("First positive side is valid"), FSideId::FromValue(1).IsValid());
	TestTrue(TEXT("Maximum positive side is valid"), FSideId::FromValue(std::numeric_limits<int32>::max()).IsValid());
	TestEqual(TEXT("Side import preserves value"), FSideId::FromValue(7).GetValue(), int32(7));

	TSet<FUnitId> Units;
	TSet<FOrderId> Orders;
	for (int32 Index = 0; Index < 512; ++Index)
	{
		const FUnitId Unit = FUnitId::Create();
		const FOrderId Order = FOrderId::Create();
		TestTrue(TEXT("Generated UnitId is valid"), Unit.IsValid());
		TestTrue(TEXT("Generated OrderId is valid"), Order.IsValid());
		TestFalse(TEXT("Generated UnitId is unique in this sample"), Units.Contains(Unit));
		TestFalse(TEXT("Generated OrderId is unique in this sample"), Orders.Contains(Order));
		Units.Add(Unit);
		Orders.Add(Order);
		TestTrue(TEXT("Explicit GUID import preserves UnitId"), FUnitId::FromGuid(Unit.GetValue()) == Unit);
		TestTrue(TEXT("Explicit GUID import preserves OrderId"), FOrderId::FromGuid(Order.GetValue()) == Order);
	}
	TestEqual(TEXT("All generated UnitIds retained"), Units.Num(), int32(512));
	TestEqual(TEXT("All generated OrderIds retained"), Orders.Num(), int32(512));
	AddInfo(TEXT("Checked: 512 UnitIds and OrderIds for validity/uniqueness, GUID import preservation, side bounds and distinct identity types."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogUnitLifecycleTest,
	"FogOfCommand.UnitState.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogUnitLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	FUnitState Unit;
	TestFalse(TEXT("An empty slot is not initialized"), Unit.IsInitialized());
	TestFalse(TEXT("An empty slot is not alive"), Unit.IsAlive());
	TestFalse(TEXT("An empty slot is not destroyed"), Unit.IsDestroyed());
	ExpectUnchanged(*this, TEXT("Move before initialization"), Unit, EUnitStateResult::NotInitialized,
		[&] { return Unit.MoveTo(FVector(1.0, 2.0, 3.0)); });
	ExpectUnchanged(*this, TEXT("Damage before initialization"), Unit, EUnitStateResult::NotInitialized,
		[&] { return Unit.ApplyDamage(1.0); });
	ExpectUnchanged(*this, TEXT("Consumption before initialization"), Unit, EUnitStateResult::NotInitialized,
		[&] { return Unit.ConsumeAmmunition(0); });
	ExpectUnchanged(*this, TEXT("Destroy before initialization"), Unit, EUnitStateResult::NotInitialized,
		[&] { return Unit.Destroy(); });
	const FUnitStateSnapshot Initial = MakeInitialState();
	if (!CheckResult(*this, TEXT("Initialize"), Unit.Initialize(Initial), EUnitStateResult::Success)) { return false; }
	TestTrue(TEXT("Initialized unit is alive"), Unit.IsInitialized() && Unit.IsAlive() && !Unit.IsDestroyed());
	TArray<uint8> CreationBytes;
	CheckResult(*this, TEXT("Capture original creation data"), Unit.Serialize(CreationBytes), EUnitStateResult::Success);
	const FVector Moved(900.0, -450.0, 20.0);
	CheckResult(*this, TEXT("Move"), Unit.MoveTo(Moved), EUnitStateResult::Success);
	TestEqual(TEXT("Position changed"), Unit.GetSnapshot().Position, Moved);
	CheckResult(*this, TEXT("Nonlethal damage"), Unit.ApplyDamage(13.5), EUnitStateResult::Success);
	TestEqual(TEXT("Damage reduces health"), Unit.GetSnapshot().Health, 60.0);
	CheckResult(*this, TEXT("Ammunition consumption"), Unit.ConsumeAmmunition(3), EUnitStateResult::Success);
	TestEqual(TEXT("Only spent ammunition is removed"), Unit.GetSnapshot().Ammunition, int32(14));
	TestTrue(TEXT("ID and side survive movement/damage/consumption"), Unit.GetSnapshot().UnitId == Initial.UnitId
		&& Unit.GetSnapshot().SideId == Initial.SideId);
	TestTrue(TEXT("Nonlethal operations preserve the opaque order"),
		Unit.GetSnapshot().CurrentOrderId.IsSet() && Unit.GetSnapshot().CurrentOrderId.GetValue() == Initial.CurrentOrderId.GetValue());
	const FUnitStateSnapshot BeforeDeath = Unit.GetSnapshot();
	CheckResult(*this, TEXT("Explicit destroy"), Unit.Destroy(), EUnitStateResult::Success);
	TestTrue(TEXT("Death is terminal state"), Unit.IsDestroyed() && !Unit.IsAlive());
	TestEqual(TEXT("Destroyed health is exactly zero"), Unit.GetSnapshot().Health, 0.0);
	TestFalse(TEXT("Death clears the order reference"), Unit.GetSnapshot().CurrentOrderId.IsSet());
	TestTrue(TEXT("Death preserves identity/side/location/resources"), Unit.GetSnapshot().UnitId == BeforeDeath.UnitId
		&& Unit.GetSnapshot().SideId == BeforeDeath.SideId && Unit.GetSnapshot().Position == BeforeDeath.Position
		&& Unit.GetSnapshot().Ammunition == BeforeDeath.Ammunition && Unit.GetSnapshot().AmmoCapacity == BeforeDeath.AmmoCapacity
		&& Unit.GetSnapshot().MaxHealth == BeforeDeath.MaxHealth);
	ExpectUnchanged(*this, TEXT("Repeated destroy"), Unit, EUnitStateResult::AlreadyDestroyed, [&] { return Unit.Destroy(); });
	ExpectUnchanged(*this, TEXT("Move after death"), Unit, EUnitStateResult::Destroyed, [&] { return Unit.MoveTo(FVector::ZeroVector); });
	ExpectUnchanged(*this, TEXT("Damage after death"), Unit, EUnitStateResult::Destroyed, [&] { return Unit.ApplyDamage(0.0); });
	ExpectUnchanged(*this, TEXT("Consumption after death"), Unit, EUnitStateResult::Destroyed, [&] { return Unit.ConsumeAmmunition(0); });
	ExpectUnchanged(*this, TEXT("Repeated create cannot resurrect"), Unit, EUnitStateResult::AlreadyExists, [&] { return Unit.Initialize(Initial); });
	ExpectUnchanged(*this, TEXT("Creation snapshot load cannot resurrect"), Unit, EUnitStateResult::AlreadyExists, [&] { return Unit.Deserialize(CreationBytes); });
	const FUnitStateSnapshot OtherIdentity = MakeInitialState();
	ExpectUnchanged(*this, TEXT("A different ID cannot replace a tombstone"), Unit, EUnitStateResult::IdentityConflict,
		[&] { return Unit.Initialize(OtherIdentity); });
	AddInfo(TEXT("Checked: move/damage/consumption preserve identity; death retains spent resources and is terminal across repeated creation, destruction and snapshot replay."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogUnitResourcesTest,
	"FogOfCommand.UnitState.Resources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogUnitResourcesTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	FUnitStateSnapshot Initial = MakeInitialState();
	Initial.Ammunition = 0;
	Initial.AmmoCapacity = 0;
	FUnitState EmptyStock;
	CheckResult(*this, TEXT("Zero-stock/zero-capacity is valid"), EmptyStock.Initialize(Initial), EUnitStateResult::Success);
	ExpectUnchanged(*this, TEXT("Consume zero"), EmptyStock, EUnitStateResult::Success, [&] { return EmptyStock.ConsumeAmmunition(0); });
	ExpectUnchanged(*this, TEXT("Empty stock cannot be consumed"), EmptyStock, EUnitStateResult::InsufficientAmmunition,
		[&] { return EmptyStock.ConsumeAmmunition(1); });
	ExpectUnchanged(*this, TEXT("Negative consumption cannot refill"), EmptyStock, EUnitStateResult::InvalidAmount,
		[&] { return EmptyStock.ConsumeAmmunition(-1); });
	ExpectUnchanged(*this, TEXT("Minimum integer consumption cannot wrap/refill"), EmptyStock, EUnitStateResult::InvalidAmount,
		[&] { return EmptyStock.ConsumeAmmunition(std::numeric_limits<int32>::min()); });
	ExpectUnchanged(*this, TEXT("Maximum consumption cannot underflow zero stock"), EmptyStock, EUnitStateResult::InsufficientAmmunition,
		[&] { return EmptyStock.ConsumeAmmunition(std::numeric_limits<int32>::max()); });
	ExpectUnchanged(*this, TEXT("Zero damage"), EmptyStock, EUnitStateResult::Success, [&] { return EmptyStock.ApplyDamage(0.0); });
	for (double InvalidDamage : { -1.0, -std::numeric_limits<double>::max(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() })
	{
		ExpectUnchanged(*this, TEXT("Invalid damage is rejected atomically"), EmptyStock, EUnitStateResult::InvalidAmount,
			[&] { return EmptyStock.ApplyDamage(InvalidDamage); });
	}
	Initial = MakeInitialState();
	Initial.Ammunition = std::numeric_limits<int32>::max();
	Initial.AmmoCapacity = std::numeric_limits<int32>::max();
	FUnitState FullStock;
	CheckResult(*this, TEXT("Maximum integer stock/capacity"), FullStock.Initialize(Initial), EUnitStateResult::Success);
	CheckResult(*this, TEXT("Consume exactly maximum stock"), FullStock.ConsumeAmmunition(std::numeric_limits<int32>::max()), EUnitStateResult::Success);
	TestEqual(TEXT("Exact exhaustion reaches zero without wrapping"), FullStock.GetSnapshot().Ammunition, int32(0));
	TestEqual(TEXT("Consumption preserves capacity"), FullStock.GetSnapshot().AmmoCapacity, std::numeric_limits<int32>::max());
	ExpectUnchanged(*this, TEXT("Repeated exhaustion cannot go negative"), FullStock, EUnitStateResult::InsufficientAmmunition,
		[&] { return FullStock.ConsumeAmmunition(1); });
	CheckResult(*this, TEXT("Finite overkill destroys without negative/infinite health"), FullStock.ApplyDamage(std::numeric_limits<double>::max()), EUnitStateResult::Success);
	TestTrue(TEXT("Overkill leaves a valid zero-health tombstone"), FullStock.IsDestroyed() && FullStock.GetSnapshot().Health == 0.0
		&& FUnitState::ValidateSnapshot(FullStock.GetSnapshot()) == EUnitStateResult::Success);
	Initial = MakeInitialState();
	Initial.Health = Initial.MaxHealth;
	FUnitState ExactLethal;
	CheckResult(*this, TEXT("Maximum-health boundary"), ExactLethal.Initialize(Initial), EUnitStateResult::Success);
	CheckResult(*this, TEXT("Exact lethal damage"), ExactLethal.ApplyDamage(Initial.Health), EUnitStateResult::Success);
	TestTrue(TEXT("Exact lethal damage destroys and clears order"), ExactLethal.IsDestroyed() && !ExactLethal.GetSnapshot().CurrentOrderId.IsSet());
	Initial = MakeInitialState();
	Initial.Health = std::numeric_limits<double>::max();
	Initial.MaxHealth = std::numeric_limits<double>::max();
	FUnitState MaximumFiniteHealth;
	CheckResult(*this, TEXT("Maximum finite health/capacity is representable"), MaximumFiniteHealth.Initialize(Initial), EUnitStateResult::Success);
	CheckResult(*this, TEXT("Exact maximum finite damage does not overflow"), MaximumFiniteHealth.ApplyDamage(Initial.Health), EUnitStateResult::Success);
	TestTrue(TEXT("Maximum finite health becomes exactly zero on death"), MaximumFiniteHealth.IsDestroyed() && MaximumFiniteHealth.GetSnapshot().Health == 0.0);
	AddInfo(TEXT("Checked: ammunition 0/INT32_MAX boundaries, exact depletion, insufficient/negative/INT32_MIN rejection, NaN/Inf damage rejection and finite overkill without negative health or overflow."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogUnitValidationTest,
	"FogOfCommand.UnitState.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogUnitValidationTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	const FUnitStateSnapshot Good = MakeInitialState();
	FUnitState Existing;
	if (!CheckResult(*this, TEXT("Validation fixture"), Existing.Initialize(Good), EUnitStateResult::Success)) { return false; }
	auto Reject = [&](const TCHAR* Description, const FUnitStateSnapshot& Invalid, EUnitStateResult Expected)
	{
		CheckResult(*this, Description, FUnitState::ValidateSnapshot(Invalid), Expected);
		FUnitState Fresh;
		ExpectUnchanged(*this, Description, Fresh, Expected, [&] { return Fresh.Initialize(Invalid); });
		ExpectUnchanged(*this, Description, Existing, Expected, [&] { return Existing.Initialize(Invalid); });
	};
	FUnitStateSnapshot Invalid = Good;
	Invalid.Version = FUnitStateSnapshot::CurrentVersion + 1;
	Reject(TEXT("Unknown snapshot version"), Invalid, EUnitStateResult::UnsupportedVersion);
	Invalid = Good; Invalid.UnitId = FUnitId();
	Reject(TEXT("Invalid UnitId"), Invalid, EUnitStateResult::InvalidUnitId);
	for (int32 Side : { 0, -1, std::numeric_limits<int32>::min() })
	{
		Invalid = Good; Invalid.SideId = FSideId::FromValue(Side);
		Reject(TEXT("Invalid side"), Invalid, EUnitStateResult::InvalidSideId);
	}
	for (double NonFinite : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() })
	{
		// Assign after construction to test validation, without triggering FVector's optional constructor diagnostics.
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Invalid = Good; Invalid.Position[Axis] = NonFinite;
			Reject(TEXT("Nonfinite position coordinate"), Invalid, EUnitStateResult::InvalidPosition);
			ExpectUnchanged(*this, TEXT("Nonfinite movement"), Existing, EUnitStateResult::InvalidPosition,
				[&] { return Existing.MoveTo(Invalid.Position); });
		}
		Invalid = Good; Invalid.Health = NonFinite;
		Reject(TEXT("Nonfinite health"), Invalid, EUnitStateResult::InvalidHealth);
		Invalid = Good; Invalid.MaxHealth = NonFinite;
		Reject(TEXT("Nonfinite maximum health"), Invalid, EUnitStateResult::InvalidHealth);
	}
	for (double Health : { -1.0, 100.01 })
	{
		Invalid = Good; Invalid.Health = Health;
		Reject(TEXT("Health outside bounds"), Invalid, EUnitStateResult::InvalidHealth);
	}
	for (double Maximum : { 0.0, -1.0 })
	{
		Invalid = Good; Invalid.MaxHealth = Maximum;
		Reject(TEXT("Nonpositive maximum health"), Invalid, EUnitStateResult::InvalidHealth);
	}
	Invalid = Good; Invalid.Ammunition = -1;
	Reject(TEXT("Negative stock"), Invalid, EUnitStateResult::InvalidAmmunition);
	Invalid = Good; Invalid.AmmoCapacity = -1;
	Reject(TEXT("Negative capacity"), Invalid, EUnitStateResult::InvalidAmmunition);
	Invalid = Good; Invalid.Ammunition = Invalid.AmmoCapacity + 1;
	Reject(TEXT("Stock above capacity"), Invalid, EUnitStateResult::InvalidAmmunition);
	Invalid = Good; Invalid.Life = static_cast<EUnitLifeState>(255);
	Reject(TEXT("Unknown life state"), Invalid, EUnitStateResult::InvalidLifeState);
	Invalid = Good; Invalid.CurrentOrderId = FOrderId();
	Reject(TEXT("Present but invalid OrderId"), Invalid, EUnitStateResult::InvalidOrderId);
	for (int32 Case = 0; Case < 3; ++Case)
	{
		Invalid = Good;
		if (Case == 0) { Invalid.Health = 0.0; }
		else { Invalid.Life = EUnitLifeState::Destroyed; if (Case == 2) { Invalid.Health = 0.0; } }
		const EUnitStateResult Rejection = FUnitState::ValidateSnapshot(Invalid);
		TestTrue(TEXT("Incoherent life/health/order combination is rejected"), Rejection != EUnitStateResult::Success);
		Reject(TEXT("Incoherent lifecycle import remains atomic"), Invalid, Rejection);
	}
	AddInfo(TEXT("Checked: invalid IDs, side, version, geometry, health, ammunition, lifecycle and order reject atomically for both fresh and initialized destinations."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogUnitRepeatedOperationsTest,
	"FogOfCommand.UnitState.RepeatedOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogUnitRepeatedOperationsTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	TSet<FUnitId> CreatedIds;
	for (int32 Index = 0; Index < 64; ++Index)
	{
		FUnitState Unit;
		const FUnitStateSnapshot Initial = MakeInitialState();
		TestFalse(TEXT("Repeated independent creation receives a new identity"), CreatedIds.Contains(Initial.UnitId));
		CreatedIds.Add(Initial.UnitId);
		if (!CheckResult(*this, TEXT("Repeated lifecycle creation"), Unit.Initialize(Initial), EUnitStateResult::Success)) { return false; }
		CheckResult(*this, TEXT("Mutate before create retry"), Unit.ConsumeAmmunition(5), EUnitStateResult::Success);
		CheckResult(*this, TEXT("Damage before create retry"), Unit.ApplyDamage(10.0), EUnitStateResult::Success);
		const FVector Position(double(Index), -double(Index), 12.5);
		CheckResult(*this, TEXT("Move before create retry"), Unit.MoveTo(Position), EUnitStateResult::Success);
		FUnitStateSnapshot ConflictingValues = Initial;
		ConflictingValues.SideId = FSideId::FromValue(3);
		ConflictingValues.Health = ConflictingValues.MaxHealth;
		ConflictingValues.Ammunition = ConflictingValues.AmmoCapacity;
		ConflictingValues.CurrentOrderId = FOrderId::Create();
		for (int32 Retry = 0; Retry < 4; ++Retry)
		{
			ExpectUnchanged(*this, TEXT("Retry creation cannot heal/refill/change ownership/order"), Unit, EUnitStateResult::AlreadyExists,
				[&] { return Unit.Initialize(ConflictingValues); });
			ExpectUnchanged(*this, TEXT("Move to the same position is idempotent"), Unit, EUnitStateResult::Success,
				[&] { return Unit.MoveTo(Position); });
		}
		CheckResult(*this, TEXT("Destroy repeated lifecycle"), Unit.Destroy(), EUnitStateResult::Success);
		for (int32 Retry = 0; Retry < 4; ++Retry)
		{
			ExpectUnchanged(*this, TEXT("Repeated deletion retains the same tombstone"), Unit, EUnitStateResult::AlreadyDestroyed, [&] { return Unit.Destroy(); });
			ExpectUnchanged(*this, TEXT("Repeated creation retains the same tombstone"), Unit, EUnitStateResult::AlreadyExists,
				[&] { return Unit.Initialize(Initial); });
		}
		TestTrue(TEXT("ID remains unchanged through the full repeated lifecycle"), Unit.GetSnapshot().UnitId == Initial.UnitId);
	}
	TestEqual(TEXT("All independent lifecycles had unique UnitIds"), CreatedIds.Num(), int32(64));
	AddInfo(TEXT("Checked: 64 independent lifecycles with four repeated create/move/destroy retries; no healing, refilling, ownership/order reset or resurrection."));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogUnitSerializationTest,
	"FogOfCommand.UnitState.Serialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogUnitSerializationTest::RunTest(const FString& Parameters)
{
	using namespace FogOfCommand::Domain;
	FUnitState Uninitialized;
	TArray<uint8> Sentinel = { 0xA5, 0x5A, 0x11 };
	const TArray<uint8> OriginalSentinel = Sentinel;
	CheckResult(*this, TEXT("An empty slot cannot serialize"), Uninitialized.Serialize(Sentinel), EUnitStateResult::NotInitialized);
	TestTrue(TEXT("Failed serialization preserves the caller buffer"), Sentinel == OriginalSentinel);
	const FUnitStateSnapshot Initial = MakeInitialState();
	FUnitState Source;
	if (!CheckResult(*this, TEXT("Serialization source"), Source.Initialize(Initial), EUnitStateResult::Success)) { return false; }
	CheckResult(*this, TEXT("Serialization source movement"), Source.MoveTo(FVector(-1234.5, 987.25, -6.5)), EUnitStateResult::Success);
	CheckResult(*this, TEXT("Serialization source damage"), Source.ApplyDamage(12.5), EUnitStateResult::Success);
	CheckResult(*this, TEXT("Serialization source consumption"), Source.ConsumeAmmunition(2), EUnitStateResult::Success);
	TArray<uint8> Bytes;
	if (!CheckResult(*this, TEXT("Serialize v1"), Source.Serialize(Bytes), EUnitStateResult::Success)) { return false; }
	if (!TestEqual(TEXT("Published v1 frame size"), Bytes.Num(), FUnitState::SerializedSize)) { return false; }
	FUnitState Loaded;
	if (!CheckResult(*this, TEXT("Deserialize into a fresh slot"), Loaded.Deserialize(Bytes), EUnitStateResult::Success)) { return false; }
	CheckRoundTripFields(*this, Loaded.GetSnapshot(), Source.GetSnapshot());
	TestTrue(TEXT("Round-trip alive state"), Loaded.IsInitialized() && Loaded.IsAlive() && !Loaded.IsDestroyed());
	TArray<uint8> Again;
	CheckResult(*this, TEXT("Reserialize round trip"), Loaded.Serialize(Again), EUnitStateResult::Success);
	TestTrue(TEXT("Round-trip v1 bytes are stable"), Again == Bytes);
	ExpectUnchanged(*this, TEXT("Loading the same identity cannot replace existing state"), Loaded, EUnitStateResult::AlreadyExists,
		[&] { return Loaded.Deserialize(Bytes); });
	FUnitStateSnapshot DifferentValues = Initial;
	DifferentValues.Ammunition = 0; DifferentValues.Health = 1.0; DifferentValues.CurrentOrderId.Reset();
	FUnitState SameIdentity;
	CheckResult(*this, TEXT("Distinct values with the same imported identity"), SameIdentity.Initialize(DifferentValues), EUnitStateResult::Success);
	ExpectUnchanged(*this, TEXT("Snapshot replay cannot refill/heal an initialized identity"), SameIdentity, EUnitStateResult::AlreadyExists,
		[&] { return SameIdentity.Deserialize(Bytes); });
	FUnitState ProtectedDestination;
	CheckResult(*this, TEXT("Protected destination"), ProtectedDestination.Initialize(MakeInitialState()), EUnitStateResult::Success);
	ExpectUnchanged(*this, TEXT("Deserialize cannot replace another identity"), ProtectedDestination, EUnitStateResult::IdentityConflict,
		[&] { return ProtectedDestination.Deserialize(Bytes); });
	auto RejectBytes = [&](const TCHAR* Description, const TArray<uint8>& Invalid, EUnitStateResult Expected)
	{
		FUnitState Fresh;
		ExpectUnchanged(*this, Description, Fresh, Expected, [&] { return Fresh.Deserialize(Invalid); });
		ExpectUnchanged(*this, Description, ProtectedDestination, Expected, [&] { return ProtectedDestination.Deserialize(Invalid); });
	};
	TArray<uint8> Invalid = Bytes;
	WriteWireUint32(Invalid, 0, FUnitStateSnapshot::CurrentVersion + 1);
	RejectBytes(TEXT("Unsupported wire version"), Invalid, EUnitStateResult::UnsupportedVersion);
	for (int32 Length = 0; Length < Bytes.Num(); ++Length)
	{
		TArray<uint8> Truncated;
		Truncated.Append(Bytes.GetData(), Length);
		RejectBytes(TEXT("Every truncated v1 frame is rejected atomically"), Truncated, EUnitStateResult::MalformedSnapshot);
	}
	Invalid = Bytes; Invalid.Add(0);
	RejectBytes(TEXT("Trailing bytes cannot be ignored"), Invalid, EUnitStateResult::MalformedSnapshot);
	Invalid = Bytes;
	for (int32 Index = 4; Index < 20; ++Index) { Invalid[Index] = 0; }
	RejectBytes(TEXT("Wire zero UnitId"), Invalid, EUnitStateResult::InvalidUnitId);
	Invalid = Bytes; WriteWireUint32(Invalid, 20, 0);
	RejectBytes(TEXT("Wire zero SideId"), Invalid, EUnitStateResult::InvalidSideId);
	Invalid = Bytes; WriteWireUint32(Invalid, 64, 0xFFFFFFFFu);
	RejectBytes(TEXT("Wire negative ammunition"), Invalid, EUnitStateResult::InvalidAmmunition);
	Invalid = Bytes; Invalid[72] = 255;
	RejectBytes(TEXT("Wire invalid life state"), Invalid, EUnitStateResult::InvalidLifeState);
	Invalid = Bytes; Invalid[73] = 2;
	RejectBytes(TEXT("Wire optional-order flag must be boolean"), Invalid, EUnitStateResult::MalformedSnapshot);
	Invalid = Bytes; Invalid[73] = 0;
	RejectBytes(TEXT("Absent wire order cannot conceal a nonzero GUID"), Invalid, EUnitStateResult::MalformedSnapshot);
	Invalid = Bytes;
	for (int32 Index = 74; Index < 90; ++Index) { Invalid[Index] = 0; }
	RejectBytes(TEXT("Present wire order requires a valid GUID"), Invalid, EUnitStateResult::InvalidOrderId);
	// IEEE-754 v1 wire values: quiet NaN, positive infinity and negative infinity.
	for (uint64 Bits : { uint64(0x7FF8000000000000ULL), uint64(0x7FF0000000000000ULL), uint64(0xFFF0000000000000ULL) })
	{
		Invalid = Bytes; WriteWireUint64(Invalid, 24, Bits);
		RejectBytes(TEXT("Nonfinite wire geometry cannot enter a state"), Invalid, EUnitStateResult::InvalidPosition);
		Invalid = Bytes; WriteWireUint64(Invalid, 48, Bits);
		RejectBytes(TEXT("Nonfinite wire health cannot enter a state"), Invalid, EUnitStateResult::InvalidHealth);
		Invalid = Bytes; WriteWireUint64(Invalid, 56, Bits);
		RejectBytes(TEXT("Nonfinite wire maximum health cannot enter a state"), Invalid, EUnitStateResult::InvalidHealth);
	}
	FUnitStateSnapshot NoOrderInitial = MakeInitialState();
	NoOrderInitial.CurrentOrderId.Reset();
	NoOrderInitial.Ammunition = 0;
	FUnitState NoOrder;
	CheckResult(*this, TEXT("Alive source with no order and zero ammunition"), NoOrder.Initialize(NoOrderInitial), EUnitStateResult::Success);
	TArray<uint8> NoOrderBytes;
	CheckResult(*this, TEXT("Serialize alive source with no order"), NoOrder.Serialize(NoOrderBytes), EUnitStateResult::Success);
	FUnitState NoOrderLoaded;
	CheckResult(*this, TEXT("Decode alive source with no order"), NoOrderLoaded.Deserialize(NoOrderBytes), EUnitStateResult::Success);
	CheckRoundTripFields(*this, NoOrderLoaded.GetSnapshot(), NoOrder.GetSnapshot());

	CheckResult(*this, TEXT("Destroy serialization source"), Source.Destroy(), EUnitStateResult::Success);
	TArray<uint8> DeadBytes;
	CheckResult(*this, TEXT("Serialize tombstone without order"), Source.Serialize(DeadBytes), EUnitStateResult::Success);
	FUnitState DeadLoaded;
	if (!CheckResult(*this, TEXT("Deserialize tombstone"), DeadLoaded.Deserialize(DeadBytes), EUnitStateResult::Success)) { return false; }
	CheckRoundTripFields(*this, DeadLoaded.GetSnapshot(), Source.GetSnapshot());
	TestTrue(TEXT("Destroyed lifecycle/order survives round trip"), DeadLoaded.IsDestroyed() && !DeadLoaded.IsAlive()
		&& !DeadLoaded.GetSnapshot().CurrentOrderId.IsSet());
	ExpectUnchanged(*this, TEXT("Loading an alive snapshot cannot resurrect a decoded tombstone"), DeadLoaded, EUnitStateResult::AlreadyExists,
		[&] { return DeadLoaded.Deserialize(Bytes); });
	AddInfo(TEXT("Checked: versioned alive/absent-order/destroyed snapshot round trips preserve every field; unsupported version, every truncated prefix, trailing bytes and malformed identity/resource/lifecycle data leave destinations unchanged."));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
