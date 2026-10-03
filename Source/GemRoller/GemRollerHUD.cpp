#include "GemRollerHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GRAssets.h"
#include "GameFramework/PlayerController.h"
#include "GemRollerGameMode.h"
#include "MarblePawn.h"

namespace
{
	const FLinearColor Cream(1.0f, 0.97f, 0.88f);
	const FLinearColor Dim(0.80f, 0.86f, 0.95f, 0.85f);
	const FLinearColor Gold(1.0f, 0.80f, 0.22f);
	const FLinearColor Red(1.0f, 0.38f, 0.33f);
	const FLinearColor Mint(0.45f, 1.0f, 0.62f);
	const FLinearColor Panel(0.04f, 0.06f, 0.14f, 0.55f);
}

FString AGemRollerHUD::FormatTime(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
}

void AGemRollerHUD::Label(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, EAnchor Anchor, bool bLarge)
{
	UFont* Font = bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, Scale);

	float DrawX = X;
	if (Anchor == EAnchor::Center)
	{
		DrawX = X - Width * 0.5f;
	}
	else if (Anchor == EAnchor::Right)
	{
		DrawX = X - Width;
	}

	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.65f), DrawX + 2.f * Scale, Y + 2.f * Scale, Font, Scale);
	DrawText(Text, Color, DrawX, Y, Font, Scale);
}

void AGemRollerHUD::Bar(float X, float Y, float Width, float Height, float Fill, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 2.f, Y - 2.f, Width + 4.f, Height + 4.f);
	DrawRect(Color, X, Y, Width * FMath::Clamp(Fill, 0.f, 1.f), Height);
}

