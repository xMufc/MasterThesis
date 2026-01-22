// UnitManager.h - Enhanced with Spatial Partitioning Combat System
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseUnit.h" 
#include "GridManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UnitManager.generated.h"

class ABaseUnit;
class AGridManager;
class AStrategyPlayerController;
class USpatialGrid; 

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnUnitSpawned, ABaseUnit*, SpawnedUnit, int32, PlayerID, FVector2D, GridPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnUnitMoved, ABaseUnit*, MovedUnit, FVector2D, OldPosition, FVector2D, NewPosition, int32, PlayerID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUnitSelected, ABaseUnit*, SelectedUnit, int32, PlayerID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitDeselected, ABaseUnit*, DeselectedUnit);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatStarted, int32, PlayerCount, float, CombatUpdateInterval);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnUnitAttacked, ABaseUnit*, Attacker, ABaseUnit*, Target, int32, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitKilled, ABaseUnit*, DeadUnit);

USTRUCT(BlueprintType)
struct FSpawnedUnitData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    ABaseUnit* Unit;

    UPROPERTY(BlueprintReadWrite)
    int32 PlayerID;

    UPROPERTY(BlueprintReadWrite)
    FVector2D GridPosition;

    UPROPERTY(BlueprintReadWrite)
    EBaseUnitType UnitType; 

    FSpawnedUnitData()
    {
        Unit = nullptr;
        PlayerID = -1;
        GridPosition = FVector2D::ZeroVector;
        UnitType = EBaseUnitType::Tank; 
    }
};

UCLASS(BlueprintType, Blueprintable)
class MAGISTERKABKONKEL_API AUnitManager : public AActor
{
    GENERATED_BODY()
    friend class AStrategyPlayerController;
public:
    AUnitManager();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Spatial Partitioning")
    void InitializeSpatialGrid();

    UFUNCTION(BlueprintCallable, Category = "Spatial Partitioning")
    void ToggleSpatialPartitioning();

    UFUNCTION(BlueprintCallable, Category = "Spatial Partitioning")
    void DebugSpatialGrid();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Partitioning")
    bool IsSpatialPartitioningEnabled() const { return bUseSpatialPartitioning; }

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StartCombatPhase();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StopCombatPhase();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void UpdateCombatUsingSpatialGrid();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ProcessSpatialCombat(const TArray<ABaseUnit*>& AliveUnits);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ProcessUnitCombatWithSpatialGrid(ABaseUnit* Unit);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void UpdateAllUnitsCombat();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EnableAutoCombatForAllUnits();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void DisableAutoCombatForAllUnits();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    TArray<ABaseUnit*> GetAllUnits() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    TArray<ABaseUnit*> GetAliveUnits() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void UpdateSpatialGridPositions();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void PopulateSpatialGridWithAllUnits();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ClearSpatialGrid();

    UFUNCTION(BlueprintCallable, Category = "Data Locality Combat")
    void InitializeCombatDataLocality();

    UFUNCTION(BlueprintCallable, Category = "Data Locality Combat")
    void AddUnitToCombatArray(ABaseUnit* Unit);

    UFUNCTION(BlueprintCallable, Category = "Data Locality Combat")
    void RemoveUnitFromCombatArray(ABaseUnit* Unit);

