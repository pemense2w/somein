#include "CourseProp.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GRLayout.h"
#include "GemRollerGameMode.h"
#include "MarblePawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CollectDuration = 0.28f;
	constexpr float GemPickupRadius = 105.f;
	constexpr float ActiveCheckpointGlow = 1.f;
}

ACourseProp::ACourseProp()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);

	ColliderA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ColliderA"));
	ColliderA->SetupAttachment(Root);
	ColliderA->SetMobility(EComponentMobility::Movable);
	ColliderA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ColliderA->SetVisibility(false);
	ColliderA->SetCanEverAffectNavigation(false);

	ColliderB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ColliderB"));
	ColliderB->SetupAttachment(Root);
	ColliderB->SetMobility(EComponentMobility::Movable);
	ColliderB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ColliderB->SetVisibility(false);
	ColliderB->SetCanEverAffectNavigation(false);

	// Engine primitives: referenced here so they are always cooked, used as colliders and placeholders.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (SphereFinder.Succeeded())
	{
		SphereMesh = SphereFinder.Object;
	}
	if (CylinderFinder.Succeeded())
	{
		CylinderMesh = CylinderFinder.Object;
	}
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}
}

UStaticMesh* ACourseProp::GetShape(EGRShape Shape) const
{
	switch (Shape)
	{
	case EGRShape::Cylinder:
		return CylinderMesh;
	case EGRShape::Cube:
		return CubeMesh;
	default:
		return SphereMesh;
	}
}

void ACourseProp::ApplyVisual(EGRMesh MeshId, const FLinearColor& Tint, float Emissive)
{
	FLinearColor FinalTint = Tint;
	if (UStaticMesh* Imported = GRAssets::FindMesh(MeshId))
	{
		Visual->SetStaticMesh(Imported);
		Visual->SetRelativeScale3D(FVector(1.0, 1.0, 1.0));
		Visual->SetRelativeLocation(FVector::ZeroVector);
	}
	else
	{
		const FGRFallback Fallback = GRAssets::GetFallback(MeshId);
		Visual->SetStaticMesh(GetShape(Fallback.Shape));
		Visual->SetRelativeScale3D(Fallback.Scale);
		Visual->SetRelativeLocation(FVector(0.0, 0.0, Fallback.ZOffset));
		FinalTint = Tint * Fallback.Color;  // placeholders have no vertex colours, so bake the colour in
	}

	VisualMaterial = GRAssets::MakeMaterial(this, FinalTint, Emissive);
	if (VisualMaterial != nullptr)
	{
		for (int32 Slot = 0; Slot < Visual->GetNumMaterials(); ++Slot)
		{
			Visual->SetMaterial(Slot, VisualMaterial);
		}
	}

	VisualBaseLocation = Visual->GetRelativeLocation();
	VisualBaseScale = Visual->GetRelativeScale3D();
	VisualBaseRotation = Visual->GetRelativeRotation();
}

void ACourseProp::MakeCylinderCollider(UStaticMeshComponent* Comp, float Diameter, float Height, float ZCenter)
{
	Comp->SetStaticMesh(CylinderMesh);  // engine cylinder is 100 x 100 x 100, centred
	Comp->SetRelativeScale3D(FVector(Diameter / 100.f, Diameter / 100.f, Height / 100.f));
	Comp->SetRelativeLocation(FVector(0.0, 0.0, ZCenter));
	Comp->SetCollisionProfileName(TEXT("BlockAll"));
	Comp->SetVisibility(false);
}

void ACourseProp::MakeBoxCollider(UStaticMeshComponent* Comp, const FVector& Size, const FVector& Center)
{
	Comp->SetStaticMesh(CubeMesh);  // engine cube is 100 x 100 x 100, centred
	Comp->SetRelativeScale3D(Size / 100.0);
	Comp->SetRelativeLocation(Center);
	Comp->SetCollisionProfileName(TEXT("BlockAll"));
	Comp->SetVisibility(false);
}

