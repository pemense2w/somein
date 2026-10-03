#include "GemRollerGameMode.h"

#include "Components/CapsuleComponent.h"
#include "CourseProp.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GemRollerHUD.h"
#include "GemSkyRig.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "MarblePawn.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"

namespace
{
	const FLinearColor ColCyan(0.20f, 0.90f, 1.00f);
	const FLinearColor ColPink(1.00f, 0.35f, 0.85f);
	const FLinearColor ColLime(0.60f, 1.00f, 0.30f);
	const FLinearColor ColGold(1.00f, 0.78f, 0.20f);
	const FLinearColor ColOrange(1.00f, 0.65f, 0.10f);
	const FLinearColor ColWhite(1.f, 1.f, 1.f);
	const FLinearColor ColGood(0.45f, 1.00f, 0.60f);
	const FLinearColor ColBad(1.00f, 0.40f, 0.35f);

	FVector ToVec(const GRLayout::V3& V)
	{
		return FVector(V.X, V.Y, V.Z);
	}

	uint32 MakeRandomSeed()
	{
		return static_cast<uint32>((FDateTime::UtcNow().GetTicks() / 10000) & 0x7FFFFFFF);
	}

	EGRMesh RockMesh(int32 Variant)
	{
		switch (Variant)
		{
		case 1:
			return EGRMesh::RockB;
		case 2:
			return EGRMesh::RockC;
		default:
			return EGRMesh::RockA;
		}
	}
}

AGemRollerGameMode::AGemRollerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AMarblePawn::StaticClass();
	HUDClass = AGemRollerHUD::StaticClass();
}

void AGemRollerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	GRAssets::ResetCache();

	const FString SeedOption = UGameplayStatics::ParseOption(Options, TEXT("Seed"));
	Seed = SeedOption.IsEmpty() ? MakeRandomSeed() : static_cast<uint32>(FCString::Atoi64(*SeedOption));
	NumIslands = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Islands"), NumIslands), 3, 20);

	int32 Best = 0;
	GConfig->GetInt(TEXT("GemRoller"), TEXT("BestScore"), Best, GGameUserSettingsIni);
	BestScore = Best;

	BuildCourse();
	ShowBanner(TEXT("GO!"), ColGood, 1.2f);
}

