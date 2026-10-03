#pragma once

#include "CoreMinimal.h"
#include "GRAssets.h"
#include "GameFramework/Actor.h"
#include "CourseProp.generated.h"

class AGemRollerGameMode;
class AMarblePawn;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

enum class ECoursePropKind : uint8
{
	Decor,       // rocks, trees, clouds: no gameplay
	Island,      // solid round platform
	StepStone,   // small solid platform
	Mover,       // platform that slides between PointA and PointB
	Spinner,     // rotating hazard bar that shoves the marble
	Gem,
	TimeCrystal,
	Checkpoint,  // flag; the game mode lights it up when the island is reached
	Goal,        // finish ring
	JumpPad,     // launches the marble onto the next island
	BoostPad     // speed strip
};

/**
 * One generic actor for everything the course is made of. The game mode spawns it and calls
 * Initialize(); Tick then does whatever that kind of prop does (spin, bob, move, detect the marble).
 *
 * Visuals come from the Blender meshes (or a placeholder primitive). Collision never depends on the
 * imported mesh: solid props get an invisible engine cylinder/cube of the right size instead.
 */
UCLASS()
class GEMROLLER_API ACourseProp : public AActor
{
	GENERATED_BODY()

public:
	ACourseProp();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Initialize(AGemRollerGameMode* InGameMode, ECoursePropKind InKind, EGRMesh MeshId, const FLinearColor& Tint, float Emissive = 0.f);

	/** Checkpoint flag turns green. */
	void SetCheckpointActive(bool bActive);

	// Per-kind data, filled in by the game mode right after Initialize().
	float SpinRate = 0.f;         // deg/s (spinner, gem yaw)
	float BobAmplitude = 0.f;     // UU
	float BobSpeed = 1.f;         // rad/s
	float Phase = 0.f;            // radians
	FVector PointA = FVector::ZeroVector;  // mover start / jump pad landing
	FVector PointB = FVector::ZeroVector;  // mover end
	float Period = 5.f;           // mover period, seconds
	float FlightTime = 1.2f;      // jump pad
	FVector Direction = FVector::ForwardVector;  // boost pad
	bool bGolden = false;

private:
	void ApplyVisual(EGRMesh MeshId, const FLinearColor& Tint, float Emissive);
	void MakeCylinderCollider(UStaticMeshComponent* Comp, float Diameter, float Height, float ZCenter);
	void MakeBoxCollider(UStaticMeshComponent* Comp, const FVector& Size, const FVector& Center);
	UStaticMesh* GetShape(EGRShape Shape) const;

	AMarblePawn* GetMarble() const;
	bool MarbleWithin(const AMarblePawn* Marble, float Radius) const;
	bool MarbleOnPad(const AMarblePawn* Marble, float Radius) const;
	void TickSpinnerShove(AMarblePawn* Marble);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ColliderA;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ColliderB;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> VisualMaterial;

	TWeakObjectPtr<AGemRollerGameMode> GameMode;
	ECoursePropKind Kind = ECoursePropKind::Decor;

	FVector VisualBaseLocation = FVector::ZeroVector;
	FVector VisualBaseScale = FVector(1.0, 1.0, 1.0);
	FRotator VisualBaseRotation = FRotator::ZeroRotator;
	float ElapsedTime = 0.f;
	float Cooldown = 0.f;
	float Pulse = 0.f;

	bool bCollected = false;
	float CollectTimer = 0.f;
	bool bTriggered = false;
};
