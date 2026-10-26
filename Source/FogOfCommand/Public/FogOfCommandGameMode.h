#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FogOfCommandGameMode.generated.h"

/** Startup GameMode using Unreal's standard free-flying pawn. */
UCLASS()
class FOGOFCOMMAND_API AFogOfCommandGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFogOfCommandGameMode();

protected:
	virtual void BeginPlay() override;
};