void AGemRollerHUD::DrawHUD()
{
	Super::DrawHUD();

	const AGemRollerGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AGemRollerGameMode>() : nullptr;
	if (GM == nullptr || Canvas == nullptr || GEngine == nullptr)
	{
		return;
	}

	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float S = FMath::Clamp(H / 1080.f, 0.6f, 2.0f);
	const float Margin = 36.f * S;
	APlayerController* PC = GetOwningPlayerController();

	// ---- top-left: score ---------------------------------------------------------------------
	Label(TEXT("SCORE"), Margin, Margin, Dim, 0.55f * S, EAnchor::Left, false);
	Label(FString::Printf(TEXT("%d"), GM->Score), Margin, Margin + 22.f * S, Cream, 1.25f * S, EAnchor::Left);
	Label(FString::Printf(TEXT("BEST %d"), GM->BestScore), Margin, Margin + 82.f * S, Dim, 0.55f * S, EAnchor::Left, false);

	// ---- top-centre: time + progress ------------------------------------------------------------
	const bool bLow = GM->TimeLeft < 10.f && GM->RunState == EGRRunState::Playing;
	const float Throb = bLow ? 0.5f + 0.5f * FMath::Sin(GM->RunTime * 10.f) : 0.f;
	const FLinearColor TimeColor = bLow ? FMath::Lerp(Cream, Red, Throb) : Cream;
	Label(TEXT("TIME"), W * 0.5f, Margin, Dim, 0.55f * S, EAnchor::Center, false);
	Label(FormatTime(GM->TimeLeft), W * 0.5f, Margin + 22.f * S, TimeColor, (1.25f + 0.2f * Throb) * S, EAnchor::Center);
	const int32 TotalIslands = static_cast<int32>(GM->Course.Islands.size());
	Label(FString::Printf(TEXT("ISLAND %d / %d"), GM->CurrentIsland + 1, TotalIslands), W * 0.5f, Margin + 82.f * S, Dim, 0.55f * S,
	      EAnchor::Center, false);

	// ---- top-right: gems + combo ------------------------------------------------------------------
	Label(TEXT("GEMS"), W - Margin, Margin, Dim, 0.55f * S, EAnchor::Right, false);
	Label(FString::Printf(TEXT("%d / %d"), GM->GemsCollected, GM->GemsTotal), W - Margin, Margin + 22.f * S, Cream, 1.25f * S, EAnchor::Right);
	if (GM->Combo > 1)
	{
		const FLinearColor ComboColor = GM->Combo >= 4 ? FLinearColor(1.0f, 0.45f, 0.9f) : FLinearColor(0.3f, 0.92f, 1.0f);
		Label(FString::Printf(TEXT("COMBO x%d"), GM->Combo), W - Margin, Margin + 76.f * S, ComboColor, 0.9f * S, EAnchor::Right);
		Bar(W - Margin - 220.f * S, Margin + 116.f * S, 220.f * S, 8.f * S, GM->ComboTimer / FMath::Max(GM->ComboWindow, 0.1f), ComboColor);
	}

	// ---- bottom-centre: dash meter -------------------------------------------------------------------
	if (const AMarblePawn* Marble = GetWorld()->GetFirstPlayerController() ? Cast<AMarblePawn>(GetWorld()->GetFirstPlayerController()->GetPawn()) : nullptr)
	{
		const float Ready = Marble->GetDashReadiness();
		const bool bReady = Ready >= 1.f;
		Label(bReady ? TEXT("DASH READY  [SHIFT]") : TEXT("DASH"), W * 0.5f, H - Margin - 44.f * S, bReady ? Mint : Dim, 0.55f * S,
		      EAnchor::Center, false);
		Bar(W * 0.5f - 130.f * S, H - Margin - 14.f * S, 260.f * S, 9.f * S, Ready, bReady ? Mint : FLinearColor(0.35f, 0.6f, 1.f));
	}

	// ---- floating "+30" popups ----------------------------------------------------------------------
	if (PC != nullptr)
	{
		for (const FGRPopup& P : GM->Popups)
		{
			FVector2D Screen;
			if (PC->ProjectWorldLocationToScreen(P.WorldPos + FVector(0.0, 0.0, 40.0), Screen))
			{
				const float T = P.Age / FMath::Max(P.Life, 0.1f);
				FLinearColor Color = P.Color;
				Color.A = 1.f - T * T;
				Label(P.Text, Screen.X, Screen.Y - 70.f * S * T, Color, (0.8f + 0.4f * (1.f - T)) * S, EAnchor::Center);
			}
		}

		// ---- objective marker: where to go next ---------------------------------------------------
		if (GM->RunState == EGRRunState::Playing)
		{
			const FVector Target = GM->GetNextTarget();
			FVector2D Screen;
			if (PC->ProjectWorldLocationToScreen(Target, Screen) && Screen.X > 40.f && Screen.X < W - 40.f && Screen.Y > 140.f * S &&
			    Screen.Y < H - 80.f)
			{
				const AMarblePawn* Pawn = Cast<AMarblePawn>(PC->GetPawn());
				const float Meters = Pawn ? FVector::Dist(Pawn->GetActorLocation(), Target) / 100.f : 0.f;
				const FLinearColor C = Gold;
				const float Bob = 6.f * S * FMath::Sin(GM->RunTime * 4.f);
				DrawRect(C, Screen.X - 7.f * S, Screen.Y - 46.f * S + Bob, 14.f * S, 14.f * S);
				DrawRect(C, Screen.X - 3.f * S, Screen.Y - 34.f * S + Bob, 6.f * S, 10.f * S);
				Label(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Meters)), Screen.X, Screen.Y - 78.f * S + Bob, C, 0.5f * S,
				      EAnchor::Center, false);
			}
		}
	}

	// ---- banner (checkpoint, oops, go) ---------------------------------------------------------------
	if (GM->BannerTimer > 0.f && GM->RunState == EGRRunState::Playing)
	{
		const float Fade = FMath::Clamp(GM->BannerTimer / FMath::Min(0.5f, GM->BannerDuration), 0.f, 1.f);
		const float Pop = 1.f + 0.25f * FMath::Clamp((GM->BannerDuration - GM->BannerTimer) * -6.f + 1.f, 0.f, 1.f);
		FLinearColor C = GM->BannerColor;
		C.A = Fade;
		Label(GM->BannerText, W * 0.5f, H * 0.2f, C, 1.1f * S * Pop, EAnchor::Center);
	}

	// ---- how to play (first seconds) -------------------------------------------------------------
	if (GM->RunTime < 14.f && GM->RunState == EGRRunState::Playing)
	{
		FLinearColor C = Cream;
		C.A = FMath::Clamp((14.f - GM->RunTime) / 2.f, 0.f, 1.f);
		const float X = Margin;
		const float Y = H - Margin - 120.f * S;
		DrawRect(FLinearColor(Panel.R, Panel.G, Panel.B, Panel.A * C.A), X - 14.f * S, Y - 10.f * S, 640.f * S, 130.f * S);
		Label(TEXT("WASD roll     SPACE jump     SHIFT dash"), X, Y, C, 0.6f * S, EAnchor::Left, false);
		Label(TEXT("MOUSE / Q E look     R retry     N new course"), X, Y + 34.f * S, C, 0.6f * S, EAnchor::Left, false);
		Label(TEXT("Grab gems fast to build a combo. Roll through the golden ring!"), X, Y + 76.f * S, C, 0.55f * S, EAnchor::Left, false);
	}

	// ---- small print --------------------------------------------------------------------------------
	Label(FString::Printf(TEXT("seed %u"), GM->Seed), W - Margin, H - Margin - 14.f * S, Dim, 0.45f * S, EAnchor::Right, false);
	if (GRAssets::IsUsingPlaceholders())
	{
		Label(TEXT("Placeholder shapes: run Tools/ImportAssets.py to load the Blender art"), W - Margin, H - Margin - 38.f * S,
		      FLinearColor(1.f, 0.7f, 0.3f, 0.9f), 0.45f * S, EAnchor::Right, false);
	}

	if (GM->RunState != EGRRunState::Playing)
	{
		DrawEndScreen(GM, S);
	}
}

