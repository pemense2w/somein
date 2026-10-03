#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MarblePawn.generated.h"

class UCameraComponent;
class USphereComponent;
class USpringArmComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The player's marble.
 *
 * The root is a simulating sphere (so rolling, bouncing and friction are real physics); the
 * Blender mesh is a purely visual child. Input is read straight from the player controller each
 * frame: WASD / left stick to roll, mouse / right stick / Q E to orbit, Space to jump, Shift to dash.
 */
UCLASS()
class GEMROLLER_API AMarblePawn : public APawn
{
	GENERATED_BODY()

public:
	AMarblePawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Teleport back to a respawn point, standing still, with the camera behind the marble. */
	void ResetAt(const FVector& Location, float YawDegrees);

	/** Jump pad: fly on a parabola that ends at Target after FlightTime seconds. */
	void LaunchTowards(const FVector& Target, float FlightTime);

	/** Boost pad: set horizontal speed along Direction (keeps vertical speed). */
	void BoostAlong(const FVector& Direction, float Speed);

	/** Instant velocity change (spinner shove). */
	void Knock(const FVector& VelocityChange);

	FVector GetBallVelocity() const;
	bool IsGrounded() const { return bGrounded; }

	/** 0 = just dashed, 1 = ready again. */
	float GetDashReadiness() const;

	void SetControlsEnabled(bool bEnabled) { bControlsEnabled = bEnabled; }

	// --- tuning (UU = centimetres) -------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Marble|Movement")
	float MoveAccel = 2300.f;

	UPROPERTY(EditAnywhere, Category = "Marble|Movement")
	float MaxSpeed = 1150.f;

	UPROPERTY(EditAnywhere, Category = "Marble|Movement")
	float AirControl = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Marble|Movement")
	float BrakeAccel = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Marble|Movement")
	float JumpSpeed = 720.f;

	UPROPERTY(EditAnywhere, Category = "Marble|Movement")
	float DashSpeed = 950.f;

	UPROPERTY(EditAnywhere, Category = "Marble|Movement")
	float DashCooldown = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Marble|Camera")
	float MouseSensitivity = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Marble|Camera")
	float StickLookSpeed = 140.f;

	UPROPERTY(EditAnywhere, Category = "Marble|Camera")
	bool bInvertY = false;

private:
	void UpdateGrounded(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaceholderSphere;

	bool bControlsEnabled = true;
	bool bGrounded = false;

	float CamYaw = 0.f;
	float CamPitch = -28.f;

	float CoyoteTimer = 0.f;
	float JumpBuffer = 0.f;
	float JumpCooldown = 0.f;
	float DashTimer = 0.f;
	float LaunchLock = 0.f;  // brief period after a pad launch with no ground-brake
};
