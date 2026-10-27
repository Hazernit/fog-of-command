#pragma once

#include "CoreTypes.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
namespace FogOfCommand::Debug
{
void RegisterEventLogDebugCommands();
void UnregisterEventLogDebugCommands();
}
#endif
