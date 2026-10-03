#include "GRAssets.h"

#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	TSet<FString> GMissing;
	bool GUsedPlaceholder = false;

	const TCHAR* MeshPath(EGRMesh Which)
	{
		switch (Which)
		{
		case EGRMesh::Marble:      return TEXT("/Game/GemRoller/Meshes/SM_Marble.SM_Marble");
		case EGRMesh::Island:      return TEXT("/Game/GemRoller/Meshes/SM_Island.SM_Island");
		case EGRMesh::StepStone:   return TEXT("/Game/GemRoller/Meshes/SM_StepStone.SM_StepStone");
		case EGRMesh::Gem:         return TEXT("/Game/GemRoller/Meshes/SM_Gem.SM_Gem");
		case EGRMesh::TimeCrystal: return TEXT("/Game/GemRoller/Meshes/SM_TimeCrystal.SM_TimeCrystal");
		case EGRMesh::Spinner:     return TEXT("/Game/GemRoller/Meshes/SM_Spinner.SM_Spinner");
		case EGRMesh::JumpPad:     return TEXT("/Game/GemRoller/Meshes/SM_JumpPad.SM_JumpPad");
		case EGRMesh::BoostPad:    return TEXT("/Game/GemRoller/Meshes/SM_BoostPad.SM_BoostPad");
		case EGRMesh::Checkpoint:  return TEXT("/Game/GemRoller/Meshes/SM_Checkpoint.SM_Checkpoint");
		case EGRMesh::Goal:        return TEXT("/Game/GemRoller/Meshes/SM_Goal.SM_Goal");
		case EGRMesh::RockA:       return TEXT("/Game/GemRoller/Meshes/SM_Rock_A.SM_Rock_A");
		case EGRMesh::RockB:       return TEXT("/Game/GemRoller/Meshes/SM_Rock_B.SM_Rock_B");
		case EGRMesh::RockC:       return TEXT("/Game/GemRoller/Meshes/SM_Rock_C.SM_Rock_C");
		case EGRMesh::Tree:        return TEXT("/Game/GemRoller/Meshes/SM_Tree.SM_Tree");
		case EGRMesh::CloudA:      return TEXT("/Game/GemRoller/Meshes/SM_Cloud_A.SM_Cloud_A");
		case EGRMesh::CloudB:      return TEXT("/Game/GemRoller/Meshes/SM_Cloud_B.SM_Cloud_B");
		}
		return TEXT("");
	}

	const TCHAR* SoundPath(EGRSound Which)
	{
		switch (Which)
		{
		case EGRSound::Gem:        return TEXT("/Game/GemRoller/Audio/SW_Gem.SW_Gem");
		case EGRSound::Golden:     return TEXT("/Game/GemRoller/Audio/SW_Golden.SW_Golden");
		case EGRSound::Time:       return TEXT("/Game/GemRoller/Audio/SW_Time.SW_Time");
		case EGRSound::Jump:       return TEXT("/Game/GemRoller/Audio/SW_Jump.SW_Jump");
		case EGRSound::Pad:        return TEXT("/Game/GemRoller/Audio/SW_Pad.SW_Pad");
		case EGRSound::Boost:      return TEXT("/Game/GemRoller/Audio/SW_Boost.SW_Boost");
		case EGRSound::Dash:       return TEXT("/Game/GemRoller/Audio/SW_Dash.SW_Dash");
		case EGRSound::Checkpoint: return TEXT("/Game/GemRoller/Audio/SW_Checkpoint.SW_Checkpoint");
		case EGRSound::Fall:       return TEXT("/Game/GemRoller/Audio/SW_Fall.SW_Fall");
		case EGRSound::Win:        return TEXT("/Game/GemRoller/Audio/SW_Win.SW_Win");
		case EGRSound::Lose:       return TEXT("/Game/GemRoller/Audio/SW_Lose.SW_Lose");
		}
		return TEXT("");
	}

	template <typename T>
	T* LoadOrNull(const TCHAR* Path)
	{
		const FString Key(Path);
		if (GMissing.Contains(Key))
		{
			return nullptr;
		}
		T* Obj = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn);
		if (Obj == nullptr)
		{
			GMissing.Add(Key);
		}
		return Obj;
	}
}

namespace GRAssets
{
	void ResetCache()
	{
		GMissing.Reset();
		GUsedPlaceholder = false;
	}

	UStaticMesh* FindMesh(EGRMesh Which)
	{
		UStaticMesh* Mesh = LoadOrNull<UStaticMesh>(MeshPath(Which));
		if (Mesh == nullptr)
		{
			GUsedPlaceholder = true;
		}
		return Mesh;
	}

