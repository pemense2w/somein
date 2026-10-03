#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GemRollerHUD.generated.h"

class AGemRollerGameMode;

/** Score, timer, combo, dash meter, objective marker and the end screens, all drawn on the canvas. */
UCLASS()
class GEMROLLER_API AGemRollerHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	enum class EAnchor : uint8
	{
		Left,
		Center,
		Right
	};

	void Label(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, EAnchor Anchor, bool bLarge = true);
	void Bar(float X, float Y, float Width, float Height, float Fill, const FLinearColor& Color);
	void DrawEndScreen(const AGemRollerGameMode* GM, float S);
	static FString FormatTime(float Seconds);
};
