#pragma once

#include "Containers/Array.h"
#include "Domain/UnitIdentifiers.h"
#include "Math/Vector.h"
#include "Misc/Optional.h"

namespace FogOfCommand::Domain
{
enum class EUnitLifeState : uint8
{
	Alive = 0,
	Destroyed = 1
};

enum class EUnitStateResult : uint8
{
	Success,
	AlreadyExists,
	IdentityConflict,
	NotInitialized,
	Destroyed,
	AlreadyDestroyed,
	InvalidUnitId,
	InvalidSideId,
	InvalidPosition,
	InvalidHealth,
	InvalidAmmunition,
	InvalidLifeState,
	InvalidOrderId,
	InvalidAmount,
	InsufficientAmmunition,
	UnsupportedVersion,
	MalformedSnapshot,
	OrderConflict
};

/** Mutable import/export value. Only validated copies can enter FUnitState. */
struct FOGOFCOMMAND_API FUnitStateSnapshot
{
	static constexpr uint32 CurrentVersion = 1;
	uint32 Version = CurrentVersion;
	FUnitId UnitId;
	FSideId SideId;
	FVector Position = FVector::ZeroVector;
	double Health = 100.0;
	double MaxHealth = 100.0;
	int32 Ammunition = 0;
	int32 AmmoCapacity = 0;
	EUnitLifeState Life = EUnitLifeState::Alive;
	TOptional<FOrderId> CurrentOrderId;

	bool operator==(const FUnitStateSnapshot& Other) const;
	bool operator!=(const FUnitStateSnapshot& Other) const { return !(*this == Other); }
};

/**
 * One entity's Core-only state slot, not a registry or a scene object.
 * A successful first Initialize/Deserialize fixes its identity and initial data.
 * Retrying with the same valid ID returns AlreadyExists without resetting anything,
 * including a tombstone. A different valid ID returns IdentityConflict.
 */
class FOGOFCOMMAND_API FUnitState
{
public:
	FUnitState() = default;
	/** A copy denotes the same entity; it does not allocate another identity. */
	FUnitState(const FUnitState&) = default;
	/** Assignment must not bypass first-creation-wins or reset a destroyed slot. */
	FUnitState& operator=(const FUnitState&) = delete;
	FUnitState& operator=(FUnitState&&) = delete;

	/** Fixed v1 wire size, including an optional-order flag and zero/valid order GUID. */
	static constexpr int32 SerializedSize = 90;
	bool IsInitialized() const { return bInitialized; }
	bool IsAlive() const { return bInitialized && Data.Life == EUnitLifeState::Alive; }
	bool IsDestroyed() const { return bInitialized && Data.Life == EUnitLifeState::Destroyed; }
	/** The default slot exposes invalid IDs until initialization; no mutable state is exposed. */
	const FUnitStateSnapshot& GetSnapshot() const { return Data; }

	/** Validates version, IDs, finite geometry/resources and lifecycle coherence, without mutation. */
	static EUnitStateResult ValidateSnapshot(const FUnitStateSnapshot& Snapshot);
	EUnitStateResult Initialize(const FUnitStateSnapshot& Initial);
	/** Pure position change; this performs no navigation, scene movement or order execution. */
	EUnitStateResult MoveTo(const FVector& Position);
	/** Nonnegative finite damage. Lethal damage destroys; zero is a valid no-op while alive. */
	EUnitStateResult ApplyDamage(double Amount);
	/** Nonnegative integer consumption, with an explicit insufficient-stock failure. */
	EUnitStateResult ConsumeAmmunition(int32 Amount);
	/** Terminal and idempotent: retain ID/side/position/ammo, zero health, clear opaque order. */
	EUnitStateResult Destroy();
	/**
	 * Validate identity and replace the opaque link only if it still equals Expected.
	 * Expected/Desired may be empty, but set IDs must be valid. Requires a living unit;
	 * a mismatch or invalid input leaves all unit data unchanged. This stores identity
	 * metadata only; order ownership and lifecycle belong to FUnitOrders.
	 */
	EUnitStateResult CompareExchangeCurrentOrderId(const FUnitId& ExpectedUnitId,
		const TOptional<FOrderId>& Expected, const TOptional<FOrderId>& Desired);

	/**
	 * Encode valid initialized state. Failure leaves OutBytes unchanged.
	 * v1 is little endian: uint32 version, GUID unit (4 uint32), int32 side,
	 * 3 doubles position, doubles health/max, int32 ammo/capacity,
	 * uint8 life, uint8 has-order (0/1), GUID order (zero when absent).
	 */
	EUnitStateResult Serialize(TArray<uint8>& OutBytes) const;
	/**
	 * Decode and validate a complete value before Initialize. Unknown versions,
	 * truncation, trailing bytes and invalid fields leave this slot unchanged.
	 * This is first-creation import, not an overwrite/restore/heal API.
	 */
	EUnitStateResult Deserialize(const TArray<uint8>& Bytes);

private:
	bool bInitialized = false;
	FUnitStateSnapshot Data;
};
}
