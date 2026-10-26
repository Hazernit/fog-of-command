#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "Templates/TypeHash.h"

namespace FogOfCommand::Domain
{
/** Persistent entity identity. Copy/import preserves identity; Create allocates a new GUID. */
class FOGOFCOMMAND_API FUnitId
{
public:
	FUnitId() = default;
	static FUnitId Create();
	/** Explicit raw import; a zero GUID remains invalid and cannot initialize a unit. */
	static FUnitId FromGuid(const FGuid& Guid) { return FUnitId(Guid); }
	bool IsValid() const { return Value.IsValid(); }
	const FGuid& GetValue() const { return Value; }
	friend bool operator==(const FUnitId& A, const FUnitId& B) { return A.Value == B.Value; }
	friend bool operator!=(const FUnitId& A, const FUnitId& B) { return !(A == B); }
	friend uint32 GetTypeHash(const FUnitId& Id) { return GetTypeHash(Id.Value); }

private:
	explicit FUnitId(const FGuid& Guid) : Value(Guid) {}
	FGuid Value;
};

/** Opaque order identity only: no order kind, queue, execution or ownership policy. */
class FOGOFCOMMAND_API FOrderId
{
public:
	FOrderId() = default;
	static FOrderId Create();
	static FOrderId FromGuid(const FGuid& Guid) { return FOrderId(Guid); }
	bool IsValid() const { return Value.IsValid(); }
	const FGuid& GetValue() const { return Value; }
	friend bool operator==(const FOrderId& A, const FOrderId& B) { return A.Value == B.Value; }
	friend bool operator!=(const FOrderId& A, const FOrderId& B) { return !(A == B); }
	friend uint32 GetTypeHash(const FOrderId& Id) { return GetTypeHash(Id.Value); }

private:
	explicit FOrderId(const FGuid& Guid) : Value(Guid) {}
	FGuid Value;
};

/** Positive side number; default zero and explicitly imported negative values are invalid. */
class FOGOFCOMMAND_API FSideId
{
public:
	FSideId() = default;
	static FSideId FromValue(int32 Number) { return FSideId(Number); }
	bool IsValid() const { return Value > 0; }
	int32 GetValue() const { return Value; }
	friend bool operator==(const FSideId& A, const FSideId& B) { return A.Value == B.Value; }
	friend bool operator!=(const FSideId& A, const FSideId& B) { return !(A == B); }
	friend uint32 GetTypeHash(const FSideId& Id) { return ::GetTypeHash(Id.Value); }

private:
	explicit FSideId(int32 Number) : Value(Number) {}
	int32 Value = 0;
};
}
