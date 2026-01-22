// BaseUnit.h - Enhanced with Combat Replication
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "BaseUnit.generated.h"

class AUnitManager;
class USpatialGrid;

UENUM(BlueprintType)
enum class EBaseUnitType : uint8
{
    Tank    UMETA(DisplayName = "Tank"),
    Ninja      UMETA(DisplayName = "Ninja"),
    Sword     UMETA(DisplayName = "Sword"),
    Armor        UMETA(DisplayName = "Armor")
};

UENUM(BlueprintType)
enum class EAnimationState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Moving      UMETA(DisplayName = "Moving"),
    Attacking   UMETA(DisplayName = "Attacking"),
    Dying       UMETA(DisplayName = "Dying"),
    Dead        UMETA(DisplayName = "Dead")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitDied, ABaseUnit*, DeadUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUnitDamaged, ABaseUnit*, DamagedUnit, int32, DamageAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBaseUnitMoved, ABaseUnit*, MovedUnit);

UCLASS(BlueprintType, Blueprintable)
class MAGISTERKABKONKEL_API ABaseUnit : public AActor
{
    GENERATED_BODY()

public:
    ABaseUnit();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootSceneComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USkeletalMeshComponent* UnitMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UWidgetComponent* HealthBarWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Unit Stats", meta = (ClampMin = "1", DisplayName = "Max Health"))
    int32 MaxHealth = 100;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Unit Stats", meta = (DisplayName = "Current Health"))
    int32 CurrentHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats", meta = (ClampMin = "1", DisplayName = "Attack Power"))
    int32 Attack = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats", meta = (ClampMin = "0", DisplayName = "Defense"))
    int32 Defense = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats", meta = (ClampMin = "1", DisplayName = "Movement Speed"))
    float Speed = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats", meta = (ClampMin = "1", DisplayName = "Attack Range"))
    float AttackRange = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats", meta = (ClampMin = "0", DisplayName = "Unit Cost"))
    int32 Cost = 10;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, Category = "Position")
    FVector WorldPosition = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position", meta = (DisplayName = "Movement Step Size"))
    float MovementStepSize = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position", meta = (DisplayName = "Min World Bounds"))
    FVector MinWorldBounds = FVector(-1000, -1000, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position", meta = (DisplayName = "Max World Bounds"))
    FVector MaxWorldBounds = FVector(1000, 1000, 100);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Grid")
    FVector2D GridPosition = FVector2D(0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    float GridSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Unit Settings", meta = (DisplayName = "Team ID"))
    int32 TeamID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Unit Settings", meta = (DisplayName = "Unit Type"))
    EBaseUnitType UnitType = EBaseUnitType::Tank;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Animation", meta = (DisplayName = "Current Animation State"))
    EAnimationState AnimationState = EAnimationState::Idle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.1", ClampMax = "5.0", DisplayName = "Animation Speed Multiplier"))
    float AnimationSpeed = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Status")
    bool bIsAlive = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Status")
    bool bCanMove = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Status")
    bool bCanAttack = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar", meta = (ClampMin = "10", ClampMax = "200", DisplayName = "Health Bar Height"))
    float HealthBarHeight = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar", meta = (DisplayName = "Show Health Bar"))
    bool bShowHealthBar = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar", meta = (DisplayName = "Health Bar Always Visible"))
    bool bHealthBarAlwaysVisible = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar 3D", meta = (DisplayName = "Always Face Camera"))
    bool bHealthBarFaceCamera = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar 3D", meta = (DisplayName = "Scale With Distance"))
    bool bScaleWithDistance = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar 3D", meta = (ClampMin = "0.1", ClampMax = "3.0", DisplayName = "Min Scale"))
    float MinHealthBarScale = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar 3D", meta = (ClampMin = "0.1", ClampMax = "5.0", DisplayName = "Max Scale"))
    float MaxHealthBarScale = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar 3D", meta = (ClampMin = "500", ClampMax = "5000", DisplayName = "Max Visibility Distance"))
    float MaxHealthBarVisibilityDistance = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar 3D", meta = (DisplayName = "Health Bar Offset"))
    FVector HealthBarOffset = FVector(0, 0, 80);

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUnitDied OnUnitDied;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUnitDamaged OnUnitDamaged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBaseUnitMoved OnBaseUnitMoved;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void ReceiveDamage(int32 DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual bool AttackTarget(ABaseUnit* Target);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual bool MoveToGridPosition(FVector2D NewGridPosition);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual bool CanMoveToGridPosition(FVector2D NewGridPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual bool MoveToWorldPosition(FVector NewWorldPosition);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual bool CanMoveToWorldPosition(FVector NewWorldPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual void RotateTowardsDirection(FVector Direction);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual void RotateTowardsTarget(ABaseUnit* Target);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void Die();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual TArray<ABaseUnit*> GetValidTargets(const TArray<ABaseUnit*>& AllUnits) const;

    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual TArray<FVector> GetValidMovePositions(const TArray<ABaseUnit*>& AllUnits) const;

    UFUNCTION(BlueprintCallable, Category = "Health Bar")
    virtual void UpdateHealthBar();

    UFUNCTION(BlueprintCallable, Category = "Health Bar")
    virtual void ShowHealthBar();

    UFUNCTION(BlueprintCallable, Category = "Health Bar")
    virtual void HideHealthBar();

    UFUNCTION(BlueprintCallable, Category = "Health Bar")
    virtual void SetHealthBarVisibility(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Health Bar 3D")
    virtual void BillboardHealthBarToCamera();

    UFUNCTION(BlueprintCallable, Category = "Health Bar 3D")
    virtual void UpdateHealthBarScaleBasedOnDistance();

    UFUNCTION(BlueprintCallable, Category = "Health Bar 3D")
    virtual void UpdateHealthBarVisibilityBasedOnDistance();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Stats")
    float GetHealthPercentage() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grid")
    FVector GetWorldPositionFromGrid(FVector2D GridPos) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grid")
    FVector2D GetGridPositionFromWorld(FVector WorldPos) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    bool IsValidTarget(const ABaseUnit* Target) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    float GetDistanceToTarget(const ABaseUnit* Target) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Position")
    bool IsWithinBattlefieldBounds(FVector Position) const;

    UFUNCTION(BlueprintCallable, Category = "Animation")
    virtual void SetAnimationState(EAnimationState NewState);

    UFUNCTION(BlueprintImplementableEvent, Category = "Animation")
    void OnAnimationStateChanged(EAnimationState OldState, EAnimationState NewState);

    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnUnitSpawned();

    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnDamageReceived(int32 DamageAmount, int32 RemainingHealth);

    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnAttackPerformed(ABaseUnit* Target);

    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnMovementCompleted(FVector OldPosition, FVector NewPosition);

    UFUNCTION(BlueprintImplementableEvent, Category = "Health Bar")
    void OnHealthBarUpdated(float HealthPercentage);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1", ClampMax = "10.0", DisplayName = "Attack Cooldown"))
    float AttackCooldown = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1", ClampMax = "10.0", DisplayName = "Movement Speed"))
    float MovementSpeed = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Combat", meta = (DisplayName = "Auto Combat Enabled"))
    bool bAutoCombatEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (DisplayName = "Search Range"))
    float SearchRange = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (DisplayName = "Search Range Using Grid"))
    float SearchRangeGrid = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (DisplayName = "Rotation Speed"))
    float RotationSpeed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (DisplayName = "Smooth Rotation"))
    bool bSmoothRotation = true;

    UFUNCTION(BlueprintCallable, Category = "Combat Enhanced")
    virtual ABaseUnit* FindNearestEnemyOptimized(const TArray<ABaseUnit*>& AllUnits) const;

    UFUNCTION(BlueprintCallable, Category = "Combat Enhanced")
    virtual ABaseUnit* FindNearestEnemyUsingSpatialGrid(AUnitManager* UnitManager) const;

    UFUNCTION(BlueprintCallable, Category = "Combat Enhanced")
    virtual void MoveTowardsTargetOptimized(ABaseUnit* Target);

    UFUNCTION(BlueprintCallable, Category = "Combat Enhanced")
    virtual bool IsPathClearToTarget(ABaseUnit* Target, const TArray<ABaseUnit*>& NearbyUnits) const;

    UFUNCTION(BlueprintCallable, Category = "Combat Enhanced")
    virtual FVector FindAlternativeMovementPosition(ABaseUnit* Target, const TArray<ABaseUnit*>& NearbyUnits) const;

    UFUNCTION(BlueprintCallable, Category = "Combat Enhanced")
    virtual AUnitManager* GetUnitManager() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Combat State")
    ABaseUnit* CurrentTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Combat State")
    bool bIsAttacking;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Combat State")
    bool bIsMovingToTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat State")
    float LastAttackTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1", ClampMax = "2.0", DisplayName = "Movement Interval"))
    float MovementInterval = 0.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat State")
    float LastMovementTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.5", ClampMax = "3.0", DisplayName = "Target Search Interval"))
    float TargetSearchInterval = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat State")
    float LastTargetSearchTime;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void StartAutoCombat();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void StopAutoCombat();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void FindAndAttackNearestEnemy(const TArray<ABaseUnit*>& AllUnits);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual ABaseUnit* FindNearestEnemy(const TArray<ABaseUnit*>& AllUnits) const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void MoveTowardsTarget(ABaseUnit* Target);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual bool CanAttackTarget(ABaseUnit* Target) const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void PerformAttack(ABaseUnit* Target);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void UpdateCombatBehavior(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void SetTarget(ABaseUnit* NewTarget);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void ClearTarget();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    virtual bool HasValidTarget() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    virtual float GetDistanceToCurrentTarget() const;

    UFUNCTION(NetMulticast, Reliable)
    void MulticastReceiveDamage(int32 DamageAmount, int32 NewHealth);

    UFUNCTION(NetMulticast, Reliable) 
    void MulticastPerformAttack(ABaseUnit* Target);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastSetTarget(ABaseUnit* NewTarget);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastClearTarget();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastDie();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastSetAnimationState(EAnimationState NewState);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastMoveToWorldPosition(FVector NewPosition);

    UFUNCTION()
    void OnRep_CurrentHealth();

    UFUNCTION()
    void OnRep_AnimationState();

    UFUNCTION() 
    void OnRep_CurrentTarget();

    UFUNCTION()
    void OnRep_bIsAttacking();

    UFUNCTION()
    void OnRep_bIsMovingToTarget();

    UFUNCTION()
    void OnRep_WorldPosition();
    void HideUnit();

protected:
    virtual void UpdateWorldPosition();
    virtual bool CanPerformAction() const;
    virtual void OnHealthChanged();
    virtual void SetupHealthBar();
    virtual void RotateTowardsDirection(FVector Direction, float DeltaTime);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Enhanced")
    bool bUseSpatialOptimizations = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Enhanced")
    float PathClearanceRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Enhanced")
    int32 MaxAlternativePathAttempts = 8;

private:
    UPROPERTY()
    class APlayerController* PlayerController;
};