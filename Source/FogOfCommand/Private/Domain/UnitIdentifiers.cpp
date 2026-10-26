#include "Domain/UnitIdentifiers.h"

namespace FogOfCommand::Domain
{
FUnitId FUnitId::Create()
{
	return FUnitId(FGuid::NewGuid());
}

FOrderId FOrderId::Create()
{
	return FOrderId(FGuid::NewGuid());
}
}
