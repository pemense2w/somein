#include "MarblePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GRAssets.h"
#include "GRLayout.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CoyoteTime = 0.12f;
	constexpr float JumpBufferTime = 0.14f;
}

AMarblePawn::AMarblePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// Physics body: a plain sphere, so rolling never depends on how a mesh was imported.
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(static_cast<float>(GRLayout::kMarbleRadius));
	Sphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	Sphere->SetSimulatePhysics(true);
	Sphere->SetLinearDamping(0.05f);
	Sphere->SetAngularDamping(0.6f);
	Sphere->BodyInstance.bUseCCD = true;
	Sphere->BodyInstance.MaxAngularVelocity = 5000.f;
	Sphere->SetMassOverrideInKg(NAME_None, 80.f, true);
	Sphere->SetCanEverAffectNavigation(false);
	SetRootComponent(Sphere);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Sphere);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		PlaceholderSphere = SphereFinder.Object;
		Visual->SetStaticMesh(SphereFinder.Object);
	}

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Sphere);
	SpringArm->SetUsingAbsoluteRotation(true);  // the camera orbits; it must not roll with the ball
	SpringArm->TargetArmLength = 650.f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void AMarblePawn::BeginPlay()
{
	Super::BeginPlay();

	CamYaw = GetActorRotation().Yaw;

	UStaticMesh* Imported = GRAssets::FindMesh(EGRMesh::Marble);
	if (Imported != nullptr)
	{
		Visual->SetStaticMesh(Imported);
	}
	else if (PlaceholderSphere != nullptr)
	{
		Visual->SetStaticMesh(PlaceholderSphere);
	}
	Visual->SetRelativeScale3D(FVector(1.0, 1.0, 1.0));

	// The imported mesh carries its own beach-ball colours (white tint); the placeholder needs one.
	const FLinearColor Tint = (Imported != nullptr) ? FLinearColor::White : GRAssets::GetFallback(EGRMesh::Marble).Color;
	if (UMaterialInstanceDynamic* Mat = GRAssets::MakeMaterial(this, Tint, 0.f, 0.35f, 0.f))
	{
		for (int32 Slot = 0; Slot < Visual->GetNumMaterials(); ++Slot)
		{
			Visual->SetMaterial(Slot, Mat);
		}
	}
}

void AMarblePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateGrounded(DeltaSeconds);
	Sphere->WakeRigidBody();  // never let the ball fall asleep on a moving platform

	JumpCooldown = FMath::Max(0.f, JumpCooldown - DeltaSeconds);
	DashTimer = FMath::Max(0.f, DashTimer - DeltaSeconds);
	JumpBuffer = FMath::Max(0.f, JumpBuffer - DeltaSeconds);
	LaunchLock = FMath::Max(0.f, LaunchLock - DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC == nullptr)
	{
		return;
	}

	// ---- camera orbit -------------------------------------------------------------------------
	float MouseX = 0.f;
	float MouseY = 0.f;
	PC->GetInputMouseDelta(MouseX, MouseY);
	float KeyYaw = 0.f;
	if (PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::Right))
	{
		KeyYaw += 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::Q) || PC->IsInputKeyDown(EKeys::Left))
	{
		KeyYaw -= 1.f;
	}
	const float StickX = PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX);
	const float StickY = PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY);
	CamYaw += MouseX * MouseSensitivity + (StickX + KeyYaw * 0.85f) * StickLookSpeed * DeltaSeconds;
	CamPitch += (bInvertY ? -1.f : 1.f) * (MouseY * MouseSensitivity + StickY * StickLookSpeed * DeltaSeconds);
	CamPitch = FMath::Clamp(CamPitch, -80.f, -6.f);
	SpringArm->SetWorldRotation(FRotator(CamPitch, CamYaw, 0.f));

	// ---- rolling ------------------------------------------------------------------------------
	const float YawRad = FMath::DegreesToRadians(CamYaw);
	const FVector CamForward(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.f);
	const FVector CamRight(-FMath::Sin(YawRad), FMath::Cos(YawRad), 0.f);

	float Forward = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	float Right = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	if (PC->IsInputKeyDown(EKeys::W)) { Forward += 1.f; }
	if (PC->IsInputKeyDown(EKeys::S)) { Forward -= 1.f; }
	if (PC->IsInputKeyDown(EKeys::D)) { Right += 1.f; }
	if (PC->IsInputKeyDown(EKeys::A)) { Right -= 1.f; }

	FVector Wish = CamForward * Forward + CamRight * Right;
	if (Wish.SizeSquared() > 1.f)
	{
		Wish.Normalize();
	}
	if (!bControlsEnabled)
	{
		Wish = FVector::ZeroVector;
	}

	const FVector Velocity = Sphere->GetPhysicsLinearVelocity();
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);

	if (!Wish.IsNearlyZero(0.05))
	{
		const float Along = FVector::DotProduct(Horizontal, Wish.GetSafeNormal());
		if (Along < MaxSpeed)
		{
			const float Control = bGrounded ? 1.f : AirControl;
			Sphere->AddForce(Wish * (MoveAccel * Control), NAME_None, true);
		}
	}
	else if (bGrounded && LaunchLock <= 0.f)
	{
		// No input: roll to a stop instead of coasting forever.
		const float Speed = Horizontal.Size();
		if (Speed > 5.f)
		{
			const float Brake = FMath::Min(BrakeAccel, Speed / FMath::Max(DeltaSeconds, 0.001f));
			Sphere->AddForce(-Horizontal / Speed * Brake, NAME_None, true);
		}
	}

	// ---- jump (with a little coyote time and input buffering) -------------------------------
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
	{
		JumpBuffer = JumpBufferTime;
	}
	if (bControlsEnabled && JumpBuffer > 0.f && CoyoteTimer > 0.f && JumpCooldown <= 0.f)
	{
		FVector V = Sphere->GetPhysicsLinearVelocity();
		V.Z = JumpSpeed;
		Sphere->SetPhysicsLinearVelocity(V);
		JumpBuffer = 0.f;
		CoyoteTimer = 0.f;
		JumpCooldown = 0.25f;
		bGrounded = false;
		GRAssets::PlaySfx(this, EGRSound::Jump, 0.6f);
	}

	// ---- dash ---------------------------------------------------------------------------------
	const bool bDashPressed = PC->WasInputKeyJustPressed(EKeys::LeftShift) || PC->WasInputKeyJustPressed(EKeys::RightShift) ||
	                          PC->WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left);
	if (bControlsEnabled && bDashPressed && DashTimer <= 0.f)
	{
		FVector Dir = Wish.GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			Dir = Horizontal.GetSafeNormal();
		}
		if (Dir.IsNearlyZero())
		{
			Dir = CamForward;
		}
		Sphere->AddImpulse(Dir * DashSpeed, NAME_None, true);
		DashTimer = DashCooldown;
		GRAssets::PlaySfx(this, EGRSound::Dash, 0.7f);
	}
}

