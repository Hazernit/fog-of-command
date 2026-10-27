#include "Domain/UnitState.h"

#include "Math/UnrealMathUtility.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace FogOfCommand::Domain
{
namespace
{
bool IsFinitePosition(const FVector& Position)
{
	return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z);
}

EUnitStateResult RequireAlive(const FUnitState& Unit)
{
	if (!Unit.IsInitialized())
	{
		return EUnitStateResult::NotInitialized;
	}
	return Unit.IsDestroyed() ? EUnitStateResult::Destroyed : EUnitStateResult::Success;
}

// Explicit scalar fields avoid native struct padding and engine-version-dependent FVector encoding.
struct FWireSnapshot
{
	uint32 Version = FUnitStateSnapshot::CurrentVersion;
	FGuid Unit;
	int32 Side = 0;
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
	double Health = 0.0;
	double MaxHealth = 0.0;
	int32 Ammunition = 0;
	int32 AmmoCapacity = 0;
	uint8 Life = 0;
	uint8 HasOrder = 0;
	FGuid Order;
};

void SerializePayload(FArchive& Archive, FWireSnapshot& Wire)
{
	Archive << Wire.Unit << Wire.Side << Wire.X << Wire.Y << Wire.Z;
	Archive << Wire.Health << Wire.MaxHealth << Wire.Ammunition << Wire.AmmoCapacity;
	Archive << Wire.Life << Wire.HasOrder << Wire.Order;
}
}

bool FUnitStateSnapshot::operator==(const FUnitStateSnapshot& Other) const
{
	return Version == Other.Version && UnitId == Other.UnitId && SideId == Other.SideId
		&& Position == Other.Position && Health == Other.Health && MaxHealth == Other.MaxHealth
		&& Ammunition == Other.Ammunition && AmmoCapacity == Other.AmmoCapacity
		&& Life == Other.Life && CurrentOrderId == Other.CurrentOrderId;
}

EUnitStateResult FUnitState::ValidateSnapshot(const FUnitStateSnapshot& Snapshot)
{
	if (Snapshot.Version != FUnitStateSnapshot::CurrentVersion)
	{
		return EUnitStateResult::UnsupportedVersion;
	}
	if (!Snapshot.UnitId.IsValid())
	{
		return EUnitStateResult::InvalidUnitId;
	}
	if (!Snapshot.SideId.IsValid())
	{
		return EUnitStateResult::InvalidSideId;
	}
	if (!IsFinitePosition(Snapshot.Position))
	{
		return EUnitStateResult::InvalidPosition;
	}
	if (!FMath::IsFinite(Snapshot.Health) || !FMath::IsFinite(Snapshot.MaxHealth)
		|| Snapshot.MaxHealth <= 0.0 || Snapshot.Health < 0.0 || Snapshot.Health > Snapshot.MaxHealth)
	{
		return EUnitStateResult::InvalidHealth;
	}
	if (Snapshot.Ammunition < 0 || Snapshot.AmmoCapacity < 0 || Snapshot.Ammunition > Snapshot.AmmoCapacity)
	{
		return EUnitStateResult::InvalidAmmunition;
	}
	if (Snapshot.Life != EUnitLifeState::Alive && Snapshot.Life != EUnitLifeState::Destroyed)
	{
		return EUnitStateResult::InvalidLifeState;
	}
	if ((Snapshot.Life == EUnitLifeState::Alive && Snapshot.Health <= 0.0)
		|| (Snapshot.Life == EUnitLifeState::Destroyed && (Snapshot.Health != 0.0 || Snapshot.CurrentOrderId.IsSet())))
	{
		return EUnitStateResult::InvalidLifeState;
	}
	if (Snapshot.CurrentOrderId.IsSet() && !Snapshot.CurrentOrderId.GetValue().IsValid())
	{
		return EUnitStateResult::InvalidOrderId;
	}
	return EUnitStateResult::Success;
}

EUnitStateResult FUnitState::Initialize(const FUnitStateSnapshot& Initial)
{
	const EUnitStateResult Validation = ValidateSnapshot(Initial);
	if (Validation != EUnitStateResult::Success)
	{
		return Validation;
	}
	if (bInitialized)
	{
		return Data.UnitId == Initial.UnitId ? EUnitStateResult::AlreadyExists : EUnitStateResult::IdentityConflict;
	}
	Data = Initial;
	bInitialized = true;
	return EUnitStateResult::Success;
}

EUnitStateResult FUnitState::MoveTo(const FVector& Position)
{
	const EUnitStateResult Precondition = RequireAlive(*this);
	if (Precondition != EUnitStateResult::Success)
	{
		return Precondition;
	}
	if (!IsFinitePosition(Position))
	{
		return EUnitStateResult::InvalidPosition;
	}
	Data.Position = Position;
	return EUnitStateResult::Success;
}

EUnitStateResult FUnitState::ApplyDamage(double Amount)
{
	const EUnitStateResult Precondition = RequireAlive(*this);
	if (Precondition != EUnitStateResult::Success)
	{
		return Precondition;
	}
	if (!FMath::IsFinite(Amount) || Amount < 0.0)
	{
		return EUnitStateResult::InvalidAmount;
	}
	if (Amount >= Data.Health)
	{
		return Destroy();
	}
	const double Remaining = Data.Health - Amount;
	if (Remaining <= 0.0)
	{
		return Destroy();
	}
	Data.Health = Remaining;
	return EUnitStateResult::Success;
}