void AGemRollerGameMode::BuildCourse()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	Course = GRLayout::Generate(Seed, NumIslands);
	TimeLeft = static_cast<float>(Course.TimeBudget);
	GemsTotal = static_cast<int32>(Course.Gems.size());

	// A sun and sky, unless the level brings its own.
	bool bHasSun = false;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		bHasSun = true;
		break;
	}
	if (!bHasSun && SkyRig == nullptr)
	{
		SkyRig = World->SpawnActor<AGemSkyRig>(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	// ---- islands ------------------------------------------------------------------------------
	CheckpointProps.Reset();
	for (size_t I = 0; I < Course.Islands.size(); ++I)
	{
		const GRLayout::Island& Isl = Course.Islands[I];
		const FVector Center = ToVec(Isl.Center);
		const float Scale = static_cast<float>(Isl.Scale);

		SpawnProp(ECoursePropKind::Island, EGRMesh::Island, Center, static_cast<float>(Isl.Yaw), Scale, ColWhite);
		SpawnProp(ECoursePropKind::Decor, RockMesh(Isl.RockVariant), Center - FVector(0.0, 0.0, 50.0 * Isl.Scale),
		          static_cast<float>(Isl.Yaw), Scale, ColWhite);

		for (size_t T = 0; T < Isl.Trees.size(); ++T)
		{
			const float Yaw = static_cast<float>(FMath::Fmod(static_cast<double>(I) * 57.0 + static_cast<double>(T) * 131.0, 360.0));
			const float TreeScale = 0.8f + 0.1f * static_cast<float>((I + T) % 4);
			SpawnProp(ECoursePropKind::Decor, EGRMesh::Tree, ToVec(Isl.Trees[T]), Yaw, TreeScale, ColWhite);
		}

		ACourseProp* Pole = SpawnProp(ECoursePropKind::Checkpoint, EGRMesh::Checkpoint, ToVec(Isl.CheckpointPole),
		                              static_cast<float>(Isl.RespawnYaw), 1.f, ColWhite);
		CheckpointProps.Add(Pole);

		if (Isl.bSpinner)
		{
			if (ACourseProp* Spinner = SpawnProp(ECoursePropKind::Spinner, EGRMesh::Spinner, Center, 0.f, 1.f, ColWhite))
			{
				Spinner->SpinRate = static_cast<float>(Isl.SpinRate);
			}
		}
	}
	if (CheckpointProps.Num() > 0 && CheckpointProps[0] != nullptr)
	{
		CheckpointProps[0]->SetCheckpointActive(true);
	}

	// ---- links between islands ----------------------------------------------------------------
	for (const GRLayout::Link& L : Course.Links)
	{
		switch (L.Kind)
		{
		case GRLayout::LinkKind::JumpPad:
			if (ACourseProp* Pad = SpawnProp(ECoursePropKind::JumpPad, EGRMesh::JumpPad, ToVec(L.PadPos) + FVector(0.0, 0.0, 2.0), 0.f,
			                                 1.f, ColPink, 1.4f))
			{
				Pad->PointA = ToVec(L.Landing);
				Pad->FlightTime = static_cast<float>(L.FlightTime);
			}
			break;

		case GRLayout::LinkKind::StepStones:
			for (size_t S = 0; S < L.Stones.size(); ++S)
			{
				SpawnProp(ECoursePropKind::StepStone, EGRMesh::StepStone, ToVec(L.Stones[S]), static_cast<float>(S) * 40.f, 1.f, ColWhite);
			}
			break;

		case GRLayout::LinkKind::MovingPlatform:
			if (ACourseProp* Mover = SpawnProp(ECoursePropKind::Mover, EGRMesh::StepStone, ToVec(L.MoverA), 0.f,
			                                   static_cast<float>(GRLayout::kMoverScale), ColWhite))
			{
				Mover->PointA = ToVec(L.MoverA);
				Mover->PointB = ToVec(L.MoverB);
				Mover->Period = static_cast<float>(L.MoverPeriod);
				Mover->Phase = static_cast<float>(L.MoverPhase);
			}
			break;
		}
	}

	// ---- pickups, pads, goal ------------------------------------------------------------------
	const FLinearColor GemColors[3] = {ColCyan, ColPink, ColLime};
	for (size_t I = 0; I < Course.Gems.size(); ++I)
	{
		const GRLayout::GemSpot& G = Course.Gems[I];
		const FLinearColor Tint = G.bGolden ? ColGold : GemColors[I % 3];
		if (ACourseProp* Gem = SpawnProp(ECoursePropKind::Gem, EGRMesh::Gem, ToVec(G.Pos), 0.f, G.bGolden ? 1.5f : 1.f, Tint,
		                                 G.bGolden ? 3.f : 1.8f))
		{
			Gem->bGolden = G.bGolden;
			Gem->SpinRate = 110.f;
			Gem->BobAmplitude = 8.f;
			Gem->BobSpeed = 2.4f;
			Gem->Phase = static_cast<float>(I) * 0.7f;
		}
	}

	for (const GRLayout::V3& P : Course.TimeCrystals)
	{
		if (ACourseProp* Crystal = SpawnProp(ECoursePropKind::TimeCrystal, EGRMesh::TimeCrystal, ToVec(P), 0.f, 1.f, ColWhite, 1.f))
		{
			Crystal->SpinRate = 70.f;
			Crystal->BobAmplitude = 10.f;
			Crystal->BobSpeed = 2.f;
		}
	}

	for (const GRLayout::BoostPadSpot& B : Course.BoostPads)
	{
		if (ACourseProp* Boost = SpawnProp(ECoursePropKind::BoostPad, EGRMesh::BoostPad, ToVec(B.Pos), static_cast<float>(B.Yaw), 1.f,
		                                   ColOrange, 1.6f))
		{
			Boost->Direction = FRotator(0.0, B.Yaw, 0.0).Vector();
		}
	}

	SpawnProp(ECoursePropKind::Goal, EGRMesh::Goal, ToVec(Course.GoalPos), static_cast<float>(Course.GoalYaw), 1.f, ColWhite, 0.9f);

	// ---- scenery ------------------------------------------------------------------------------
	for (const GRLayout::BackdropRock& R : Course.Rocks)
	{
		if (ACourseProp* Rock = SpawnProp(ECoursePropKind::Decor, RockMesh(R.Variant), ToVec(R.Pos), static_cast<float>(R.Yaw),
		                                  static_cast<float>(R.Scale), ColWhite))
		{
			Rock->BobAmplitude = 25.f;
			Rock->BobSpeed = 0.6f;
			Rock->Phase = static_cast<float>(R.BobPhase);
		}
	}
	for (const GRLayout::CloudSpot& C : Course.Clouds)
	{
		SpawnProp(ECoursePropKind::Decor, C.Variant == 0 ? EGRMesh::CloudA : EGRMesh::CloudB, ToVec(C.Pos), static_cast<float>(C.Yaw),
		          static_cast<float>(C.Scale), ColWhite, 0.35f);
	}

	// ---- where the marble starts --------------------------------------------------------------
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerStart* Start = World->SpawnActor<APlayerStart>(ToVec(Course.Start), FRotator(0.0, Course.StartYaw, 0.0), Params);
	if (Start != nullptr)
	{
		Start->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // never bump the marble
		SpawnedActors.Add(Start);
	}
	PlayerStartActor = Start;
}

void AGemRollerGameMode::ClearCourse()
{
	for (TObjectPtr<AActor>& Actor : SpawnedActors)
	{
		if (Actor != nullptr)
		{
			Actor->Destroy();
		}
	}
	SpawnedActors.Reset();
	CheckpointProps.Reset();
	PlayerStartActor = nullptr;
}

void AGemRollerGameMode::ResetRunState()
{
	RunState = EGRRunState::Playing;
	Score = 0;
	bNewBest = false;
	Combo = 0;
	MaxComboReached = 0;
	GemsCollected = 0;
	Falls = 0;
	CurrentIsland = 0;
	TimeBonus = 0;
	ComboTimer = 0.f;
	RunTime = 0.f;
	StateTime = 0.f;
	BannerTimer = 0.f;
	Popups.Reset();
}

ACourseProp* AGemRollerGameMode::SpawnProp(ECoursePropKind Kind, EGRMesh Mesh, const FVector& Location, float YawDegrees, float Scale,
                                           const FLinearColor& Tint, float Emissive)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACourseProp* Prop = GetWorld()->SpawnActor<ACourseProp>(Location, FRotator(0.f, YawDegrees, 0.f), Params);
	if (Prop != nullptr)
	{
		Prop->SetActorScale3D(FVector(Scale));
		Prop->Initialize(this, Kind, Mesh, Tint, Emissive);
		SpawnedActors.Add(Prop);
	}
	return Prop;
}