	bool IsUsingPlaceholders()
	{
		return GUsedPlaceholder;
	}

	FGRFallback GetFallback(EGRMesh Which)
	{
		FGRFallback F;
		switch (Which)
		{
		case EGRMesh::Marble:
			F.Shape = EGRShape::Sphere;
			F.Scale = FVector(1.0, 1.0, 1.0);
			F.Color = FLinearColor(1.0f, 0.36f, 0.34f);
			break;
		case EGRMesh::Island:
			F.Shape = EGRShape::Cylinder;
			F.Scale = FVector(7.0, 7.0, 0.6);
			F.ZOffset = -30.f;
			F.Color = FLinearColor(0.2f, 0.65f, 0.2f);
			break;
		case EGRMesh::StepStone:
			F.Shape = EGRShape::Cylinder;
			F.Scale = FVector(2.4, 2.4, 0.5);
			F.ZOffset = -25.f;
			F.Color = FLinearColor(0.1f, 0.6f, 0.55f);
			break;
		case EGRMesh::Gem:
			F.Shape = EGRShape::Sphere;
			F.Scale = FVector(0.5, 0.5, 0.7);
			break;
		case EGRMesh::TimeCrystal:
			F.Shape = EGRShape::Cylinder;
			F.Scale = FVector(0.5, 0.5, 1.0);
			F.Color = FLinearColor(0.6f, 0.9f, 1.0f);
			break;
		case EGRMesh::Spinner:
			F.Shape = EGRShape::Cube;
			F.Scale = FVector(4.4, 0.38, 0.5);
			F.ZOffset = 25.f;
			F.Color = FLinearColor(1.0f, 0.55f, 0.0f);
			break;
		case EGRMesh::JumpPad:
			F.Shape = EGRShape::Cylinder;
			F.Scale = FVector(2.0, 2.0, 0.12);
			F.ZOffset = 6.f;
			break;
		case EGRMesh::BoostPad:
			F.Shape = EGRShape::Cube;
			F.Scale = FVector(2.0, 1.4, 0.1);
			F.ZOffset = 5.f;
			break;
		case EGRMesh::Checkpoint:
			F.Shape = EGRShape::Cylinder;
			F.Scale = FVector(0.12, 0.12, 2.5);
			F.ZOffset = 125.f;
			F.Color = FLinearColor(0.9f, 0.9f, 0.9f);
			break;
		case EGRMesh::Goal:
			F.Shape = EGRShape::Sphere;
			F.Scale = FVector(3.0, 3.0, 3.0);
			F.Color = FLinearColor(1.0f, 0.8f, 0.2f);
			break;
		case EGRMesh::RockA:
		case EGRMesh::RockB:
		case EGRMesh::RockC:
			F.Shape = EGRShape::Sphere;
			F.Scale = FVector(6.2, 6.2, 6.3);
			F.ZOffset = -315.f;
			F.Color = FLinearColor(0.35f, 0.27f, 0.22f);
			break;
		case EGRMesh::Tree:
			F.Shape = EGRShape::Cylinder;
			F.Scale = FVector(0.8, 0.8, 3.0);
			F.ZOffset = 150.f;
			F.Color = FLinearColor(0.1f, 0.5f, 0.15f);
			break;
		case EGRMesh::CloudA:
		case EGRMesh::CloudB:
			F.Shape = EGRShape::Sphere;
			F.Scale = FVector(15.0, 8.0, 4.0);
			break;
		}
		return F;
	}

	UMaterialInstanceDynamic* MakeMaterial(UObject* Outer, const FLinearColor& Tint, float Emissive, float Roughness, float Metallic)
	{
		UMaterialInterface* Parent = LoadOrNull<UMaterialInterface>(TEXT("/Game/GemRoller/Materials/M_GR_Master.M_GR_Master"));
		if (Parent == nullptr)
		{
			// Not imported yet: the engine's basic-shape material has a "Color" parameter.
			Parent = LoadOrNull<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		if (Parent == nullptr)
		{
			return nullptr;
		}

		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, Outer);
		if (Mid != nullptr)
		{
			Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
			Mid->SetVectorParameterValue(TEXT("Color"), Tint);
			Mid->SetScalarParameterValue(TEXT("Emissive"), Emissive);
			Mid->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			Mid->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		}
		return Mid;
	}

	void PlaySfx(const UObject* WorldContext, EGRSound Which, float Volume, float Pitch)
	{
		if (USoundBase* Sound = LoadOrNull<USoundBase>(SoundPath(Which)))
		{
			UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume, Pitch);
		}
	}
}