void ACourseProp::Initialize(AGemRollerGameMode* InGameMode, ECoursePropKind InKind, EGRMesh MeshId, const FLinearColor& Tint, float Emissive)
{
	GameMode = InGameMode;
	Kind = InKind;

	ApplyVisual(MeshId, Tint, Emissive);

	switch (Kind)
	{
	case ECoursePropKind::Island:
		MakeCylinderCollider(ColliderA, 700.f, 60.f, -30.f);
		break;
	case ECoursePropKind::StepStone:
	case ECoursePropKind::Mover:
		MakeCylinderCollider(ColliderA, 240.f, 50.f, -25.f);
		break;
	case ECoursePropKind::Spinner:
		MakeBoxCollider(ColliderA, FVector(440.0, 38.0, 50.0), FVector(0.0, 0.0, 25.0));
		MakeCylinderCollider(ColliderB, 110.f, 80.f, 40.f);
		break;
	case ECoursePropKind::Goal:
		// The ring is vertical: the Blender torus lies flat, so stand it up. Actor yaw aims it down the course.
		Visual->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
		VisualBaseRotation = Visual->GetRelativeRotation();
		break;
	default:
		break;
	}

}

void ACourseProp::BeginPlay()
{
	Super::BeginPlay();

	// Only things that move or react need a tick (the game mode has filled in BobAmplitude etc. by now).
	const bool bStatic = Kind == ECoursePropKind::Island || Kind == ECoursePropKind::StepStone ||
	                     Kind == ECoursePropKind::Checkpoint || (Kind == ECoursePropKind::Decor && BobAmplitude <= 0.f);
	SetActorTickEnabled(!bStatic);
}

void ACourseProp::SetCheckpointActive(bool bActive)
{
	if (VisualMaterial == nullptr)
	{
		return;
	}
	const FLinearColor Color = bActive ? FLinearColor(0.35f, 1.f, 0.5f) : FLinearColor::White;
	VisualMaterial->SetVectorParameterValue(TEXT("Tint"), Color);
	VisualMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	VisualMaterial->SetScalarParameterValue(TEXT("Emissive"), bActive ? ActiveCheckpointGlow : 0.f);
}

AMarblePawn* ACourseProp::GetMarble() const
{
	return GameMode.IsValid() ? GameMode->GetMarble() : nullptr;
}

bool ACourseProp::MarbleWithin(const AMarblePawn* Marble, float Radius) const
{
	return FVector::DistSquared(Marble->GetActorLocation(), GetActorLocation()) <= FMath::Square(Radius);
}

bool ACourseProp::MarbleOnPad(const AMarblePawn* Marble, float Radius) const
{
	const FVector D = Marble->GetActorLocation() - GetActorLocation();
	return D.Size2D() <= Radius && D.Z > -30.f && D.Z < 170.f;  // a ball resting on a pad sits ~50 above it
}

void ACourseProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;
	Cooldown = FMath::Max(0.f, Cooldown - DeltaSeconds);
	Pulse = FMath::Max(0.f, Pulse - DeltaSeconds * 3.f);

	AMarblePawn* Marble = GetMarble();

	switch (Kind)
	{
	case ECoursePropKind::Gem:
	case ECoursePropKind::TimeCrystal:
	{
		if (bCollected)
		{
			// Pop: swell, rise and shrink away, then disappear.
			CollectTimer += DeltaSeconds;
			const float A = FMath::Clamp(CollectTimer / CollectDuration, 0.f, 1.f);
			Visual->SetRelativeScale3D(VisualBaseScale * (1.f + 0.9f * A) * (1.f - A));
			Visual->AddRelativeRotation(FRotator(0.f, 900.f * DeltaSeconds, 0.f));
			AddActorWorldOffset(FVector(0.f, 0.f, 380.f * DeltaSeconds));
			if (A >= 1.f)
			{
				Destroy();
			}
			break;
		}

		const float Bob = BobAmplitude * FMath::Sin(ElapsedTime * BobSpeed + Phase);
		Visual->SetRelativeLocation(VisualBaseLocation + FVector(0.0, 0.0, Bob));
		Visual->SetRelativeRotation(FRotator(VisualBaseRotation.Pitch, SpinRate * ElapsedTime + FMath::RadiansToDegrees(Phase), VisualBaseRotation.Roll));

		if (Marble != nullptr && GameMode.IsValid() && MarbleWithin(Marble, bGolden ? GemPickupRadius + 15.f : GemPickupRadius))
		{
			bCollected = true;
			if (Kind == ECoursePropKind::Gem)
			{
				GameMode->OnGemCollected(GetActorLocation(), bGolden);
			}
			else
			{
				GameMode->OnTimeCrystalCollected(GetActorLocation());
			}
		}
		break;
	}

	case ECoursePropKind::JumpPad:
		if (Marble != nullptr && GameMode.IsValid())
		{
			if (Cooldown <= 0.f && MarbleOnPad(Marble, 115.f))
			{
				Marble->LaunchTowards(PointA, FlightTime);
				GameMode->OnPadUsed(GetActorLocation());
				Cooldown = 0.8f;
				Pulse = 1.f;
			}
		}
		Visual->SetRelativeScale3D(VisualBaseScale * (1.f + 0.35f * Pulse));
		break;

	case ECoursePropKind::BoostPad:
		if (Marble != nullptr && GameMode.IsValid())
		{
			if (Cooldown <= 0.f && MarbleOnPad(Marble, 120.f))
			{
				Marble->BoostAlong(Direction, 1650.f);
				GameMode->OnBoostUsed(GetActorLocation());
				Cooldown = 0.5f;
				Pulse = 1.f;
			}
		}
		Visual->SetRelativeScale3D(VisualBaseScale * (1.f + 0.15f * Pulse));
		break;

	case ECoursePropKind::Spinner:
		AddActorLocalRotation(FRotator(0.f, SpinRate * DeltaSeconds, 0.f));
		if (Marble != nullptr)
		{
			TickSpinnerShove(Marble);
		}
		break;

	case ECoursePropKind::Mover:
	{
		const float Alpha = 0.5f - 0.5f * FMath::Cos(2.f * PI * ElapsedTime / FMath::Max(Period, 0.5f) + Phase);
		SetActorLocation(FMath::Lerp(PointA, PointB, static_cast<double>(Alpha)), false, nullptr, ETeleportType::None);
		break;
	}

	case ECoursePropKind::Goal:
	{
		const float Swell = 1.f + 0.06f * FMath::Sin(ElapsedTime * 3.5f);
		Visual->SetRelativeScale3D(VisualBaseScale * Swell);
		if (!bTriggered && Marble != nullptr && GameMode.IsValid() && MarbleWithin(Marble, 240.f))
		{
			bTriggered = true;
			GameMode->OnGoalReached();
		}
		break;
	}

	case ECoursePropKind::Decor:
		if (BobAmplitude > 0.f)
		{
			const float Bob = BobAmplitude * FMath::Sin(ElapsedTime * BobSpeed + Phase);
			Visual->SetRelativeLocation(VisualBaseLocation + FVector(0.0, 0.0, Bob));
		}
		break;

	default:
		break;
	}
}

void ACourseProp::TickSpinnerShove(AMarblePawn* Marble)
{
	if (Cooldown > 0.f)
	{
		return;
	}

	// Marble position in the bar's own space; the bar is a 440 x 38 x 50 box on the ground.
	const FVector Local = GetActorTransform().InverseTransformPosition(Marble->GetActorLocation());
	const FVector Closest(FMath::Clamp(Local.X, -220.0, 220.0), FMath::Clamp(Local.Y, -19.0, 19.0), FMath::Clamp(Local.Z, 0.0, 50.0));
	const FVector Away = Local - Closest;
	const float Reach = static_cast<float>(GRLayout::kMarbleRadius) + 10.f;

	// A ball rolling on top of the bar (centre ~100 up) is not shoved; one hit from the side is.
	if (Local.Z < 85.0 && Away.SizeSquared() < FMath::Square(Reach))
	{
		FVector Out = GetActorTransform().TransformVectorNoScale(FVector(Away.X, Away.Y, 0.0));
		Out = Out.GetSafeNormal();
		if (Out.IsNearlyZero())
		{
			Out = GetActorForwardVector().RotateAngleAxis(90.f, FVector::UpVector);
		}
		Marble->Knock(Out * 620.f + FVector(0.0, 0.0, 330.0));
		Cooldown = 0.35f;
		if (GameMode.IsValid())
		{
			GameMode->OnSpinnerHit(Marble->GetActorLocation());
		}
	}
}
