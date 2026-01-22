// SpatialGrid.h - Hierarchical Spatial Partitioning System for Combat Units
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BaseUnit.h" 
#include "SpatialGrid.generated.h"

class ABaseUnit;

USTRUCT(BlueprintType)
struct FSpatialCell
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<ABaseUnit*> Units;

    FVector2D MinBounds;
    FVector2D MaxBounds;

    int32 MegaCellX;
    int32 MegaCellY;

    int32 BaseGridStartX;
    int32 BaseGridStartY;
    int32 BaseGridEndX;
    int32 BaseGridEndY;

    FSpatialCell()
    {
        MinBounds = FVector2D::ZeroVector;
        MaxBounds = FVector2D::ZeroVector;
        MegaCellX = 0;
        MegaCellY = 0;
        BaseGridStartX = 0;
        BaseGridStartY = 0;
        BaseGridEndX = 0;
        BaseGridEndY = 0;
    }

    void AddUnit(ABaseUnit* Unit)
    {
        if (Unit && !Units.Contains(Unit))
        {
            Units.Add(Unit);
        }
    }

    void RemoveUnit(ABaseUnit* Unit)
    {
        Units.Remove(Unit);
    }

    bool ContainsUnit(ABaseUnit* Unit) const
    {
        return Units.Contains(Unit);
    }

    int32 GetUnitCount() const
    {
        return Units.Num();
    }

    bool IsEmpty() const
    {
        return Units.Num() == 0;
    }

    bool ContainsBaseGridCell(int32 BaseX, int32 BaseY) const
    {
        return BaseX >= BaseGridStartX && BaseX <= BaseGridEndX &&
            BaseY >= BaseGridStartY && BaseY <= BaseGridEndY;
    }
};

UCLASS(BlueprintType)
class MAGISTERKABKONKEL_API USpatialGrid : public UObject
{
    GENERATED_BODY()

public:
    USpatialGrid();

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    void InitializeGrid(FVector2D WorldMin, FVector2D WorldMax, float InCellSize = 200.0f);

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    void InitializeGridFromBaseGrid(FVector2D WorldMin, int32 BaseGridWidth, int32 BaseGridHeight, float BaseGridCellSize = 200.0f);

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    void AddUnit(ABaseUnit* Unit);

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    void RemoveUnit(ABaseUnit* Unit);

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    void UpdateUnitPosition(ABaseUnit* Unit, FVector OldPosition, FVector NewPosition);

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    TArray<ABaseUnit*> GetUnitsInRange(FVector Position, float Range) const;

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    TArray<ABaseUnit*> GetUnitsInMegaCell(int32 MegaCellX, int32 MegaCellY) const;

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    TArray<ABaseUnit*> GetUnitsInBaseGridCell(int32 BaseGridX, int32 BaseGridY) const;

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    TArray<ABaseUnit*> GetNearbyEnemies(ABaseUnit* Unit, float SearchRange) const;

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    ABaseUnit* FindNearestEnemy(ABaseUnit* Unit, float MaxRange = 2000.0f) const;

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    void HandleCombatInMegaCell(int32 MegaCellX, int32 MegaCellY);

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid")
    void HandleAllCombat();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Grid")
    FVector2D GetMegaCellCoordinates(FVector WorldPosition) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Grid")
    FVector2D GetBaseGridCoordinates(FVector WorldPosition) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Grid")
    FVector2D BaseGridToMegaCell(int32 BaseGridX, int32 BaseGridY) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Grid")
    bool IsValidMegaCellCoordinate(int32 MegaCellX, int32 MegaCellY) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Grid")
    bool IsValidBaseGridCoordinate(int32 BaseGridX, int32 BaseGridY) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Grid")
    FVector2D GetMegaCellCenter(int32 MegaCellX, int32 MegaCellY) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Grid")
    int32 GetTotalUnitCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Grid")
    int32 GetActiveMegaCellCount() const;

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid Debug")
    void DebugPrintGridStats() const;

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid Debug")
    void DebugDrawGrid(UWorld* World, float Duration = 5.0f) const;

    UFUNCTION(BlueprintCallable, Category = "Spatial Grid Debug")
    void DebugDrawMegaCellBoundaries(UWorld* World, float Duration = 5.0f) const;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    float BaseGridCellSize;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    int32 BaseGridWidth;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    int32 BaseGridHeight;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    int32 MegaCellsPerDimension; 

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    int32 MegaGridWidth;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    int32 MegaGridHeight;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    float MegaCellSize;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    FVector2D WorldMin;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    FVector2D WorldMax;

    UPROPERTY()
    TArray<FSpatialCell> MegaCells;

    UPROPERTY()
    TMap<ABaseUnit*, FVector2D> UnitToMegaCellMap;

private:
    int32 GetMegaCellIndex(int32 MegaCellX, int32 MegaCellY) const;
    FSpatialCell* GetMegaCell(int32 MegaCellX, int32 MegaCellY);
    const FSpatialCell* GetMegaCell(int32 MegaCellX, int32 MegaCellY) const;

    void GetNeighboringMegaCells(int32 MegaCellX, int32 MegaCellY, TArray<FVector2D>& NeighborCoords) const;
    bool IsWithinMegaGridBounds(int32 MegaCellX, int32 MegaCellY) const;

    void HandleCombatInMegaCellInternal(FSpatialCell* MegaCell);
    void ProcessUnitsInMegaCell(const TArray<ABaseUnit*>& Units);

    void InitializeMegaCells();
    void ClearGrid();
};