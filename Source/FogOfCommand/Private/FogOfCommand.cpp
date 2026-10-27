#include "Modules/ModuleManager.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Debug/EventLogDebug.h"
#endif

class FFogOfCommandModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		FogOfCommand::Debug::RegisterEventLogDebugCommands();
#endif
	}

	virtual void ShutdownModule() override
	{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		FogOfCommand::Debug::UnregisterEventLogDebugCommands();
#endif
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FFogOfCommandModule, FogOfCommand, "FogOfCommand");