void AMarblePawn::UpdateGrounded(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.0, 0.0, GRLayout::kMarbleRadius + 14.0);
	FHitResult Hit;
	FCollisionQueryParams Params(NAME_None, false, this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	bGrounded = bHit && Hit.ImpactNormal.Z > 0.45f;

	if (bGrounded)
	{
		CoyoteTimer = CoyoteTime;
	}
	else
	{
		CoyoteTimer = FMath::Max(0.f, CoyoteTimer - DeltaSeconds);
	}
}

void AMarblePawn::ResetAt(const FVector& Location, float YawDegrees)
{
	Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	SetActorLocationAndRotation(Location, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	CamYaw = YawDegrees;
	CamPitch = -28.f;
	CoyoteTimer = 0.f;
	JumpBuffer = 0.f;
	LaunchLock = 0.f;
}

void AMarblePawn::LaunchTowards(const FVector& Target, float FlightTime)
{
	const float T = FMath::Max(FlightTime, 0.1f);
	const float Gravity = FMath::Max(1.f, -GetWorld()->GetGravityZ());
	const FVector From = GetActorLocation();
	FVector V = (Target - From) / T;
	V.Z += 0.5f * Gravity * T;
	Sphere->SetPhysicsLinearVelocity(V);
	bGrounded = false;
	CoyoteTimer = 0.f;
	JumpCooldown = 0.4f;
	LaunchLock = T;
}

void AMarblePawn::BoostAlong(const FVector& Direction, float Speed)
{
	FVector V = Sphere->GetPhysicsLinearVelocity();
	const FVector Dir = FVector(Direction.X, Direction.Y, 0.0).GetSafeNormal();
	V.X = Dir.X * Speed;
	V.Y = Dir.Y * Speed;
	Sphere->SetPhysicsLinearVelocity(V);
}

void AMarblePawn::Knock(const FVector& VelocityChange)
{
	Sphere->AddImpulse(VelocityChange, NAME_None, true);
}

FVector AMarblePawn::GetBallVelocity() const
{
	return Sphere->GetPhysicsLinearVelocity();
}

float AMarblePawn::GetDashReadiness() const
{
	return DashCooldown > KINDA_SMALL_NUMBER ? 1.f - FMath::Clamp(DashTimer / DashCooldown, 0.f, 1.f) : 1.f;
}
