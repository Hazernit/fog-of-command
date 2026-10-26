#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "FogOfCommandGameMode.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/HitResult.h"
#include "Engine/Level.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
constexpr TCHAR StartupPackage[] = TEXT("/Game/Maps/Startup");
constexpr TCHAR StartupWorld[] = TEXT("/Game/Maps/Startup.Startup");
constexpr TCHAR NativeGameMode[] = TEXT("/Script/FogOfCommand.FogOfCommandGameMode");

bool IsUsableLight(const ULightComponentBase* Light)
{
	return Light && Light->bAffectsWorld && Light->IsVisible()
		&& FMath::IsFinite(Light->Intensity) && Light->Intensity > 0.0f;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOfCommandConfigurationSmokeTest,
	"FogOfCommand.Smoke.Configuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOfCommandConfigurationSmokeTest::RunTest(const FString& Parameters)
{
	const UGameMapsSettings* Maps = GetDefault<UGameMapsSettings>();
	TestEqual(TEXT("Editor startup map resolves to Startup"), Maps->EditorStartupMap.ToString(), FString(StartupWorld));
	TestEqual(TEXT("Client default map resolves to Startup"),
		UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client), FString(StartupPackage));
	TestEqual(TEXT("Global GameMode resolves to the native project class"),
		UGameMapsSettings::GetGlobalDefaultGameMode(), FString(NativeGameMode));

	const FEngineVersion& Version = FEngineVersion::Current();
	TestEqual(TEXT("Running pinned Unreal version"), Version.ToString(EVersionComponent::Patch), FString(TEXT("5.8.4")));
	TestEqual(TEXT("Running pinned Unreal changelist"), Version.GetChangelist(), uint32(58902052));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOfCommandModuleSmokeTest,
	"FogOfCommand.Smoke.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOfCommandModuleSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("FogOfCommand runtime module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("FogOfCommand")));
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, *UGameMapsSettings::GetGlobalDefaultGameMode());
	if (!TestNotNull(TEXT("Configured GameMode class can be loaded"), GameModeClass))
	{
		return false;
	}
	TestEqual(TEXT("Configured class is FogOfCommandGameMode"), GameModeClass, AFogOfCommandGameMode::StaticClass());
	TestTrue(TEXT("GameMode is a native C++ class"), GameModeClass->HasAnyClassFlags(CLASS_Native));

	const AGameModeBase* Defaults = GetDefault<AGameModeBase>(GameModeClass);
	TestEqual(TEXT("GameMode supplies the standard DefaultPawn"), Defaults->DefaultPawnClass.Get(), ADefaultPawn::StaticClass());
	TestNotNull(TEXT("GameMode supplies a player controller"), Defaults->PlayerControllerClass.Get());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogOfCommandStartupMapSmokeTest,
	"FogOfCommand.Smoke.StartupMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFogOfCommandStartupMapSmokeTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Editor is available for saved-map validation"), GEditor))
	{
		return false;
	}
	if (GEditor->PlayWorld)
	{
		AddError(TEXT("Run StartupMap in an isolated Editor process with no active PIE session."));
		return false;
	}
	TArray<UPackage*> DirtyWorlds;
	FEditorFileUtils::GetDirtyWorldPackages(DirtyWorlds);
	if (!DirtyWorlds.IsEmpty())
	{
		AddError(TEXT("Refusing to replace a world with unsaved map changes. Use the isolated smoke runner."));
		return false;
	}

	FString MapFilename;
	if (!TestTrue(TEXT("Saved Startup map package exists"), FPackageName::DoesPackageExist(StartupPackage, &MapFilename)))
	{
		return false;
	}
	FAutomationEditorCommonUtils::LoadMap(MapFilename);
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!TestNotNull(TEXT("Saved map creates an Editor world"), World))
	{
		return false;
	}
	if (!TestEqual(TEXT("Loaded world is the saved Startup asset"), World->GetPathName(), FString(StartupWorld)))
	{
		return false;
	}
	TestFalse(TEXT("Startup does not require World Partition"), World->IsPartitionedWorld());
	if (!TestNotNull(TEXT("Startup has a persistent level"), World->PersistentLevel.Get()))
	{
		return false;
	}
	TestFalse(TEXT("Startup does not require external actor packages"), World->PersistentLevel->IsUsingExternalActors());
	const AWorldSettings* Settings = World->GetWorldSettings(false, false);
	if (!TestNotNull(TEXT("Startup has WorldSettings"), Settings))
	{
		return false;
	}
	TestEqual(TEXT("Saved map selects the native GameMode"), Settings->DefaultGameMode.Get(), AFogOfCommandGameMode::StaticClass());

	int32 MeshCount = 0;
	TArray<AStaticMeshActor*> Floors;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		++MeshCount;
		const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
		if (TestNotNull(TEXT("A display actor has a mesh component"), Mesh))
		{
			TestNotNull(TEXT("A display mesh has a loaded asset"), Mesh->GetStaticMesh().Get());
		}
		if (It->GetActorLabel() == TEXT("FOC_Floor"))
		{
			Floors.Add(*It);
		}
	}
	TestEqual(TEXT("Startup has a floor and three display meshes"), MeshCount, 4);
	if (!TestEqual(TEXT("Startup has one identifiable floor"), Floors.Num(), 1))
	{
		return false;
	}
	AStaticMeshActor* Floor = Floors[0];
	const UStaticMeshComponent* FloorComponent = Floor->GetStaticMeshComponent();
	if (!TestNotNull(TEXT("Floor has a collision component"), FloorComponent))
	{
		return false;
	}
	TestTrue(TEXT("Floor supports query and physics collision"), FloorComponent->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
	TestTrue(TEXT("Floor blocks pawns"), FloorComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
	TestTrue(TEXT("Floor blocks visibility traces"), FloorComponent->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block);

	bool bHasSun = false;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		bHasSun |= IsUsableLight(It->GetLightComponent());
	}
	bool bHasSkyLight = false;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		bHasSkyLight |= IsUsableLight(It->FindComponentByClass<USkyLightComponent>());
	}
	TestTrue(TEXT("Startup has an enabled positive-intensity sun"), bHasSun);
	TestTrue(TEXT("Startup has an enabled positive-intensity sky light"), bHasSkyLight);

	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Starts.Add(*It);
	}
	if (!TestEqual(TEXT("Startup has one PlayerStart"), Starts.Num(), 1))
	{
		return false;
	}
	APlayerStart* Start = Starts[0];
	const FVector Spawn = Start->GetActorLocation();
	TestFalse(TEXT("PlayerStart coordinates are finite"), Spawn.ContainsNaN());
	const float PawnRadius = GetDefault<ADefaultPawn>()->GetCollisionComponent()->GetScaledSphereRadius();
	if (!TestTrue(TEXT("DefaultPawn collision radius is finite and positive"), FMath::IsFinite(PawnRadius) && PawnRadius > 0.0f))
	{
		return false;
	}
	FVector FloorOrigin;
	FVector FloorExtent;
	Floor->GetActorBounds(true, FloorOrigin, FloorExtent);
	TestTrue(TEXT("PlayerStart is over the floor with pawn-width clearance"),
		FMath::Abs(Spawn.X - FloorOrigin.X) + PawnRadius < FloorExtent.X
		&& FMath::Abs(Spawn.Y - FloorOrigin.Y) + PawnRadius < FloorExtent.Y);
	TestTrue(TEXT("DefaultPawn starts entirely above the floor"), Spawn.Z - PawnRadius > FloorOrigin.Z + FloorExtent.Z);

	FCollisionQueryParams Query;
	Query.AddIgnoredActor(Start);
	FHitResult FloorHit;
	const FVector BelowFloor(Spawn.X, Spawn.Y, FloorOrigin.Z - FloorExtent.Z - PawnRadius);
	if (TestTrue(TEXT("A real collision trace finds support below PlayerStart"),
		World->LineTraceSingleByChannel(FloorHit, Spawn, BelowFloor, ECC_Visibility, Query)))
	{
		TestTrue(TEXT("PlayerStart support is the floor"), FloorHit.GetActor() == Floor);
	}
	TestFalse(TEXT("DefaultPawn fits at PlayerStart without a blocking overlap"),
		World->OverlapBlockingTestByChannel(Spawn, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(PawnRadius), Query));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
