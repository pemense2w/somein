#pragma once

#include "CoreMinimal.h"
#include "GRAssets.h"
#include "GRLayout.h"
#include "GameFramework/GameModeBase.h"
#include "GemRollerGameMode.generated.h"

class ACourseProp;
class AGemSkyRig;
class AMarblePawn;
enum class ECoursePropKind : uint8;

enum class EGRRunState : uint8
{
	Playing,
	Won,
	Lost
};

/** "+30" style text that floats up from where a gem was collected. */
struct FGRPopup
{
	FVector WorldPos = FVector::ZeroVector;
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Age = 0.f;
	float Life = 1.f;
};

/**
 * Runs a Gem Roller game: turns a seed into a course (GRLayout), spawns it, and keeps score.
 *
 * Works on any level, even an empty one. Add "?Seed=1234" to the URL to start on a given course.
 * In game, R replays the same course and N rolls a new one; both rebuild in place, no level reload.
 */
UCLASS()
class GEMROLLER_API AGemRollerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGemRollerGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	AMarblePawn* GetMarble();

	// Events raised by props.
	void OnGemCollected(const FVector& At, bool bGolden);
	void OnTimeCrystalCollected(const FVector& At);
	void OnPadUsed(const FVector& At);
	void OnBoostUsed(const FVector& At);
	void OnSpinnerHit(const FVector& At);
	void OnGoalReached();

	/** Throw the course away and start over: same seed = same course, new seed = a fresh one. */
	void RestartCourse(bool bNewSeed);

	/** World position the HUD arrow should point at: the next island, or the goal. */
	FVector GetNextTarget() const;

	// ---- state, read by the HUD -------------------------------------------------------------------
	EGRRunState RunState = EGRRunState::Playing;
	GRLayout::Course Course;
	uint32 Seed = 0;

	int32 Score = 0;
	int32 BestScore = 0;
	bool bNewBest = false;
	int32 Combo = 0;
	int32 MaxComboReached = 0;
	int32 GemsCollected = 0;
	int32 GemsTotal = 0;
	int32 Falls = 0;
	int32 CurrentIsland = 0;
	int32 TimeBonus = 0;

	float TimeLeft = 120.f;
	float ComboTimer = 0.f;
	float RunTime = 0.f;
	float StateTime = 0.f;

	FString BannerText;
	FLinearColor BannerColor = FLinearColor::White;
	float BannerTimer = 0.f;
	float BannerDuration = 1.6f;

	TArray<FGRPopup> Popups;

	// ---- tuning --------------------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Gem Roller", meta = (ClampMin = "3", ClampMax = "20"))
	int32 NumIslands = 9;

	UPROPERTY(EditDefaultsOnly, Category = "Gem Roller")
	int32 GemValue = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Gem Roller")
	int32 GoldenGemValue = 50;

	UPROPERTY(EditDefaultsOnly, Category = "Gem Roller")
	int32 MaxCombo = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Gem Roller")
	float ComboWindow = 2.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Gem Roller")
	float FallPenalty = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Gem Roller")
	float TimeCrystalBonus = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "Gem Roller")
	float IslandTimeBonus = 3.f;

private:
	void BuildCourse();
	void ClearCourse();
	void ResetRunState();
	ACourseProp* SpawnProp(ECoursePropKind Kind, EGRMesh Mesh, const FVector& Location, float YawDegrees, float Scale,
	                       const FLinearColor& Tint, float Emissive = 0.f);
	void ReachIsland(int32 Index);
	void HandleFall();
	void EndRun(EGRRunState NewState);
	void ShowBanner(const FString& Text, const FLinearColor& Color, float Duration = 1.6f);
	void AddPopup(const FVector& At, const FString& Text, const FLinearColor& Color);
	void SaveBest() const;

	UPROPERTY()
	TObjectPtr<AActor> PlayerStartActor;

	/** Sun + sky; created once and kept across restarts so the lighting never flashes. */
	UPROPERTY()
	TObjectPtr<AGemSkyRig> SkyRig;

	/** Everything else BuildCourse() spawned, so RestartCourse() can clean it up. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY()
	TArray<TObjectPtr<ACourseProp>> CheckpointProps;

	TWeakObjectPtr<AMarblePawn> MarbleCache;
};