EUnitStateResult FUnitState::ConsumeAmmunition(int32 Amount)
{
	const EUnitStateResult Precondition = RequireAlive(*this);
	if (Precondition != EUnitStateResult::Success)
	{
		return Precondition;
	}
	if (Amount < 0)
	{
		return EUnitStateResult::InvalidAmount;
	}
	if (Amount > Data.Ammunition)
	{
		return EUnitStateResult::InsufficientAmmunition;
	}
	Data.Ammunition -= Amount;
	return EUnitStateResult::Success;
}

EUnitStateResult FUnitState::Destroy()
{
	if (!bInitialized)
	{
		return EUnitStateResult::NotInitialized;
	}
	if (IsDestroyed())
	{
		return EUnitStateResult::AlreadyDestroyed;
	}
	Data.Health = 0.0;
	Data.Life = EUnitLifeState::Destroyed;
	Data.CurrentOrderId.Reset();
	return EUnitStateResult::Success;
}

EUnitStateResult FUnitState::CompareExchangeCurrentOrderId(const FUnitId& ExpectedUnitId,
	const TOptional<FOrderId>& Expected, const TOptional<FOrderId>& Desired)
{
	if (!bInitialized)
	{
		return EUnitStateResult::NotInitialized;
	}
	if (!ExpectedUnitId.IsValid())
	{
		return EUnitStateResult::InvalidUnitId;
	}
	if (ExpectedUnitId != Data.UnitId)
	{
		return EUnitStateResult::IdentityConflict;
	}
	if ((Expected.IsSet() && !Expected.GetValue().IsValid()) || (Desired.IsSet() && !Desired.GetValue().IsValid()))
	{
		return EUnitStateResult::InvalidOrderId;
	}
	if (IsDestroyed())
	{
		return EUnitStateResult::Destroyed;
	}
	if (Data.CurrentOrderId != Expected)
	{
		return EUnitStateResult::OrderConflict;
	}
	Data.CurrentOrderId = Desired;
	return EUnitStateResult::Success;
}

EUnitStateResult FUnitState::Serialize(TArray<uint8>& OutBytes) const
{
	if (!bInitialized)
	{
		return EUnitStateResult::NotInitialized;
	}
	const EUnitStateResult Validation = ValidateSnapshot(Data);
	if (Validation != EUnitStateResult::Success)
	{
		return Validation;
	}
	FWireSnapshot Wire;
	Wire.Version = Data.Version;
	Wire.Unit = Data.UnitId.GetValue();
	Wire.Side = Data.SideId.GetValue();
	Wire.X = Data.Position.X;
	Wire.Y = Data.Position.Y;
	Wire.Z = Data.Position.Z;
	Wire.Health = Data.Health;
	Wire.MaxHealth = Data.MaxHealth;
	Wire.Ammunition = Data.Ammunition;
	Wire.AmmoCapacity = Data.AmmoCapacity;
	Wire.Life = static_cast<uint8>(Data.Life);
	Wire.HasOrder = Data.CurrentOrderId.IsSet() ? 1 : 0;
	Wire.Order = Data.CurrentOrderId.IsSet() ? Data.CurrentOrderId.GetValue().GetValue() : FGuid();

	TArray<uint8> Encoded;
	{
		FMemoryWriter Writer(Encoded, true);
		Writer.SetByteSwapping(!PLATFORM_LITTLE_ENDIAN);
		Writer << Wire.Version;
		SerializePayload(Writer, Wire);
		if (Writer.IsError())
		{
			return EUnitStateResult::MalformedSnapshot;
		}
	}
	if (Encoded.Num() != SerializedSize)
	{
		return EUnitStateResult::MalformedSnapshot;
	}
	OutBytes = MoveTemp(Encoded);
	return EUnitStateResult::Success;
}

EUnitStateResult FUnitState::Deserialize(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() < int32(sizeof(uint32)))
	{
		return EUnitStateResult::MalformedSnapshot;
	}
	FWireSnapshot Wire;
	FMemoryReader Reader(Bytes, true);
	Reader.SetByteSwapping(!PLATFORM_LITTLE_ENDIAN);
	Reader << Wire.Version;
	if (Reader.IsError())
	{
		return EUnitStateResult::MalformedSnapshot;
	}
	if (Wire.Version != FUnitStateSnapshot::CurrentVersion)
	{
		return EUnitStateResult::UnsupportedVersion;
	}
	if (Bytes.Num() != SerializedSize)
	{
		return EUnitStateResult::MalformedSnapshot;
	}
	SerializePayload(Reader, Wire);
	if (Reader.IsError() || !Reader.AtEnd() || Wire.HasOrder > 1 || (Wire.HasOrder == 0 && Wire.Order.IsValid()))
	{
		return EUnitStateResult::MalformedSnapshot;
	}
	// Reject raw nonfinite coordinates before FVector's constructor diagnostics can alter them.
	if (!FMath::IsFinite(Wire.X) || !FMath::IsFinite(Wire.Y) || !FMath::IsFinite(Wire.Z))
	{
		return EUnitStateResult::InvalidPosition;
	}

	FUnitStateSnapshot Candidate;
	Candidate.Version = Wire.Version;
	Candidate.UnitId = FUnitId::FromGuid(Wire.Unit);
	Candidate.SideId = FSideId::FromValue(Wire.Side);
	Candidate.Position = FVector(Wire.X, Wire.Y, Wire.Z);
	Candidate.Health = Wire.Health;
	Candidate.MaxHealth = Wire.MaxHealth;
	Candidate.Ammunition = Wire.Ammunition;
	Candidate.AmmoCapacity = Wire.AmmoCapacity;
	Candidate.Life = static_cast<EUnitLifeState>(Wire.Life);
	if (Wire.HasOrder != 0)
	{
		Candidate.CurrentOrderId = FOrderId::FromGuid(Wire.Order);
	}
	return Initialize(Candidate);
}
}
