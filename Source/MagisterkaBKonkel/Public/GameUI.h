// GameUI.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Engine/Texture2D.h"
#include "GameUI.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUnitPurchased, int32, UnitType, int32, PlayerID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPhaseCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitPurchaseRequested, int32, UnitType);

USTRUCT(BlueprintType)
struct FUnitShopData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString UnitName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* UnitIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 UnitCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 UnitType; 

    FUnitShopData()
    {
        UnitName = TEXT("Unit");
        UnitIcon = nullptr;
        UnitCost = 10;
        UnitType = 0;
    }
};

UCLASS()
class MAGISTERKABKONKEL_API UGameUI : public UUserWidget
{
    GENERATED_BODY()

public:
    UGameUI(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Player")
    int32 GetPlayerIDFromController(APlayerController* Controller) const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUnitPurchased OnUnitPurchased;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPhaseCompleted OnPhaseCompleted;

    UPROPERTY(BlueprintAssignable, Category = "UI Events")
    FOnUnitPurchaseRequested OnUnitPurchaseRequested;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* GoldAmountText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TimerText;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* TimerProgressBar;

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* ChatBox;

    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* UnitShopContainer;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* PhaseNameText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* MatchResultText;

    UPROPERTY(meta = (BindWidget))
    UBorder* MatchResultBorder;

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* PlayersInfoContainer;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Player1NameText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Player1LivesText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Player2NameText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Player2LivesText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* WaitingPlayersText;

    UPROPERTY(meta = (BindWidget))
    UBorder* WaitingPlayersBorder;

    UPROPERTY(meta = (BindWidget))
    UButton* Unit1Button;

    UPROPERTY(meta = (BindWidget))
    UButton* Unit2Button;

    UPROPERTY(meta = (BindWidget))
    UButton* Unit3Button;

    UPROPERTY(meta = (BindWidget))
    UButton* Unit4Button;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Unit1Name;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Unit2Name;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Unit3Name;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Unit4Name;

    UPROPERTY(meta = (BindWidget))
    UImage* Unit1Icon;

    UPROPERTY(meta = (BindWidget))
    UImage* Unit2Icon;

    UPROPERTY(meta = (BindWidget))
    UImage* Unit3Icon;

    UPROPERTY(meta = (BindWidget))
    UImage* Unit4Icon;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Unit1Cost;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Unit2Cost;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Unit3Cost;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Unit4Cost;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void UpdateGold(int32 NewGoldAmount);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void UpdateTimer(float TimeRemaining, float MaxTime);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void AddChatMessage(const FString& Message, const FString& PlayerName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetupUnitShop(const TArray<FUnitShopData>& Units);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetUnitAvailability(int32 UnitIndex, bool bAvailable);

    UFUNCTION(BlueprintCallable, Category = "UI")
    bool CanAffordUnit(int32 UnitIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Player Info")
    void UpdatePlayerID(int32 NewPlayerID);

    UFUNCTION(BlueprintCallable, Category = "Player Info")
    int32 GetCurrentPlayerID() const { return CurrentPlayerID; }

    UFUNCTION(BlueprintCallable, Category = "UI")
    void RequestUnitPurchase(int32 UnitType);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void UpdatePhaseDisplay(const FString& PhaseName);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowMatchResult(const FString& ResultText, float DisplayDuration = 5.0f);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideMatchResult();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void UpdatePlayerInfo(int32 PlayerID, const FString& PlayerName, int32 Lives);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowWaitingForPlayers(int32 CurrentPlayers, int32 RequiredPlayers);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideWaitingForPlayers();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetShopEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void StartPhaseTimer(float TimeInSeconds);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void StopPhaseTimer();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shop")
    bool GetShopEnabled() const;

protected:
    UFUNCTION()
    void OnUnit1ButtonClicked();

    UFUNCTION()
    void OnUnit2ButtonClicked();

    UFUNCTION()
    void OnUnit3ButtonClicked();

    UFUNCTION()
    void OnUnit4ButtonClicked();

    UFUNCTION()
    void OnTimerExpired();

    UFUNCTION()
    void OnMatchResultTimerExpired();

    void HandleUnitPurchase(int32 UnitIndex);

private:
    UPROPERTY()
    int32 CurrentGold;

    UPROPERTY()
    float PhaseTimeRemaining;

    UPROPERTY()
    float MaxPhaseTime;

    UPROPERTY()
    TArray<FUnitShopData> UnitShopData;

    UPROPERTY()
    int32 CurrentPlayerID;

    FTimerHandle PhaseTimerHandle;
    FTimerHandle MatchResultTimerHandle;

    bool bTimerActive;
    bool bShopEnabled;
};