AActor* AGemRollerGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	return PlayerStartActor != nullptr ? PlayerStartActor.Get() : Super::ChoosePlayerStart_Implementation(Player);
}

AMarblePawn* AGemRollerGameMode::GetMarble()
{
	if (!MarbleCache.IsValid())
	{
		MarbleCache = Cast<AMarblePawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	}
	return MarbleCache.Get();
}

void AGemRollerGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	StateTime += DeltaSeconds;
	BannerTimer = FMath::Max(0.f, BannerTimer - DeltaSeconds);

	for (int32 I = Popups.Num() - 1; I >= 0; --I)
	{
		Popups[I].Age += DeltaSeconds;
		if (Popups[I].Age >= Popups[I].Life)
		{
			Popups.RemoveAtSwap(I);
		}
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->WasInputKeyJustPressed(EKeys::R) || PC->WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))
		{
			RestartCourse(false);
			return;
		}
		if (PC->WasInputKeyJustPressed(EKeys::N) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top))
		{
			RestartCourse(true);
			return;
		}
	}

	AMarblePawn* Marble = GetMarble();
	if (Marble == nullptr || RunState != EGRRunState::Playing)
	{
		return;
	}

	RunTime += DeltaSeconds;
	TimeLeft -= DeltaSeconds;

	if (ComboTimer > 0.f)
	{
		ComboTimer -= DeltaSeconds;
		if (ComboTimer <= 0.f)
		{
			Combo = 0;
		}
	}

	const FVector P = Marble->GetActorLocation();

	// Reached a new island? That is the checkpoint.
	for (int32 I = CurrentIsland + 1; I < static_cast<int32>(Course.Islands.size()); ++I)
	{
		const GRLayout::Island& Isl = Course.Islands[I];
		const double Dx = P.X - Isl.Center.X;
		const double Dy = P.Y - Isl.Center.Y;
		const double Dz = P.Z - Isl.Center.Z;
		if (Dx * Dx + Dy * Dy < FMath::Square(Isl.Radius * 0.85) && Dz > 20.0 && Dz < 260.0)
		{
			ReachIsland(I);
		}
	}

	if (P.Z < Course.KillZ)
	{
		HandleFall();
	}

	if (RunState == EGRRunState::Playing && TimeLeft <= 0.f)
	{
		TimeLeft = 0.f;
		EndRun(EGRRunState::Lost);
	}
}