void AGemRollerHUD::DrawEndScreen(const AGemRollerGameMode* GM, float S)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Fade = FMath::Clamp(GM->StateTime / 0.6f, 0.f, 1.f);
	const bool bWon = GM->RunState == EGRRunState::Won;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.08f, 0.62f * Fade), 0.f, 0.f, W, H);

	FLinearColor TitleColor = bWon ? Gold : Red;
	TitleColor.A = Fade;
	FLinearColor Text = Cream;
	Text.A = Fade;
	FLinearColor Soft = Dim;
	Soft.A = Fade;

	const float CenterX = W * 0.5f;
	float Y = H * 0.22f;
	Label(bWon ? TEXT("COURSE COMPLETE!") : TEXT("TIME'S UP!"), CenterX, Y, TitleColor, 1.8f * S, EAnchor::Center);
	Y += 120.f * S;

	Label(FString::Printf(TEXT("SCORE  %d"), GM->Score), CenterX, Y, Text, 1.3f * S, EAnchor::Center);
	Y += 80.f * S;
	if (GM->bNewBest)
	{
		FLinearColor C = Mint;
		C.A = Fade;
		Label(TEXT("NEW BEST!"), CenterX, Y, C, 0.9f * S, EAnchor::Center);
	}
	else
	{
		Label(FString::Printf(TEXT("best %d"), GM->BestScore), CenterX, Y, Soft, 0.7f * S, EAnchor::Center, false);
	}
	Y += 70.f * S;

	Label(FString::Printf(TEXT("gems %d / %d      best combo x%d      falls %d"), GM->GemsCollected, GM->GemsTotal, GM->MaxComboReached, GM->Falls),
	      CenterX, Y, Soft, 0.7f * S, EAnchor::Center, false);
	Y += 44.f * S;
	if (bWon)
	{
		Label(FString::Printf(TEXT("finish bonus 500  +  time bonus %d"), GM->TimeBonus), CenterX, Y, Soft, 0.7f * S, EAnchor::Center, false);
		Y += 44.f * S;
	}
	else
	{
		Label(FString::Printf(TEXT("you reached island %d of %d"), GM->CurrentIsland + 1, static_cast<int32>(GM->Course.Islands.size())), CenterX, Y,
		      Soft, 0.7f * S, EAnchor::Center, false);
		Y += 44.f * S;
	}

	const float Pulse = 0.65f + 0.35f * FMath::Sin(GM->StateTime * 4.f);
	FLinearColor Prompt = Cream;
	Prompt.A = Fade * Pulse;
	Label(TEXT("R  retry this course        N  new course"), CenterX, H * 0.78f, Prompt, 0.8f * S, EAnchor::Center);
}
