#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GemSkyRig.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

/**
 * Sun + sky light + atmosphere in one actor. The game mode spawns it when the level has no sun of its
 * own, which is what lets Gem Roller run on a completely empty map.
 */
UCLASS()
class GEMROLLER_API AGemSkyRig : public AActor
{
	GENERATED_BODY()

public:
	AGemSkyRig();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;
};
