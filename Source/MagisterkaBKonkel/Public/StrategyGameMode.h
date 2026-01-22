#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/Engine.h"
#include "GameUI.h"
#include "BaseUnit.h"
#include "Net/UnrealNetwork.h"
#include "StrategyGameMode.generated.h"

class AUnitManager;
class AStrategyPlayerController;

UENUM(BlueprintType)
enum class EGamePhase : uint8
{
    WaitingForPlayers   UMETA(DisplayName = "Waiting For Players"),
    Purchase            UMETA(DisplayName = "Purchase"),
    Battle              UMETA(DisplayName = "Battle"),
    Results             UMETA(DisplayName = "Results"),
    GameOver            UMETA(DisplayName = "Game Over")
};

UENUM(BlueprintType)
enum class EMatchResult : uint8
{
    None                UMETA(DisplayName = "None"),
    Player1Wins         UMETA(DisplayName = "Player 1 Wins"),
    Player2Wins         UMETA(DisplayName = "Player 2 Wins"),
    Draw                UMETA(DisplayName = "Draw")
};

USTRUCT(BlueprintType)
struct FPlayerGameData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 PlayerID;

    UPROPERTY(BlueprintReadWrite)
    int32 Gold;

    UPROPERTY(BlueprintReadWrite)
    int32 Lives;

    UPROPERTY(BlueprintReadWrite)
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite)
    TArray<int32> PurchasedUnits;

    UPROPERTY(BlueprintReadWrite)
    class APlayerController* PlayerController;

    UPROPERTY(BlueprintReadWrite)
    bool bReadyForNextPhase;

    FPlayerGameData()
        : PlayerID(-1), Gold(1000), Lives(3), PlayerName(TEXT("")),
        PlayerController(nullptr), bReadyForNextPhase(false)
    {
    }
};