void AGemRollerGameMode::ReachIsland(int32 Index)
{
	CurrentIsland = Index;
	Score += 100;
	TimeLeft += IslandTimeBonus;

	if (CheckpointProps.IsValidIndex(Index) && CheckpointProps[Index] != nullptr)
	{
		CheckpointProps[Index]->SetCheckpointActive(true);
	}

	const int32 Total = static_cast<int32>(Course.Islands.size());
	if (Index == Total - 1)
	{
		ShowBanner(TEXT("FINAL ISLAND - roll through the ring!"), ColGold, 2.4f);
	}
	else
	{
		ShowBanner(FString::Printf(TEXT("ISLAND %d / %d   +%d s"), Index + 1, Total, FMath::RoundToInt(IslandTimeBonus)), ColGood);
	}
	GRAssets::PlaySfx(this, EGRSound::Checkpoint);
}

void AGemRollerGameMode::HandleFall()
{
	AMarblePawn* Marble = GetMarble();
	if (Marble == nullptr || Course.Islands.empty())
	{
		return;
	}

	++Falls;
	TimeLeft -= FallPenalty;
	Combo = 0;
	ComboTimer = 0.f;

	const int32 Index = FMath::Clamp(CurrentIsland, 0, static_cast<int32>(Course.Islands.size()) - 1);
	const GRLayout::Island& Isl = Course.Islands[Index];
	Marble->ResetAt(ToVec(Isl.Respawn), static_cast<float>(Isl.RespawnYaw));

	ShowBanner(FString::Printf(TEXT("OOPS!   -%d s"), FMath::RoundToInt(FallPenalty)), ColBad);
	GRAssets::PlaySfx(this, EGRSound::Fall);
}

void AGemRollerGameMode::OnGemCollected(const FVector& At, bool bGolden)
{
	if (RunState != EGRRunState::Playing)
	{
		return;
	}

	Combo = (ComboTimer > 0.f) ? FMath::Min(Combo + 1, MaxCombo) : 1;
	ComboTimer = ComboWindow;
	MaxComboReached = FMath::Max(MaxComboReached, Combo);

	const int32 Points = (bGolden ? GoldenGemValue : GemValue) * Combo;
	Score += Points;
	++GemsCollected;

	const FLinearColor Color = bGolden ? ColGold : (Combo >= 4 ? ColPink : ColCyan);
	AddPopup(At, FString::Printf(TEXT("+%d"), Points), Color);
	GRAssets::PlaySfx(this, bGolden ? EGRSound::Golden : EGRSound::Gem, 0.85f, 1.f + 0.06f * static_cast<float>(Combo - 1));
}