    UFUNCTION(BlueprintCallable, Category = "Data Locality Combat")
    void UpdateCombatArrayOrder();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Data Locality Combat")
    int32 GetMaxCombatUnits() const { return MaxCombatUnits; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Data Locality Combat")
    int32 GetActiveCombatUnitsCount() const { return ActiveCombatUnitsCount; }

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void MonitorCombatEvents();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void HandleUnitDeath(ABaseUnit* DeadUnit);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void CheckCombatEndConditions();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    bool IsCombatActive() const { return bCombatPhaseActive; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Queries")
    int32 GetAliveUnitCount(int32 PlayerID) const;

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    ABaseUnit* SpawnUnitForPlayer(int32 PlayerID, EBaseUnitType UnitType); 

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Partitioning")
    USpatialGrid* GetSpatialGrid() const { return SpatialGrid; }

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    bool MoveUnitToPosition(ABaseUnit* Unit, FVector2D NewGridPosition, int32 PlayerID);

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    bool CanMoveUnitToPosition(ABaseUnit* Unit, FVector2D NewGridPosition, int32 PlayerID) const;

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    void ForceDeselectAllUnits();

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    void ClearAllUnits();

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    void PrepareForUnitClearing();

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    void FinalizeUnitClearing();

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    void DestroyUnitsAtPositions(const TArray<FVector2D>& Positions);

    UFUNCTION(BlueprintCallable, Category = "Unit Selection")
    void SelectUnit(ABaseUnit* Unit, int32 PlayerID);

    UFUNCTION(BlueprintCallable, Category = "Unit Selection")
    void DeselectUnit();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Selection")
    ABaseUnit* GetSelectedUnit() const { return SelectedUnit; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Selection")
    int32 GetSelectedUnitPlayerID() const { return SelectedUnitPlayerID; }

    UFUNCTION(BlueprintCallable, Category = "Grid Interaction")
    void HandleGridClick(FVector2D GridPosition, int32 PlayerID);

    UFUNCTION(BlueprintCallable, Category = "Grid Interaction")
    void HandleUnitClick(ABaseUnit* ClickedUnit, int32 PlayerID);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Queries")
    FVector2D FindFirstFreeSpawnPosition(int32 PlayerID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Queries")
    bool IsPositionOccupied(FVector2D GridPosition) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Queries")
    ABaseUnit* GetUnitAtPosition(FVector2D GridPosition) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Queries")
    TArray<ABaseUnit*> GetPlayerUnits(int32 PlayerID) const;

    UFUNCTION(BlueprintCallable, Category = "Unit Management")
    void RemoveUnit(ABaseUnit* Unit);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Queries")
    int32 GetUnitCount(int32 PlayerID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Queries")
    int32 GetUnitCountByType(int32 PlayerID, EBaseUnitType UnitType) const; 

    UFUNCTION(BlueprintCallable, Category = "Visual Feedback")
    void ShowValidMovePositions(ABaseUnit* Unit, int32 PlayerID);

    UFUNCTION(BlueprintCallable, Category = "Visual Feedback")
    void HideAllHighlights();

    UFUNCTION(BlueprintCallable, Category = "Visual Feedback")
    void ShowSelectionHighlight(ABaseUnit* Unit);

    UFUNCTION(BlueprintCallable, Category = "Visual Feedback")
    void HighlightGridPosition(FVector2D GridPosition, UMaterialInterface* Material);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitSpawned(ABaseUnit* SpawnedUnit, int32 PlayerID, FVector2D GridPosition, EBaseUnitType UnitType);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitMoved(ABaseUnit* MovedUnit, FVector2D OldPosition, FVector2D NewPosition, int32 PlayerID);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitSelected(ABaseUnit* Unit, int32 PlayerID);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitDeselected();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitRemoved(ABaseUnit* Unit);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitMovedByPosition(FVector2D OldPosition, FVector2D NewPosition, int32 PlayerID);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastAllUnitsCleared();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastClearAllUnits();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastCombatStarted(int32 PlayerCount, float UpdateInterval);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastCombatStopped();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitAttacked(ABaseUnit* Attacker, ABaseUnit* Target, int32 Damage);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastUnitKilled(ABaseUnit* DeadUnit);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastCombatUpdate(const TArray<ABaseUnit*>& CombatUnits);

    UPROPERTY(BlueprintAssignable, Category = "Unit Events")
    FOnUnitSpawned OnUnitSpawned;

    UPROPERTY(BlueprintAssignable, Category = "Unit Events")
    FOnUnitMoved OnUnitMoved;

    UPROPERTY(BlueprintAssignable, Category = "Unit Events")
    FOnUnitSelected OnUnitSelected;

    UPROPERTY(BlueprintAssignable, Category = "Unit Events")
    FOnUnitDeselected OnUnitDeselected;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnCombatStarted OnCombatStarted;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnCombatStopped OnCombatStopped;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnUnitAttacked OnUnitAttacked;

    UPROPERTY(BlueprintAssignable, Category = "Combat Events")
    FOnUnitKilled OnUnitKilled;

protected:
    UPROPERTY()
    AGridManager* GridManagerRef;

    UPROPERTY()
    USpatialGrid* SpatialGrid;

    UPROPERTY(BlueprintReadOnly, Category = "Unit Management")
    TArray<FSpawnedUnitData> SpawnedUnits;

    UPROPERTY(BlueprintReadOnly, Replicated, Category = "Unit Selection")
    ABaseUnit* SelectedUnit;

    UPROPERTY(BlueprintReadOnly, Replicated, Category = "Unit Selection")
    int32 SelectedUnitPlayerID;

    UPROPERTY(BlueprintReadOnly, Replicated, Category = "Combat State")
    bool bCombatPhaseActive;

    UPROPERTY(BlueprintReadOnly, Replicated, Category = "Combat State")
    float CombatUpdateInterval;

    UPROPERTY(BlueprintReadOnly, Category = "Combat State")
    int32 TotalCombatUnits;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Partitioning")
    bool bUseSpatialPartitioning;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Partitioning", meta = (EditCondition = "bUseSpatialPartitioning"))
    float SpatialGridCellSize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Partitioning", meta = (EditCondition = "bUseSpatialPartitioning"))
    FVector2D SpatialGridWorldMin;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Partitioning", meta = (EditCondition = "bUseSpatialPartitioning"))
    FVector2D SpatialGridWorldMax;

    UPROPERTY(BlueprintReadOnly, Category = "Data Locality Combat")
    TArray<ABaseUnit*> CombatUnitsArray;

    UPROPERTY(BlueprintReadOnly, Category = "Data Locality Combat")
    int32 MaxCombatUnits;

    UPROPERTY(BlueprintReadOnly, Category = "Data Locality Combat")
    int32 ActiveCombatUnitsCount;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit Classes")
    TSubclassOf<ABaseUnit> TankUnitClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit Classes")
    TSubclassOf<ABaseUnit> NinjaUnitClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit Classes")
    TSubclassOf<ABaseUnit> SwordUnitClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit Classes")
    TSubclassOf<ABaseUnit> ArmorUnitClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Feedback")
    UMaterialInterface* ValidMoveHighlightMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Feedback")
    UMaterialInterface* InvalidMoveHighlightMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Feedback")
    UMaterialInterface* SelectionHighlightMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Feedback")
    UStaticMesh* HighlightPlaneMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Feedback")
    float HighlightSizeMultiplier = 0.5f;

    UPROPERTY()
    TArray<UStaticMeshComponent*> HighlightComponents;

private:

    void InitializeGridManager();
    void RetryInitializeGridManager();
    void DelayedGridManagerInit();

    bool IsValidPlayerSpawnPosition(FVector2D GridPosition, int32 PlayerID) const;
    TSubclassOf<ABaseUnit> GetUnitClassByType(EBaseUnitType UnitType) const;
    FVector GetWorldLocationFromGrid(FVector2D GridPosition) const;

    FSpawnedUnitData* FindUnitData(ABaseUnit* Unit);
    bool DoesUnitBelongToPlayer(ABaseUnit* Unit, int32 PlayerID) const;
    void RemoveUnitFromArray(ABaseUnit* Unit);
    void RemoveUnitFromClientArray(ABaseUnit* Unit);
    void UpdateUnitDataPosition(ABaseUnit* Unit, FVector2D NewPosition);

    void CreateHighlightComponent(FVector2D GridPosition, UMaterialInterface* Material);
    void ClearHighlightComponents();

    void BindUnitCombatEvents(ABaseUnit* Unit);
    void UnbindUnitCombatEvents(ABaseUnit* Unit);
    void OnUnitDeathEvent(ABaseUnit* DeadUnit);
    void OnUnitDamagedEvent(ABaseUnit* DamagedUnit, int32 Damage);
    void DiagnoseNetworkIssues();

    bool bInitialized;

    FTimerHandle InitializeGridManagerHandle;
    FTimerHandle RetryInitializeGridManagerHandle;
    FTimerHandle CombatUpdateTimer;
    FTimerHandle CombatMonitorTimer;
};