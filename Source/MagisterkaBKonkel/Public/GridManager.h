#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h" 
#include "Materials/MaterialInterface.h"
#include "GridManager.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAGISTERKABKONKEL_API AGridManager : public AActor
{
    GENERATED_BODY()

public:
    AGridManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
    int32 GridWidth = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
    int32 GridHeight = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
    float CellSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
    bool bShowGrid = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Visual")
    UMaterialInterface* GridLineMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Visual")
    float LineThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Visual")
    float LineOpacity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Visual")
    FLinearColor GridColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Collision")
    float FloorThickness = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Collision")
    float FloorDepth = 50.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Boundaries")
    bool bCreateMapBoundaries = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Boundaries")
    float BoundaryWallHeight = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Boundaries")
    float BoundaryWallThickness = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Boundaries")
    float BoundaryOffset = 25.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zones", meta = (ClampMin = "1"))
    int32 Player1SpawnSize = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zones", meta = (ClampMin = "1"))
    int32 Player2SpawnSize = 3;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Zones")
    TArray<FVector2D> Player1SpawnZone;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Zones")
    TArray<FVector2D> Player2SpawnZone;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    FVector GetWorldLocationFromGrid(FVector2D GridPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    FVector2D GetGridPositionFromWorld(FVector WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool IsValidGridPosition(FVector2D GridPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool IsPositionInPlayer1SpawnZone(FVector2D GridPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool IsPositionInPlayer2SpawnZone(FVector2D GridPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    TArray<FVector2D> GetValidSpawnPositions(int32 PlayerID) const;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spawn Zones")
    void RegenerateSpawnZones();

    UFUNCTION(BlueprintCallable, Category = "Spawn Zones")
    void InitializeSpawnZones();

    UFUNCTION(BlueprintCallable, Category = "Grid Visual")
    void GenerateGridVisuals();

    UFUNCTION(BlueprintCallable, Category = "Grid Visual")
    void ClearGridVisuals();

    UFUNCTION(BlueprintCallable, Category = "Grid Visual")
    void ShowGrid();

    UFUNCTION(BlueprintCallable, Category = "Grid Visual")
    void HideGrid();

    UFUNCTION(BlueprintCallable, Category = "Grid Visual")
    void HighlightCell(FVector2D GridPosition, FLinearColor HighlightColor);

    UFUNCTION(BlueprintCallable, Category = "Grid Visual")
    void AddGridLineComponent(UStaticMeshComponent* Component);

    UFUNCTION(BlueprintCallable, Category = "Map Boundaries")
    void CreateMapBoundaries();

    UFUNCTION(BlueprintCallable, Category = "Map Boundaries")
    void ClearMapBoundaries();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Map Boundaries")
    void RegenerateMapBoundaries();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid Collision")
    class UBoxComponent* GridCollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Boundaries")
    TArray<class UBoxComponent*> BoundaryWalls;

    UPROPERTY()
    TArray<UStaticMeshComponent*> GridLineComponents;

    void CreateGridLines();
    void CreateGridCell(int32 X, int32 Y);
    UStaticMeshComponent* CreateLineComponent(FVector Start, FVector End);

    void SetupCollisionBox();

    void CreateBoundaryWall(FVector Location, FVector Extent, FString Name);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    FVector GridOrigin = FVector::ZeroVector;
    bool bGridGenerated = false;
};