void AGemRollerGameMode::OnTimeCrystalCollected(const FVector& At)
{
	if (RunState != EGRRunState::Playing)
	{
		return;
	}
	TimeLeft += TimeCrystalBonus;
	Score += 50;
	AddPopup(At, FString::Printf(TEXT("+%d s"), FMath::RoundToInt(TimeCrystalBonus)), ColGood);
	GRAssets::PlaySfx(this, EGRSound::Time);
}

void AGemRollerGameMode::OnPadUsed(const FVector& At)
{
	GRAssets::PlaySfx(this, EGRSound::Pad);
}

void AGemRollerGameMode::OnBoostUsed(const FVector& At)
{
	GRAssets::PlaySfx(this, EGRSound::Boost);
}

void AGemRollerGameMode::OnSpinnerHit(const FVector& At)
{
	if (RunState != EGRRunState::Playing)
	{
		return;
	}
	Combo = 0;
	ComboTimer = 0.f;
	AddPopup(At, TEXT("BONK!"), ColBad);
	GRAssets::PlaySfx(this, EGRSound::Fall, 0.35f, 1.8f);
}

void AGemRollerGameMode::OnGoalReached()
{
	if (RunState != EGRRunState::Playing)
	{
		return;
	}
	TimeBonus = FMath::RoundToInt(FMath::Max(0.f, TimeLeft)) * 10;
	Score += 500 + TimeBonus;
	EndRun(EGRRunState::Won);
}

void AGemRollerGameMode::EndRun(EGRRunState NewState)
{
	RunState = NewState;
	StateTime = 0.f;

	if (NewState == EGRRunState::Lost)
	{
		if (AMarblePawn* Marble = GetMarble())
		{
			Marble->SetControlsEnabled(false);
		}
	}

	if (Score > BestScore)
	{
		BestScore = Score;
		bNewBest = true;
		SaveBest();
	}
	GRAssets::PlaySfx(this, NewState == EGRRunState::Won ? EGRSound::Win : EGRSound::Lose);
}

void AGemRollerGameMode::RestartCourse(bool bNewSeed)
{
	ClearCourse();
	ResetRunState();
	if (bNewSeed)
	{
		Seed = MakeRandomSeed();
	}
	GRAssets::ResetCache();
	BuildCourse();

	if (AMarblePawn* Marble = GetMarble())
	{
		Marble->SetControlsEnabled(true);
		Marble->ResetAt(ToVec(Course.Start), static_cast<float>(Course.StartYaw));
	}
	ShowBanner(TEXT("GO!"), ColGood, 1.2f);
}

FVector AGemRollerGameMode::GetNextTarget() const
{
	const int32 Next = CurrentIsland + 1;
	if (Next < static_cast<int32>(Course.Islands.size()))
	{
		const GRLayout::Island& Isl = Course.Islands[Next];
		return FVector(Isl.Center.X, Isl.Center.Y, Isl.Center.Z + 140.0);
	}
	return ToVec(Course.GoalPos);
}

void AGemRollerGameMode::ShowBanner(const FString& Text, const FLinearColor& Color, float Duration)
{
	BannerText = Text;
	BannerColor = Color;
	BannerTimer = Duration;
	BannerDuration = FMath::Max(Duration, 0.1f);
}

void AGemRollerGameMode::AddPopup(const FVector& At, const FString& Text, const FLinearColor& Color)
{
	FGRPopup Popup;
	Popup.WorldPos = At;
	Popup.Text = Text;
	Popup.Color = Color;
	Popup.Age = 0.f;
	Popup.Life = 0.9f;
	Popups.Add(Popup);
}

void AGemRollerGameMode::SaveBest() const
{
	GConfig->SetInt(TEXT("GemRoller"), TEXT("BestScore"), BestScore, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
