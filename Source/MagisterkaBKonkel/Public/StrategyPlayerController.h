// StrategyPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/HitResult.h"
#include "BaseUnit.h"
#include "GridManager.h"
#include "UnitManager.h"
#include "Net/UnrealNetwork.h"
#include "StrategyPlayerController.generated.h"

class UGameUI;
class AStrategyGameMode;

UCLASS(BlueprintType, Blueprintable)
class MAGISTERKABKONKEL_API AStrategyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AStrategyPlayerController();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

public:
    UFUNCTION(BlueprintCallable, Category = "Input")
    void OnLeftMouseClick();

    UFUNCTION(BlueprintCallable, Category = "Input")
    void OnRightMouseClick();

    UFUNCTION(BlueprintCallable, Category = "Input")
    void OnEscapePressed();

    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool GetGridPositionUnderCursor(FVector2D& OutGridPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool GetUnitUnderCursor(ABaseUnit*& OutUnit) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player")
    int32 GetPlayerID() const { return PlayerID; }

    UFUNCTION(BlueprintCallable, Category = "Player")
    void SetPlayerID(int32 NewPlayerID) { PlayerID = NewPlayerID; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game State")
    bool CanInteractWithUnits() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game State")
    bool IsDeploymentPhase() const;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void ToggleGridVisibility();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void ShowPlayerSpawnZones();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void CreateGameUI();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void UpdateUIGold(int32 NewGold);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
    UGameUI* GetGameUI() const { return GameUIRef; }

    UFUNCTION(Client, Reliable)
    void ClientInitializePlayer(int32 AssignedPlayerID, int32 StartingGold);

    UFUNCTION(Client, Reliable)
    void ClientShowUnitSelectionByPosition(FVector2D UnitGridPosition, int32 RequestingPlayerID);

    UFUNCTION(Client, Reliable)
    void ClientHideUnitSelection();

    UFUNCTION()
    void OnUnitPurchaseRequested(int32 UnitType);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerHandleGridClick(FVector2D GridPosition, int32 RequestingPlayerID);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerHandleUnitClick(ABaseUnit* ClickedUnit, int32 RequestingPlayerID);

    UFUNCTION(Server, Reliable)
    void ServerDeselectUnit();

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerRequestUnitPurchase(int32 UnitType, int32 RequestingPlayerID);



protected:
    bool PerformLineTrace(FHitResult& OutHit) const;
    FVector GetMouseWorldPosition() const;
    void InitializeReferences();

protected:
    UPROPERTY()
    AGridManager* GridManagerRef;

    UPROPERTY()
    AUnitManager* UnitManagerRef;

    UPROPERTY()
    AStrategyGameMode* GameModeRef;

    UPROPERTY()
    UGameUI* GameUIRef;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UGameUI> GameUIClass;

private:
    UPROPERTY(Replicated)
    int32 PlayerID;

    UPROPERTY(EditDefaultsOnly, Category = "Input Settings")
    float ClickRange;

    UPROPERTY(EditDefaultsOnly, Category = "Input Settings")
    bool bEnableDoubleClick;

    UPROPERTY(EditDefaultsOnly, Category = "Input Settings")
    float DoubleClickTime;

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    bool bShowDebugInfo;

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    bool bDrawDebugLines;

    float LastClickTime;
    FVector2D LastClickPosition;
    bool bInitialized;
};