UCLASS()
class MAGISTERKABKONKEL_API AStrategyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AStrategyGameMode();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    FPlayerGameData* GetPlayerData(int32 PlayerID);
    const FPlayerGameData* FindPlayerDataConst(int32 PlayerID) const;
    int32 GetPlayerIDFromController(APlayerController* Controller) const;
    void SetPlayerReadyForNextPhase(int32 PlayerID, bool bReady);
    void RemovePlayerFromArray(int32 PlayerID);
    void HandlePlayerLogout(int32 PlayerID);
    void UpdatePlayerUI(int32 PlayerID);
    void UpdateAllPlayersUI();
    void BroadcastToAllPlayers(const FString& Message, const FString& SenderName = TEXT("System"));
    void UpdatePlayerGoldUI(int32 PlayerID, int32 NewGold);
    void UpdatePlayerDataArray(int32 PlayerID, int32 Gold, bool bReady);
    void SyncPlayerDataToClients();

    void RemovePlayerLife(int32 PlayerID);
    bool IsPlayerEliminated(int32 PlayerID) const;
    int32 GetPlayerLives(int32 PlayerID) const;
    void ResetPlayerLives();

    void StartNextPhase();
    void InitializeWaitingPhase();
    void InitializePurchasePhase();
    void InitializeBattlePhase();
    void InitializeResultsPhase();
    void InitializeGameOverPhase();
    void OnPhaseTimerExpired();
    void CheckForBattleEnd();
    void ProcessBattleResults();
    void SetAllPlayersReady();
    void CheckAllPlayersReady();
    void ResetPlayerReadyStatus();
    void ResetPlayerPhaseData();
    FString GetPhaseText(EGamePhase Phase) const;

    int32 GetPlayerGold(int32 PlayerID) const;
    bool SpendPlayerGold(int32 PlayerID, int32 Amount);
    void AddPlayerGold(int32 PlayerID, int32 Amount);
    void OnUnitPurchased(int32 UnitType, int32 PlayerID);
    void RegisterUnitPurchase(int32 PlayerID, int32 UnitType);
    TArray<int32> GetPlayerPurchasedUnits(int32 PlayerID) const;
    int32 GetUnitCost(int32 UnitType) const;

    void InitializeUnitManager();
    void BindUnitManagerEvents();
    void OnUnitSpawned(ABaseUnit* SpawnedUnit, int32 PlayerID, FVector2D GridPosition);
    void ClearAllUnitsFromBattlefield();
    void OnUnitMoved(ABaseUnit* MovedUnit, FVector2D OldPosition, FVector2D NewPosition, int32 PlayerID);
    AUnitManager* GetUnitManager() const { return UnitManagerRef; }

    EMatchResult DetermineBattleWinner();
    void ShowMatchResult(EMatchResult Result);
    int32 CountPlayerUnits(int32 PlayerID) const;

    void CreateUIForPlayer(APlayerController* PlayerController, int32 PlayerID);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerRequestUnitPurchase(int32 UnitType, int32 PlayerID);
    void ServerRequestUnitPurchase_Implementation(int32 UnitType, int32 PlayerID);
    bool ServerRequestUnitPurchase_Validate(int32 UnitType, int32 PlayerID);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayerJoined(int32 PlayerID, const FString& PlayerName);
    void MulticastPlayerJoined_Implementation(int32 PlayerID, const FString& PlayerName);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayerLeft(int32 PlayerID);
    void MulticastPlayerLeft_Implementation(int32 PlayerID);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUpdatePlayerGold(int32 PlayerID, int32 NewGold);
    void MulticastUpdatePlayerGold_Implementation(int32 PlayerID, int32 NewGold);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUpdatePlayerLives(int32 PlayerID, int32 NewLives);
    void MulticastUpdatePlayerLives_Implementation(int32 PlayerID, int32 NewLives);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitSpawned(int32 UnitType, int32 PlayerID, FVector2D GridPosition);
    void MulticastUnitSpawned_Implementation(int32 UnitType, int32 PlayerID, FVector2D GridPosition);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPhaseChanged(EGamePhase NewPhase, float PhaseTime);
    void MulticastPhaseChanged_Implementation(EGamePhase NewPhase, float PhaseTime);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastShowMatchResult(EMatchResult Result);
    void MulticastShowMatchResult_Implementation(EMatchResult Result);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUpdatePlayersInfo();
    void MulticastUpdatePlayersInfo_Implementation();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastShowWaitingForPlayers(int32 CurrentCount, int32 RequiredCount);
    void MulticastShowWaitingForPlayers_Implementation(int32 CurrentCount, int32 RequiredCount);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastSyncPlayerData(int32 PlayerID, const FString& PlayerName, int32 Gold, int32 Lives);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPrepareUnitClearing();
    void MulticastPrepareUnitClearing_Implementation();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastClearSpecificUnits(const TArray<FVector2D>& UnitPositions);
    void MulticastClearSpecificUnits_Implementation(const TArray<FVector2D>& UnitPositions);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastAllUnitsCleared();
    void MulticastAllUnitsCleared_Implementation();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Phase")
    EGamePhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Info")
    bool HasEnoughPlayers() const { return PlayerDataArray.Num() >= RequiredPlayersCount; }

protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    UFUNCTION()
    void DelayedGameStart();

    bool AreAllPlayersReady() const;
    void SchedulePhaseTransition();

private:
    UPROPERTY(Replicated)
    EGamePhase CurrentPhase;

    UPROPERTY()
    TArray<FPlayerGameData> PlayerDataArray;

    UPROPERTY()
    int32 NextPlayerID;

    UPROPERTY()
    AUnitManager* UnitManagerRef;

    UPROPERTY()
    bool bFirstUpdatePlayerInfo;

    UPROPERTY(EditDefaultsOnly, Category = "Game Settings")
    float PurchasePhaseTime;

    UPROPERTY(EditDefaultsOnly, Category = "Game Settings")
    float BattlePhaseTime;

    UPROPERTY(EditDefaultsOnly, Category = "Game Settings")
    float ResultsPhaseTime;

    UPROPERTY(EditDefaultsOnly, Category = "Game Settings")
    int32 StartingGold;

    UPROPERTY(EditDefaultsOnly, Category = "Game Settings")
    int32 StartingLives;

    UPROPERTY(EditDefaultsOnly, Category = "Game Settings")
    int32 RequiredPlayersCount;

    UPROPERTY(EditDefaultsOnly, Category = "Game Settings")
    int32 MaxPlayersPerGame;

    FTimerHandle DelayedStartTimer;
    FTimerHandle PhaseTimer;
    FTimerHandle BattleEndCheckTimer;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UGameUI> GameUIClass;
};