// Gem Roller - where the Blender-made assets are looked up.
//
// Everything is found by path at runtime. If a mesh has not been imported yet the game keeps
// working with an engine primitive that roughly matches it, so you can press Play right after
// the first compile and import the real art afterwards (Tools/ImportAssets.py).
#pragma once

#include "CoreMinimal.h"

class UObject;
class UStaticMesh;
class UMaterialInstanceDynamic;

enum class EGRMesh : uint8
{
	Marble,
	Island,
	StepStone,
	Gem,
	TimeCrystal,
	Spinner,
	JumpPad,
	BoostPad,
	Checkpoint,
	Goal,
	RockA,
	RockB,
	RockC,
	Tree,
	CloudA,
	CloudB
};

enum class EGRShape : uint8
{
	Sphere,
	Cylinder,
	Cube
};

enum class EGRSound : uint8
{
	Gem,
	Golden,
	Time,
	Jump,
	Pad,
	Boost,
	Dash,
	Checkpoint,
	Fall,
	Win,
	Lose
};

/** Stand-in for a Blender mesh that is not imported yet: an engine primitive, scaled to fit. */
struct FGRFallback
{
	EGRShape Shape = EGRShape::Sphere;
	FVector Scale = FVector(1.0, 1.0, 1.0);
	float ZOffset = 0.f;
	FLinearColor Color = FLinearColor::White;
};

namespace GRAssets
{
	/** Forget which lookups failed (call when a level starts, so freshly imported assets are seen). */
	void ResetCache();

	/** The imported Blender mesh, or nullptr if it has not been imported. */
	UStaticMesh* FindMesh(EGRMesh Which);

	FGRFallback GetFallback(EGRMesh Which);

	/** True if any lookup since ResetCache() fell back to a placeholder. */
	bool IsUsingPlaceholders();

	/** Instance of M_GR_Master (vertex colour x Tint, plus Emissive/Roughness/Metallic). May return nullptr. */
	UMaterialInstanceDynamic* MakeMaterial(UObject* Outer, const FLinearColor& Tint, float Emissive, float Roughness = 0.5f, float Metallic = 0.f);

	/** Fire-and-forget 2D sound; silently does nothing if the sound has not been imported. */
	void PlaySfx(const UObject* WorldContext, EGRSound Which, float Volume = 1.f, float Pitch = 1.f);
}
