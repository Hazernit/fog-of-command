#include "FogOfCommandGameMode.h"

#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogFogOfCommand, Log, All);

AFogOfCommandGameMode::AFogOfCommandGameMode()
{
	DefaultPawnClass = ADefaultPawn::StaticClass();
}

void AFogOfCommandGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogFogOfCommand, Display,
		TEXT("FOC-002 GameMode BeginPlay: map=%s, default pawn=%s"),
		*GetWorld()->GetMapName(), *GetNameSafe(DefaultPawnClass.Get()));
}
