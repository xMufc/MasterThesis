// BaseUnit.cpp - Rozszerzone o system walki i replikację sieciową
#include "BaseUnit.h"
#include "HealthBarWidget.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "UnitManager.h" 
#include "SpatialGrid.h"

/// <summary>
/// KONSTRUKTOR
/// </summary>
ABaseUnit::ABaseUnit()
{
    // Włącz przetwarzanie co klatkę (Tick)
    PrimaryActorTick.bCanEverTick = true;

    // Włącz replikację tego aktora
    bReplicates = true;
    // Aktor zawsze istotny dla wszystkich klientów
    bAlwaysRelevant = true;
    // Automatyczna replikacja ruchu aktora
    SetReplicatingMovement(true);

    // Główny komponent root 
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    RootComponent = RootSceneComponent;

    // Komponent siatki szkieletowej
    UnitMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("UnitMesh"));
    UnitMesh->SetupAttachment(RootComponent);

    // Widget 3D wyświetlający pasek zdrowia nad jednostką
    HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
    HealthBarWidget->SetupAttachment(RootComponent);

    // Ustaw widget jako element 3D w świecie gry
    HealthBarWidget->SetWidgetSpace(EWidgetSpace::World);
    // Rozmiar widgetu w jednostkach świata
    HealthBarWidget->SetDrawSize(FVector2D(200.0f, 40.0f));
    // Widget widoczny z obu stron
    HealthBarWidget->SetTwoSided(true);
    // Rysuj w pożądanym rozmiarze
    HealthBarWidget->SetDrawAtDesiredSize(true);
    // Wyłącz kolizje - pasek zdrowia nie powinien blokować
    HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    MaxHealth = 100;              // Maksymalne zdrowie
    CurrentHealth = MaxHealth;     // Aktualne zdrowie
    Attack = 10;                   // Siła ataku
    Defense = 5;                   // Obrona
    Speed = 5.0f;                  // Prędkość poruszania się
    AttackRange = 1.5f;            // Zasięg ataku
    Cost = 10;                     // Koszt wyszkolenia jednostki

    AttackCooldown = 1.5f;         // Czas między atakami
    MovementSpeed = 2.0f;          // Prędkość ruchu w walce
    bAutoCombatEnabled = false;    // Automatyczna walka wyłączona domyślnie
    SearchRange = 2000.0f;         // Zasięg wyszukiwania wrogów
    SearchRangeGrid = 1.0f;        // Zasięg w siatce przestrzennej
    MovementInterval = 2.0f;       // Częstotliwość aktualizacji ruchu
    TargetSearchInterval = 0.125f;   // Częstotliwość szukania nowych celów
    MovementStepSize = 5.0f;       // Wielkość kroku ruchu

    CurrentTarget = nullptr;       // Brak początkowego celu
    bIsAttacking = false;          // Jednostka nie atakuje
    bIsMovingToTarget = false;     // Jednostka nie porusza się do celu
    LastAttackTime = 0.0f;         // Czas ostatniego ataku
    LastMovementTime = 0.0f;       // Czas ostatniego ruchu
    LastTargetSearchTime = 0.0f;   // Czas ostatniego szukania celu
    RotationSpeed = 5.0f;          // Prędkość obrotów jednostki
    bSmoothRotation = true;        // Włącz płynne obroty

    GridPosition = FVector2D(0, 0); // Pozycja w siatce
    GridSize = 100.0f;              // Rozmiar pojedynczej komórki siatki

    TeamID = 0;                     // ID drużyny 
    UnitType = EBaseUnitType::Tank; // Typ jednostki

    AnimationState = EAnimationState::Idle; // Początkowy stan: bezczynność

    bIsAlive = true;               // Jednostka żyje
    bCanMove = true;               // Może się poruszać
    bCanAttack = true;             // Może atakować

    HealthBarHeight = 80.0f;       // Wysokość nad jednostką
    bShowHealthBar = true;         // Pokaż pasek zdrowia
    bHealthBarAlwaysVisible = false; // Nie zawsze widoczny

    bHealthBarFaceCamera = true;   // Zawsze skierowany do kamery
    bScaleWithDistance = true;     // Skaluj z odległością od kamery
    MinHealthBarScale = 0.5f;      // Minimalna skala
    MaxHealthBarScale = 2.0f;      // Maksymalna skala
    MaxHealthBarVisibilityDistance = 2000.0f; // Maksymalna odległość widoczności
    HealthBarOffset = FVector(0, 0, 80); // Przesunięcie nad jednostką
}


/// <summary>
/// Rejestracja właściwości do replikacji
/// </summary>
void ABaseUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replikuj maksymalne zdrowie do wszystkich klientów
    DOREPLIFETIME(ABaseUnit, MaxHealth);
    // Replikuj zdrowie z natychmiastowym powiadomieniem 
    DOREPLIFETIME_CONDITION_NOTIFY(ABaseUnit, CurrentHealth, COND_None, REPNOTIFY_Always);
    // ID drużyny 
    DOREPLIFETIME(ABaseUnit, TeamID);
    // Typ jednostki
    DOREPLIFETIME(ABaseUnit, UnitType);

    // Pozycja w siatce
    DOREPLIFETIME_CONDITION(ABaseUnit, GridPosition, COND_None);
    // Pozycja w świecie z powiadomieniem o zmianie
    DOREPLIFETIME_CONDITION_NOTIFY(ABaseUnit, WorldPosition, COND_None, REPNOTIFY_Always);

    // Aktualny cel ataku
    DOREPLIFETIME_CONDITION_NOTIFY(ABaseUnit, CurrentTarget, COND_None, REPNOTIFY_Always);
    // Czy jednostka atakuje
    DOREPLIFETIME_CONDITION_NOTIFY(ABaseUnit, bIsAttacking, COND_None, REPNOTIFY_Always);
    // Czy porusza się do celu
    DOREPLIFETIME_CONDITION_NOTIFY(ABaseUnit, bIsMovingToTarget, COND_None, REPNOTIFY_Always);
    // Czy automatyczna walka jest włączona
    DOREPLIFETIME_CONDITION(ABaseUnit, bAutoCombatEnabled, COND_None);

    // Stan animacji z powiadomieniem
    DOREPLIFETIME_CONDITION_NOTIFY(ABaseUnit, AnimationState, COND_None, REPNOTIFY_Always);
    // Czy jednostka żyje
    DOREPLIFETIME_CONDITION(ABaseUnit, bIsAlive, COND_None);
    // Czy może się poruszać
    DOREPLIFETIME_CONDITION(ABaseUnit, bCanMove, COND_None);
    // Czy może atakować
    DOREPLIFETIME_CONDITION(ABaseUnit, bCanAttack, COND_None);
}

/// <summary>
/// Inicjalizacja po utworzeniu aktora
/// </summary>
void ABaseUnit::BeginPlay()
{
    Super::BeginPlay();

    // Pobierz kontroler gracza 
    PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    // Ustaw pełne zdrowie na starcie
    CurrentHealth = MaxHealth;

    // Skonfiguruj i zaktualizuj pasek zdrowia
    SetupHealthBar();
    UpdateHealthBar();

    // Wywołaj event spawnu jednostki
    OnUnitSpawned();

    // Log informacyjny o utworzeniu jednostki
    UE_LOG(LogTemp, Warning, TEXT("BaseUnit BeginPlay: %s - Authority: %s"),
        *GetName(), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
}

/// <summary>
/// Wywoływanie co klatkę
/// </summary>
/// <param name="DeltaTime">Odstęp miedzy klatkami</param>
void ABaseUnit::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // AKTUALIZACJA PASKA ZDROWIA 3D
    if (HealthBarWidget && bShowHealthBar && PlayerController)
    {
        // Obróć pasek zdrowia w kierunku kamery 
        if (bHealthBarFaceCamera)
        {
            BillboardHealthBarToCamera();
        }

        // Skaluj pasek w zależności od odległości od kamery
        if (bScaleWithDistance)
        {
            UpdateHealthBarScaleBasedOnDistance();
        }

        // Ukryj pasek jeśli za daleko
        UpdateHealthBarVisibilityBasedOnDistance();
    }

    // PŁYNNE OBROTY PODCZAS RUCHU
    // Jeśli włączone płynne obroty i jednostka się porusza do celu
    if (bSmoothRotation && bIsMovingToTarget && HasValidTarget())
    {
        // Oblicz kierunek do celu
        FVector Direction = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        // Płynnie obróć się w tym kierunku
        RotateTowardsDirection(Direction, DeltaTime);
    }

    // Logika walki wykonywana wyłącznie przez serwer 
    // Klienci tylko wyświetlają efekty replikowane z serwera
    if (HasAuthority())
    {
        UpdateCombatBehavior(DeltaTime);
    }
}


/// <summary>
/// Aktualizacja otrzymanych obrażen
/// </summary>
/// <param name="DamageAmount">Liczba obrażen</param>
void ABaseUnit::ReceiveDamage(int32 DamageAmount)
{
    // Tylko serwer przetwarza obrażenia
    if (!HasAuthority())
        return;

    // Sprawdź czy jednostka żyje i obrażenia są > 0
    if (!bIsAlive || DamageAmount <= 0)
        return;

    // Odejmij obronę, ale zawsze zadaj minimum 1 obrażenia
    int32 ActualDamage = FMath::Max(DamageAmount - Defense, 1);
    // Oblicz nowe zdrowie (nie poniżej 0)
    int32 NewHealth = FMath::Max(CurrentHealth - ActualDamage, 0);

    // Log informacyjny o obrażeniach
    UE_LOG(LogTemp, Warning, TEXT("=== DAMAGE: %s received %d damage, health: %d -> %d ==="),
        *GetName(), ActualDamage, CurrentHealth, NewHealth);

    // Zaktualizuj zdrowie
    CurrentHealth = NewHealth;

    // Wyślij informację o obrażeniach do wszystkich klientów
    MulticastReceiveDamage(ActualDamage, CurrentHealth);

    // AKTUALIZACJA INTERFEJSU
    OnHealthChanged();           // Event zmiany zdrowia
    UpdateHealthBar();           // Zaktualizuj pasek zdrowia
    OnDamageReceived(ActualDamage, CurrentHealth); // Event otrzymania obrażeń
    OnUnitDamaged.Broadcast(this, ActualDamage);   // Broadcast dla innych systemów

    // Jeśli pasek nie jest zawsze widoczny, pokaż go tymczasowo
    if (!bHealthBarAlwaysVisible)
    {
        ShowHealthBar();

        // Ukryj pasek po 3 sekundach
        FTimerHandle HideHealthBarTimer;
        GetWorldTimerManager().SetTimer(HideHealthBarTimer, [this]()
            {
                if (!bHealthBarAlwaysVisible && CurrentHealth > 0)
                {
                    HideHealthBar();
                }
            }, 3.0f, false);
    }

    // Jeśli zdrowie spadło do 0, jednostka umiera
    if (CurrentHealth <= 0)
    {
        Die();
    }
}

/// <summary>
/// Replikacja obrażeń do wszystkich klientów
/// </summary>
/// <param name="DamageAmount">Liczba obrazen</param>
/// <param name="NewHealth">Nowe punkty zycia</param>
void ABaseUnit::MulticastReceiveDamage_Implementation(int32 DamageAmount, int32 NewHealth)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST DAMAGE: %s received %d damage, new health: %d ==="),
        *GetName(), DamageAmount, NewHealth);

    // Wykonaj tylko na klientach
    if (!HasAuthority())
    {
        // Zaktualizuj zdrowie na kliencie
        CurrentHealth = NewHealth;

        // Aktualizuj interfejs i eventy
        OnHealthChanged();
        UpdateHealthBar();
        OnDamageReceived(DamageAmount, CurrentHealth);

        // Tymczasowo pokaż pasek zdrowia
        if (!bHealthBarAlwaysVisible)
        {
            ShowHealthBar();

            FTimerHandle HideHealthBarTimer;
            GetWorldTimerManager().SetTimer(HideHealthBarTimer, [this]()
                {
                    if (!bHealthBarAlwaysVisible && CurrentHealth > 0)
                    {
                        HideHealthBar();
                    }
                }, 3.0f, false);
        }
    }
}

/// <summary>
/// Atak celu
/// </summary>
/// <param name="Target">Atakowany obiekt</param>
/// <returns>true - w przypadku powodzenia, false - wpp</returns>
bool ABaseUnit::AttackTarget(ABaseUnit* Target)
{
    // Tylko serwer wykonuje ataki
    if (!HasAuthority())
        return false;

    // Sprawdź czy jednostka może atakować i cel jest prawidłowy
    if (!CanPerformAction() || !bCanAttack || !IsValidTarget(Target))
        return false;

    // Sprawdź zasięg ataku
    if (GetDistanceToTarget(Target) > AttackRange)
        return false;

    UE_LOG(LogTemp, Warning, TEXT("=== ATTACK: %s attacking %s ==="), *GetName(), *Target->GetName());

    SetAnimationState(EAnimationState::Attacking);

    // Wyślij animację ataku do wszystkich klientów
    MulticastPerformAttack(Target);

    // Zadanie obrażen
    Target->ReceiveDamage(Attack);

    // Event wykonania ataku
    OnAttackPerformed(Target);

    return true;
}

/// <summary>
/// Replikacja animacji ataku
/// </summary>
/// <param name="Target">Atakowany obiekt</param>
void ABaseUnit::MulticastPerformAttack_Implementation(ABaseUnit* Target)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST ATTACK: %s performing attack on %s ==="),
        *GetName(), Target ? *Target->GetName() : TEXT("NULL"));

    // Wykonaj animację ataku tylko na klientach
    if (!HasAuthority())
    {
        SetAnimationState(EAnimationState::Attacking);
        OnAttackPerformed(Target);
    }
}

/// <summary>
/// Poruszenie jednostki do danej pozycji
/// </summary>
/// <param name="NewGridPosition">Nowa pozycja</param>
/// <returns>true - w przypadku powodzenia, false - wpp</returns>
bool ABaseUnit::MoveToGridPosition(FVector2D NewGridPosition)
{
    // Tylko serwer przetwarza ruch
    if (!HasAuthority())
        return false;

    // Walidacja ruchu
    if (!CanPerformAction() || !bCanMove || NewGridPosition == GridPosition)
        return false;

    // Zapisz starą pozycję
    FVector2D OldPosition = GridPosition;
    // Ustaw nową pozycję w siatce
    GridPosition = NewGridPosition;

    // Przelicz i zaktualizuj pozycję w świecie 3D
    UpdateWorldPosition();
    // Zmień animację na ruch
    SetAnimationState(EAnimationState::Moving);

    // Broadcast eventu ruchu
    OnBaseUnitMoved.Broadcast(this);

    return true;
}

/// <summary>
/// Poruszenie jednostki do danej pozycji w swiecie 3D
/// </summary>
/// <param name="NewWorldPosition">Nowa pozycja</param>
/// <returns>true - w przypadku powodzenia, false - wpp</returns>
bool ABaseUnit::MoveToWorldPosition(FVector NewWorldPosition)
{
    // Tylko serwer może inicjować ruch
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("=== MOVEMENT ERROR: Client tried to call MoveToWorldPosition! Unit: %s ==="), *GetName());
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MoveToWorldPosition: Unit %s attempting to move to (%f,%f,%f) ==="),
        *GetName(), NewWorldPosition.X, NewWorldPosition.Y, NewWorldPosition.Z);

    // Walidacja możliwości ruchu
    if (!CanPerformAction() || !bCanMove)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SERVER MoveToWorldPosition: Cannot perform action - CanPerformAction: %s, bCanMove: %s ==="),
            CanPerformAction() ? TEXT("TRUE") : TEXT("FALSE"),
            bCanMove ? TEXT("TRUE") : TEXT("FALSE"));
        return false;
    }

    // Sprawdź czy można się ruszyć do tej pozycji
    if (!CanMoveToWorldPosition(NewWorldPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SERVER MoveToWorldPosition: CanMoveToWorldPosition returned false ==="));
        return false;
    }

    // Wykonanie ruchu
    FVector OldPosition = GetActorLocation();
    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MoveToWorldPosition: Moving from (%f,%f,%f) to (%f,%f,%f) ==="),
        OldPosition.X, OldPosition.Y, OldPosition.Z, NewWorldPosition.X, NewWorldPosition.Y, NewWorldPosition.Z);

    // Oblicz kierunek ruchu dla rotacji
    FVector MovementDirection = (NewWorldPosition - OldPosition).GetSafeNormal();

    // Obróć jednostkę w kierunku ruchu
    if (!MovementDirection.IsNearlyZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SERVER MoveToWorldPosition: Rotating towards direction (%f,%f,%f) ==="),
            MovementDirection.X, MovementDirection.Y, MovementDirection.Z);
        RotateTowardsDirection(MovementDirection);
    }

    // Ustaw pozycję najpierw na SERWERZE
    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MoveToWorldPosition: Setting actor location on SERVER ==="));
    SetActorLocation(NewWorldPosition);
    WorldPosition = NewWorldPosition;

    // Weryfikuj czy pozycja została ustawiona
    FVector ActualPosition = GetActorLocation();
    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MoveToWorldPosition: Position verification - Expected: (%f,%f,%f), Actual: (%f,%f,%f) ==="),
        NewWorldPosition.X, NewWorldPosition.Y, NewWorldPosition.Z,
        ActualPosition.X, ActualPosition.Y, ActualPosition.Z);

    // Wyślij RPC multicast do wszystkich klientów
    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MoveToWorldPosition: Sending MulticastMoveToWorldPosition RPC to clients ==="));
    MulticastMoveToWorldPosition(NewWorldPosition);

    // Aktualizacja stanu
    SetAnimationState(EAnimationState::Moving);
    OnMovementCompleted(OldPosition, NewWorldPosition);
    OnBaseUnitMoved.Broadcast(this);

    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT SUCCESS: Unit %s moved from (%f,%f,%f) to (%f,%f,%f) and RPC sent ==="),
        *GetName(), OldPosition.X, OldPosition.Y, OldPosition.Z,
        NewWorldPosition.X, NewWorldPosition.Y, NewWorldPosition.Z);

    return true;
}

/// <summary>
/// Replikacja ruchu do wszystkich klientow
/// </summary>
/// <param name="NewPosition">Nowa pozycja</param>
void ABaseUnit::MulticastMoveToWorldPosition_Implementation(FVector NewPosition)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST MOVEMENT RPC: Unit %s received movement RPC to (%f,%f,%f) ==="),
        *GetName(), NewPosition.X, NewPosition.Y, NewPosition.Z);
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST MOVEMENT RPC: HasAuthority: %s ==="),
        HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));

    // Wykonaj ruch tylko na klientach
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== CLIENT MOVEMENT: Processing movement RPC for unit %s ==="), *GetName());

        FVector OldPosition = GetActorLocation();
        UE_LOG(LogTemp, Warning, TEXT("=== CLIENT MOVEMENT: Current position: (%f,%f,%f), Target position: (%f,%f,%f) ==="),
            OldPosition.X, OldPosition.Y, OldPosition.Z, NewPosition.X, NewPosition.Y, NewPosition.Z);

        // Oblicz kierunek ruchu dla rotacji
        FVector MovementDirection = (NewPosition - OldPosition).GetSafeNormal();

        // Obróć jednostkę na kliencie
        if (!MovementDirection.IsNearlyZero())
        {
            UE_LOG(LogTemp, Warning, TEXT("=== CLIENT MOVEMENT: Rotating towards direction (%f,%f,%f) ==="),
                MovementDirection.X, MovementDirection.Y, MovementDirection.Z);
            RotateTowardsDirection(MovementDirection);
        }

        // Przesuń jednostkę na kliencie
        UE_LOG(LogTemp, Warning, TEXT("=== CLIENT MOVEMENT: Setting actor location on CLIENT ==="));
        SetActorLocation(NewPosition);
        WorldPosition = NewPosition;

        // Weryfikuj pozycję na kliencie
        FVector ActualClientPosition = GetActorLocation();
        UE_LOG(LogTemp, Warning, TEXT("=== CLIENT MOVEMENT: Position verification - Expected: (%f,%f,%f), Actual: (%f,%f,%f) ==="),
            NewPosition.X, NewPosition.Y, NewPosition.Z,
            ActualClientPosition.X, ActualClientPosition.Y, ActualClientPosition.Z);

        SetAnimationState(EAnimationState::Moving);
        OnMovementCompleted(OldPosition, NewPosition);

        UE_LOG(LogTemp, Warning, TEXT("=== CLIENT MOVEMENT SUCCESS: Unit %s moved from (%f,%f,%f) to (%f,%f,%f) ==="),
            *GetName(), OldPosition.X, OldPosition.Y, OldPosition.Z,
            ActualClientPosition.X, ActualClientPosition.Y, ActualClientPosition.Z);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT RPC: Server received own multicast (normal) ==="));
    }
}

/// <summary>
/// Smierc jednostki
/// </summary>
void ABaseUnit::Die()
{
    // Tylko serwer przetwarza śmierć
    if (!HasAuthority())
        return;

    // Sprawdź czy jednostka już nie umarła
    if (!bIsAlive)
        return;

    UE_LOG(LogTemp, Warning, TEXT("=== DEATH: %s is dying ==="), *GetName());

    bIsAlive = false;
    bCanMove = false;
    bCanAttack = false;

    // Ustaw animację umierania
    SetAnimationState(EAnimationState::Dying);

    // Wyślij informację o śmierci do wszystkich klientów
    MulticastDie();

    // Broadcast eventu śmierci
    OnUnitDied.Broadcast(this);

    // Ukryj pasek zdrowia
    HideHealthBar();

    // Po 2 sekundach zniszcz aktora
    FTimerHandle DestroyTimer;
    GetWorldTimerManager().SetTimer(DestroyTimer, [this]()
        {
            SetAnimationState(EAnimationState::Dead);
            Destroy();
        }, 2.0f, false);
}

/// <summary>
/// Replikacja smierci jednostki do wszystkich klientow
/// </summary>
void ABaseUnit::MulticastDie_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST DEATH: %s dying on client ==="), *GetName());

    // Wykonaj na klientach
    if (!HasAuthority())
    {
        bIsAlive = false;
        bCanMove = false;
        bCanAttack = false;

        SetAnimationState(EAnimationState::Dying);
        HideHealthBar();

        // Timer na kliencie
        FTimerHandle DestroyTimer;
        GetWorldTimerManager().SetTimer(DestroyTimer, [this]()
            {
                SetAnimationState(EAnimationState::Dead);
            }, 2.0f, false);
    }
}

/// <summary>
/// Zmiana stanu animacji
/// </summary>
/// <param name="NewState">Nowy stan</param>
void ABaseUnit::SetAnimationState(EAnimationState NewState)
{
    // Sprawdź czy stan się zmienił
    if (AnimationState != NewState)
    {
        EAnimationState OldState = AnimationState;
        AnimationState = NewState;

        // Jeśli to serwer, wyślij zmianę do klientów
        if (HasAuthority())
        {
            MulticastSetAnimationState(NewState);
        }

        // Event zmiany stanu animacji
        OnAnimationStateChanged(OldState, NewState);

        UE_LOG(LogTemp, Warning, TEXT("=== ANIMATION: %s changed from %d to %d ==="),
            *GetName(), (int32)OldState, (int32)NewState);
    }
}

/// <summary>
/// Replikacja nowego stanu animacji do wszystkich klientow
/// </summary>
/// <param name="NewState">Nowy stan</param>
void ABaseUnit::MulticastSetAnimationState_Implementation(EAnimationState NewState)
{
    // Wykonaj na klientach
    if (!HasAuthority())
    {
        EAnimationState OldState = AnimationState;
        AnimationState = NewState;
        OnAnimationStateChanged(OldState, NewState);

        UE_LOG(LogTemp, Warning, TEXT("=== CLIENT ANIMATION: %s changed to %d ==="),
            *GetName(), (int32)NewState);
    }
}

/// <summary>
/// Ustawienie celu ataku
/// </summary>
/// <param name="NewTarget">Nowy cel</param>
void ABaseUnit::SetTarget(ABaseUnit* NewTarget)
{
    // Tylko serwer ustawia cele
    if (!HasAuthority())
        return;

    // Sprawdź czy cel się zmienił
    if (CurrentTarget != NewTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== TARGET SET: %s changing target from %s to %s ==="),
            *GetName(),
            CurrentTarget ? *CurrentTarget->GetName() : TEXT("NONE"),
            NewTarget ? *NewTarget->GetName() : TEXT("NONE"));

        CurrentTarget = NewTarget;

        // Wyślij nowy cel do wszystkich klientów
        MulticastSetTarget(NewTarget);

        if (NewTarget)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== TARGET SET: %s new target set to %s ==="),
                *GetName(), *NewTarget->GetName());
        }
    }
}

/// <summary>
/// Replikacja zmiany celu ataku
/// </summary>
/// <param name="NewTarget">Nowy cel ataku</param>
void ABaseUnit::MulticastSetTarget_Implementation(ABaseUnit* NewTarget)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST TARGET: %s setting target to %s ==="),
        *GetName(), NewTarget ? *NewTarget->GetName() : TEXT("NONE"));

    // Wykonaj na klientach
    if (!HasAuthority())
    {
        CurrentTarget = NewTarget;
    }
}

/// <summary>
/// Czyszczenie aktualnego celu ataku
/// </summary>
void ABaseUnit::ClearTarget()
{
    // Tylko serwer czyści cele
    if (!HasAuthority())
        return;

    if (CurrentTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== TARGET CLEAR: %s clearing target %s ==="),
            *GetName(), *CurrentTarget->GetName());

        CurrentTarget = nullptr;
        bIsMovingToTarget = false;
        bIsAttacking = false;

        // === REPLIKACJA CZYSZCZENIA CELU ===
        MulticastClearTarget();
    }
}

/// <summary>
/// Replikacja czyszczenia aktualnego celu ataku
/// </summary>
void ABaseUnit::MulticastClearTarget_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST CLEAR TARGET: %s clearing target ==="), *GetName());

    // Wykonaj na klientach
    if (!HasAuthority())
    {
        CurrentTarget = nullptr;
        bIsMovingToTarget = false;
        bIsAttacking = false;
    }
}

/// <summary>
/// Wywoływane automatycznie przy replikacji zmiany zdrowia
/// </summary>
void ABaseUnit::OnRep_CurrentHealth()
{
    UE_LOG(LogTemp, Warning, TEXT("=== REP HEALTH: %s health changed to %d ==="), *GetName(), CurrentHealth);
    OnHealthChanged();
    UpdateHealthBar();
}

/// <summary>
/// Wywoływane automatycznie przy replikacji zmiany stanu animacji
/// </summary>
void ABaseUnit::OnRep_AnimationState()
{
    UE_LOG(LogTemp, Warning, TEXT("=== REP ANIMATION: %s animation state changed to %d ==="),
        *GetName(), (int32)AnimationState);
}

/// <summary>
/// Wywoływane automatycznie przy replikacji zmiany celu ataku
/// </summary>
void ABaseUnit::OnRep_CurrentTarget()
{
    UE_LOG(LogTemp, Warning, TEXT("=== REP TARGET: %s target changed to %s ==="),
        *GetName(), CurrentTarget ? *CurrentTarget->GetName() : TEXT("NONE"));
}

/// <summary>
/// Wywoływane automatycznie przy replikacji wykonanego ataku
/// </summary>
void ABaseUnit::OnRep_bIsAttacking()
{
    UE_LOG(LogTemp, Warning, TEXT("=== REP ATTACKING: %s attacking state: %s ==="),
        *GetName(), bIsAttacking ? TEXT("TRUE") : TEXT("FALSE"));
}

/// <summary>
/// Wywoływane automatycznie przy replikacji poruszenia sie
/// </summary>
void ABaseUnit::OnRep_bIsMovingToTarget()
{
    UE_LOG(LogTemp, Warning, TEXT("=== REP MOVING: %s moving to target state: %s ==="),
        *GetName(), bIsMovingToTarget ? TEXT("TRUE") : TEXT("FALSE"));
}

/// <summary>
/// Wywoływane automatycznie przy replikacji pozycji na swiecie 3D
/// </summary>
void ABaseUnit::OnRep_WorldPosition()
{
    UE_LOG(LogTemp, Warning, TEXT("=== REP NOTIFY POSITION: %s world position replicated to (%f,%f,%f) ==="),
        *GetName(), WorldPosition.X, WorldPosition.Y, WorldPosition.Z);

    FVector CurrentActorPos = GetActorLocation();
    UE_LOG(LogTemp, Warning, TEXT("=== REP NOTIFY POSITION: Current actor position: (%f,%f,%f) ==="),
        CurrentActorPos.X, CurrentActorPos.Y, CurrentActorPos.Z);

    // Zaktualizuj lokację aktora gdy WorldPosition zostanie zreplikowane
    SetActorLocation(WorldPosition);

    FVector NewActorPos = GetActorLocation();
    UE_LOG(LogTemp, Warning, TEXT("=== REP NOTIFY POSITION: Actor position after update: (%f,%f,%f) ==="),
        NewActorPos.X, NewActorPos.Y, NewActorPos.Z);
}

/// <summary>
/// Walidacja ruchu do pozycji w siatce
/// </summary>
/// <param name="NewGridPosition">Nowa pozycja</param>
/// <returns>true - w przypadku możliwości przemieszczenia się, false - wpp</returns>
bool ABaseUnit::CanMoveToGridPosition(FVector2D NewGridPosition) const
{
    // Sprawdź czy jednostka żyje i może się poruszać
    if (!bIsAlive || !bCanMove || NewGridPosition == GridPosition)
        return false;

    // Sprawdź dystans 
    float Distance = FVector2D::Distance(GridPosition, NewGridPosition);
    if (Distance > 1.0f)
        return false;

    return true;
}

/// <summary>
/// Pobieranie prawidlowych celow w zasiegu
/// </summary>
/// <param name="AllUnits">Wszystkie jednostki</param>
/// <returns>Tablica osiagalnych przeciwnikow</returns>
TArray<ABaseUnit*> ABaseUnit::GetValidTargets(const TArray<ABaseUnit*>& AllUnits) const
{
    TArray<ABaseUnit*> ValidTargets;

    // Sprawdź czy jednostka może atakować
    if (!bIsAlive || !bCanAttack)
        return ValidTargets;

    // Przefiltruj jednostki
    for (ABaseUnit* Unit : AllUnits)
    {
        if (IsValidTarget(Unit) && GetDistanceToTarget(Unit) <= AttackRange)
        {
            ValidTargets.Add(Unit);
        }
    }

    return ValidTargets;
}

/// <summary>
/// Pobranie prawidlowej pozycji do ruchu
/// </summary>
/// <param name="AllUnits">Tablica wszystkich jednostek</param>
/// <returns>Tablica wszystkich możliwych pozycji</returns>
TArray<FVector> ABaseUnit::GetValidMovePositions(const TArray<ABaseUnit*>& AllUnits) const
{
    TArray<FVector> ValidPositions;

    // Sprawdź czy jednostka może się poruszać
    if (!bIsAlive || !bCanMove)
        return ValidPositions;

    FVector CurrentPosition = GetActorLocation();

    // Sprawdź 8 kierunków wokół jednostki
    const int32 NumDirections = 8;
    for (int32 i = 0; i < NumDirections; i++)
    {
        // Oblicz kąt dla danego kierunku
        float Angle = (2.0f * PI * i) / NumDirections;
        // Oblicz wektor kierunku
        FVector Direction = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        // Oblicz testową pozycję
        FVector TestPosition = CurrentPosition + (Direction * Speed);
        TestPosition.Z = CurrentPosition.Z; // Zachowaj wysokość

        // Sprawdź granice pola bitwy
        if (!IsWithinBattlefieldBounds(TestPosition))
            continue;

        // Sprawdź kolizje z innymi jednostkami
        bool bOccupied = false;
        for (const ABaseUnit* Unit : AllUnits)
        {
            if (Unit && Unit != this && Unit->bIsAlive)
            {
                float DistanceToUnit = FVector::Dist(TestPosition, Unit->GetActorLocation());
                if (DistanceToUnit < 50.0f) // Za blisko innej jednostki
                {
                    bOccupied = true;
                    break;
                }
            }
        }

        // Dodaj pozycję jeśli jest wolna
        if (!bOccupied)
        {
            ValidPositions.Add(TestPosition);
        }
    }

    return ValidPositions;
}

/// <summary>
/// Aktualizacja paska zdrowia
/// </summary>
void ABaseUnit::UpdateHealthBar()
{
    // Sprawdź czy widget istnieje i jest włączony
    if (!HealthBarWidget || !bShowHealthBar)
        return;

    // Pobierz widget użytkownika i zaktualizuj wartości
    if (UHealthBarWidget* HealthWidget = Cast<UHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
    {
        float HealthPercentage = GetHealthPercentage();
        HealthWidget->UpdateHealthBar(HealthPercentage, CurrentHealth, MaxHealth);
        OnHealthBarUpdated(HealthPercentage);
    }
}

/// <summary>
/// Pokazanie paska zdrowia
/// </summary>
void ABaseUnit::ShowHealthBar()
{
    if (HealthBarWidget)
    {
        HealthBarWidget->SetVisibility(true);
    }
}

/// <summary>
/// Ukrycie paska zdrowia
/// </summary>
void ABaseUnit::HideHealthBar()
{
    if (HealthBarWidget && !bHealthBarAlwaysVisible)
    {
        HealthBarWidget->SetVisibility(false);
    }
}

/// <summary>
/// Ustawienie widocznosci paska zdrowia
/// </summary>
/// <param name="bVisible">Aktualny stan</param>
void ABaseUnit::SetHealthBarVisibility(bool bVisible)
{
    if (HealthBarWidget)
    {
        HealthBarWidget->SetVisibility(bVisible);
    }
}

/// <summary>
/// Pasek zdrowia skierowany do kamery
/// </summary>
void ABaseUnit::BillboardHealthBarToCamera()
{
    if (!PlayerController || !PlayerController->PlayerCameraManager || !HealthBarWidget)
        return;

    // Pobierz pozycję kamery
    FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
    // Pobierz pozycję paska zdrowia
    FVector HealthBarLocation = HealthBarWidget->GetComponentLocation();

    // Oblicz kierunek do kamery
    FVector DirectionToCamera = (CameraLocation - HealthBarLocation).GetSafeNormal();
    // Oblicz rotację aby pasek był skierowany do kamery
    FRotator LookAtRotation = DirectionToCamera.Rotation();

    // Zastosuj rotację
    HealthBarWidget->SetWorldRotation(LookAtRotation);
}

/// <summary>
/// Skalowanie paska zdrowia w zaleznosci od odleglosci
/// </summary>
void ABaseUnit::UpdateHealthBarScaleBasedOnDistance()
{
    if (!PlayerController || !PlayerController->PlayerCameraManager || !HealthBarWidget)
        return;

    // Pobierz pozycje kamery i paska zdrowia
    FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
    FVector HealthBarLocation = HealthBarWidget->GetComponentLocation();

    // Oblicz odległość
    float Distance = FVector::Dist(CameraLocation, HealthBarLocation);
    // Normalizuj odległość
    float NormalizedDistance = FMath::Clamp(Distance / 700.0f, 0.1f, 3.0f);
    // Ogranicz skalę do zdefiniowanych limitów
    float Scale = FMath::Clamp(NormalizedDistance, MinHealthBarScale, MaxHealthBarScale);

    // Zastosuj skalę
    HealthBarWidget->SetRelativeScale3D(FVector(Scale));
}

/// <summary>
/// Ukrywanie paska zdrowia przy duzej odleglosci
/// </summary>
void ABaseUnit::UpdateHealthBarVisibilityBasedOnDistance()
{
    if (!PlayerController || !PlayerController->PlayerCameraManager || !HealthBarWidget)
        return;

    // Pobierz pozycje
    FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
    FVector HealthBarLocation = HealthBarWidget->GetComponentLocation();

    // Oblicz odległość
    float Distance = FVector::Dist(CameraLocation, HealthBarLocation);
    // Pokaż tylko jeśli w zasięgu i włączone
    bool bShouldBeVisible = Distance <= MaxHealthBarVisibilityDistance && bShowHealthBar;
    HealthBarWidget->SetVisibility(bShouldBeVisible);
}

/// <summary>
/// Obliczanie procentu zdrowia
/// </summary>
/// <returns>Procent zdrowia</returns>
float ABaseUnit::GetHealthPercentage() const
{
    return MaxHealth > 0 ? (float)CurrentHealth / (float)MaxHealth : 0.0f;
}

/// <summary>
/// Konwersja pozycji siatki na pozycje w swiecie 3D
/// </summary>
/// <param name="GridPos">Aktualna pozycja</param>
/// <returns>Pozycja w swiecie 3D</returns>
FVector ABaseUnit::GetWorldPositionFromGrid(FVector2D GridPos) const
{
    return FVector(GridPos.X * GridSize, GridPos.Y * GridSize, GetActorLocation().Z);
}

/// <summary>
/// Konwersja pozycjia swiata 3D na pozycje w siatce
/// </summary>
/// <param name="WorldPos">Pozycja w swiecie 3D</param>
/// <returns>Pozycja w siatce</returns>
FVector2D ABaseUnit::GetGridPositionFromWorld(FVector WorldPos) const
{
    return FVector2D(WorldPos.X / GridSize, WorldPos.Y / GridSize);
}

/// <summary>
/// Walidacja celu
/// </summary>
/// <param name="Target">Cel</param>
/// <returns>true - wprzypadku prawidlowoscie false - wpp</returns>
bool ABaseUnit::IsValidTarget(const ABaseUnit* Target) const
{
    // Cel jest prawidłowy jeśli:
    // - Istnieje (nie nullptr)
    // - Nie jest to ta sama jednostka
    // - Cel żyje
    // - Cel jest z innej drużyny (wróg)
    return Target && Target != this && Target->bIsAlive && Target->TeamID != TeamID;
}

/// <summary>
/// Obliczenie odbleglosci do aktualnego celu
/// </summary>
/// <returns>Odleglosc</returns>
float ABaseUnit::GetDistanceToCurrentTarget() const
{
    if (!HasValidTarget())
        return FLT_MAX; // Maksymalna wartość float jeśli brak celu

    return FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
}

/// <summary>
/// Odleglosc do celu
/// </summary>
/// <param name="Target">Cel</param>
/// <returns>Odleglosc</returns>
float ABaseUnit::GetDistanceToTarget(const ABaseUnit* Target) const
{
    if (!Target)
        return FLT_MAX;

    return FVector::Dist(GetActorLocation(), Target->GetActorLocation());
}

/// <summary>
/// Aktualizacja pozycji w świecie na podstawie siatki
/// </summary>
void ABaseUnit::UpdateWorldPosition()
{
    // Przelicz pozycję siatki na współrzędne świata
    FVector NewWorldPosition = GetWorldPositionFromGrid(GridPosition);
    // Przenieś aktora
    SetActorLocation(NewWorldPosition);
}

/// <summary>
/// Sprawdzenie czy jednostka może wykonac akcję
/// </summary>
/// <returns></returns>
bool ABaseUnit::CanPerformAction() const
{
    // Jednostka może działać jeśli:
    // - Żyje
    // - Nie umiera (animacja śmierci)
    // - Nie jest już martwa
    return bIsAlive &&
        AnimationState != EAnimationState::Dying &&
        AnimationState != EAnimationState::Dead;
}

/// <summary>
/// Event zmiany zdrowia
/// </summary>
void ABaseUnit::OnHealthChanged()
{
    UpdateHealthBar();
}

/// <summary>
/// Konfiguracja paska zdrowia
/// </summary>
void ABaseUnit::SetupHealthBar()
{
    if (!HealthBarWidget)
        return;

    // Ustaw przesunięcie paska nad jednostką
    HealthBarWidget->SetRelativeLocation(HealthBarOffset);

    // Pokaż lub ukryj na podstawie ustawień
    if (bHealthBarAlwaysVisible)
    {
        ShowHealthBar();
    }
    else
    {
        HideHealthBar();
    }
}

/// <summary>
/// Uruchomienie automatycznej walki
/// </summary>
void ABaseUnit::StartAutoCombat()
{
    // Tylko serwer kontroluje automatyczną walkę
    if (!HasAuthority()) {
        return;
    }

    // Sprawdź czy jednostka żyje
    if (!bIsAlive)
        return;

    // Włącz tryb auto-combat
    bAutoCombatEnabled = true;
    UE_LOG(LogTemp, Warning, TEXT("=== AUTO COMBAT: Unit %s - Auto combat started ==="), *GetName());
}

/// <summary>
/// Zatrzymanie automatycznej walki
/// </summary>
void ABaseUnit::StopAutoCombat()
{
    // Tylko serwer kontroluje automatyczną walkę
    if (!HasAuthority())
        return;

    // Wyłącz tryb auto-combat
    bAutoCombatEnabled = false;
    // Wyczyść cel
    ClearTarget();
    // Wróć do stanu bezczynności
    SetAnimationState(EAnimationState::Idle);
    UE_LOG(LogTemp, Warning, TEXT("=== AUTO COMBAT: Unit %s - Auto combat stopped ==="), *GetName());
}

/// <summary>
/// Znalezienie i zaatakowanie najbliższego przeciwnika
/// </summary>
/// <param name="AllUnits">Wszystkie jednostki</param>
void ABaseUnit::FindAndAttackNearestEnemy(const TArray<ABaseUnit*>& AllUnits)
{
    // Tylko serwer wykonuje logikę walki
    if (!HasAuthority())
        return;

    // Sprawdź czy jednostka może walczyć
    if (!bIsAlive || !bAutoCombatEnabled)
        return;

    UE_LOG(LogTemp, Warning, TEXT("=== COMBAT DEBUG: Unit %s checking for enemies, current target: %s ==="),
        *GetName(), HasValidTarget() ? *CurrentTarget->GetName() : TEXT("NONE"));

    // Jeśli mamy cel i możemy go zaatakować - atakuj
    if (HasValidTarget() && CanAttackTarget(CurrentTarget))
    {
        UE_LOG(LogTemp, Warning, TEXT("=== COMBAT DEBUG: Unit %s can attack current target %s ==="),
            *GetName(), *CurrentTarget->GetName());
        PerformAttack(CurrentTarget);
        return;
    }

    // Jeśli nie mamy prawidłowego celu
    if (!HasValidTarget())
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        // Sprawdź czy minął interwał wyszukiwania
        if (CurrentTime - LastTargetSearchTime < TargetSearchInterval)
            return;

        // Użyj zoptymalizowanego wyszukiwania (z partycjonowaniem przestrzeni jeśli dostępne)
        ABaseUnit* NewTarget = FindNearestEnemyOptimized(AllUnits);

        if (NewTarget)
        {
            // Znaleziono wroga - ustaw jako cel i ruszaj w jego kierunku
            SetTarget(NewTarget);
            MoveTowardsTarget(NewTarget);
            LastTargetSearchTime = CurrentTime;
        }
        else
        {
            // Nie znaleziono wroga - wyczyść cel i czekaj
            ClearTarget();
            SetAnimationState(EAnimationState::Idle);
        }
    }
}

/// <summary>
/// Znalezienie najbliższego przeciwnika
/// </summary>
/// <param name="AllUnits">Wszystkie jednostki</param>
/// <returns>Nowy cel ataku</returns>
ABaseUnit* ABaseUnit::FindNearestEnemy(const TArray<ABaseUnit*>& AllUnits) const
{
    if (!bIsAlive)
        return nullptr;

    ABaseUnit* NearestEnemy = nullptr;
    float NearestDistance = SearchRange;

    UE_LOG(LogTemp, Warning, TEXT("=== TARGET SEARCH: Unit %s searching for enemies among %d units (ORIGINAL METHOD) ==="),
        *GetName(), AllUnits.Num());

    // Przeszukaj wszystkie jednostki
    for (ABaseUnit* Unit : AllUnits)
    {
        // Pomiń nieprawidłowe jednostki, martwe i sojuszników
        if (!Unit || !Unit->bIsAlive || Unit->TeamID == TeamID)
            continue;

        // Oblicz odległość
        float Distance = FVector::Dist(GetActorLocation(), Unit->GetActorLocation());
        UE_LOG(LogTemp, Warning, TEXT("=== TARGET SEARCH: Found enemy %s, distance %f ==="),
            *Unit->GetName(), Distance);

        // Sprawdź czy to najbliższy wróg
        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            NearestEnemy = Unit;
            UE_LOG(LogTemp, Warning, TEXT("=== TARGET SEARCH: New nearest enemy: %s at distance %f ==="),
                *Unit->GetName(), Distance);
        }
    }

    return NearestEnemy;
}

/// <summary>
/// Znalezienie najbliższego przeciwnika
/// </summary>
/// <param name="AllUnits">Wszystkie jednostki</param>
/// <returns>Nowy cel ataku</returns>
ABaseUnit* ABaseUnit::FindNearestEnemyOptimized(const TArray<ABaseUnit*>& AllUnits) const
{
    if (!bIsAlive)
        return nullptr;

    // Spróbuj uzyskać dostęp do UnitManager dla partycjonowania przestrzeni
    AUnitManager* UnitManager = GetUnitManager();

    // Jeśli mamy dostęp do siatki przestrzennej, użyj jej
    if (UnitManager && UnitManager->IsSpatialPartitioningEnabled())
    {
        return FindNearestEnemyUsingSpatialGrid(UnitManager);
    }

    // Fallback do podstawowej metody jeśli partycjonowanie niedostępne
    UE_LOG(LogTemp, Warning, TEXT("=== COMBAT: Unit %s using fallback enemy search ==="), *GetName());
    return FindNearestEnemy(AllUnits);
}

/// <summary>
/// Znalezienie przeciwnika za pomocą siatki przestrzennej
/// </summary>
/// <param name="UnitManager">Obiekt UnitManager</param>
/// <returns>Nowy cel ataku</returns>
ABaseUnit* ABaseUnit::FindNearestEnemyUsingSpatialGrid(AUnitManager* UnitManager) const
{
    if (!UnitManager || !bIsAlive)
        return nullptr;

    // Pobierz siatkę przestrzenną z managera jednostek
    USpatialGrid* SpatialGrid = UnitManager->GetSpatialGrid();
    if (!SpatialGrid)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL COMBAT: Unit %s - Spatial grid not available ==="), *GetName());
        return nullptr;
    }

    // Użyj zoptymalizowanego wyszukiwania wroga przez siatkę
    ABaseUnit* NearestEnemy = SpatialGrid->FindNearestEnemy(const_cast<ABaseUnit*>(this), SearchRangeGrid);

    if (NearestEnemy)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL COMBAT: Unit %s found enemy %s using spatial grid ==="),
            *GetName(), *NearestEnemy->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL COMBAT: Unit %s found no enemies in range using spatial grid ==="),
            *GetName());
    }

    return NearestEnemy;
}

/// <summary>
/// Pobranie UnitManagera
/// </summary>
/// <returns>Obiekt UnitManager</returns>
AUnitManager* ABaseUnit::GetUnitManager() const
{
    if (!GetWorld())
        return nullptr;

    // Znajdź UnitManager w świecie - używamy TActorIterator
    for (TActorIterator<AUnitManager> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
    {
        AUnitManager* UnitManager = *ActorIterator;
        // Zwróć tylko jeśli to serwer
        if (UnitManager && UnitManager->HasAuthority())
        {
            return UnitManager;
        }
    }

    return nullptr;
}

/// <summary>
/// Zooptymalizowania metoda ruchu do celu
/// </summary>
/// <param name="Target">Aktualny cel</param>
void ABaseUnit::MoveTowardsTargetOptimized(ABaseUnit* Target)
{
    // Tylko serwer kontroluje ruch
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("=== MOVEMENT ERROR: Client tried to call MoveTowardsTargetOptimized! Unit: %s ==="), *GetName());
        return;
    }

    // Walidacja podstawowa
    if (!Target || !bIsAlive || !bCanMove)
    {
        return;
    }

    AUnitManager* UnitManager = GetUnitManager();
    if (UnitManager && UnitManager->IsSpatialPartitioningEnabled())
    {
        // Pobierz pobliskie jednostki z siatki przestrzennej
        USpatialGrid* SpatialGrid = UnitManager->GetSpatialGrid();
        if (SpatialGrid)
        {
            // Pobierz jednostki w promieniu 2x prędkości
            TArray<ABaseUnit*> NearbyUnits = SpatialGrid->GetUnitsInRange(GetActorLocation(), Speed * 2.0f);

            // Sprawdź czy ścieżka do celu jest wolna
            if (IsPathClearToTarget(Target, NearbyUnits))
            {
                // Bezpośrednia ścieżka wolna - ruszaj prosto
                MoveTowardsTarget(Target);
                return;
            }
            else
            {
                // Ścieżka zablokowana - znajdź alternatywną trasę
                FVector AlternativePosition = FindAlternativeMovementPosition(Target, NearbyUnits);
                if (!AlternativePosition.IsZero())
                {
                    MoveToWorldPosition(AlternativePosition);
                    return;
                }
            }
        }
    }

    // Fallback do podstawowego ruchu
    MoveTowardsTarget(Target);
}

/// <summary>
/// Sprawdzenie czy ściezka do celu jest pusta
/// </summary>
/// <param name="Target">Cel ataku</param>
/// <param name="NearbyUnits">Pobliskie jednostki</param>
/// <returns>true - w przypadku powodzenia, false - wpp</returns>
bool ABaseUnit::IsPathClearToTarget(ABaseUnit* Target, const TArray<ABaseUnit*>& NearbyUnits) const
{
    if (!Target)
        return false;

    FVector MyPosition = GetActorLocation();
    FVector TargetPosition = Target->GetActorLocation();
    FVector Direction = (TargetPosition - MyPosition).GetSafeNormal();
    float DistanceToTarget = FVector::Dist(MyPosition, TargetPosition);

    // Sprawdź czy jakieś pobliskie jednostki blokują ścieżkę
    for (ABaseUnit* Unit : NearbyUnits)
    {
        // Pomiń siebie, cel i martwe jednostki
        if (!Unit || Unit == this || Unit == Target || !Unit->bIsAlive)
            continue;

        FVector UnitPosition = Unit->GetActorLocation();
        FVector ToUnit = UnitPosition - MyPosition;

        // Sprawdź czy jednostka jest mniej więcej w kierunku celu
        float DotProduct = FVector::DotProduct(Direction, ToUnit.GetSafeNormal());
        if (DotProduct > 0.5f) // Jednostka jest przed nami
        {
            float DistanceToUnit = ToUnit.Size();
            // Sprawdź czy blokuje ścieżkę
            if (DistanceToUnit < DistanceToTarget && DistanceToUnit < Speed * 1.5f)
            {
                return false; // Jednostka blokuje ścieżkę
            }
        }
    }

    return true; // Ścieżka wolna
}

/// <summary>
/// Znalezienie alternatywnej pozycji ruchu
/// </summary>
/// <param name="Target">Aktualny cel ataku</param>
/// <param name="NearbyUnits">Pobliskie jednostki</param>
/// <returns>Nowy cel ataku</returns>
FVector ABaseUnit::FindAlternativeMovementPosition(ABaseUnit* Target, const TArray<ABaseUnit*>& NearbyUnits) const
{
    if (!Target)
        return FVector::ZeroVector;

    FVector MyPosition = GetActorLocation();
    FVector TargetPosition = Target->GetActorLocation();
    FVector BaseDirection = (TargetPosition - MyPosition).GetSafeNormal();

    // Wypróbuj różne kąty aby znaleźć wolną ścieżkę
    const int32 NumDirections = 8;
    const float AngleStep = 360.0f / NumDirections;

    for (int32 i = 1; i < NumDirections; i++) // Zacznij od 1
    {
        float Angle = AngleStep * i;
        // Obróć kierunek o kąt
        FVector TestDirection = BaseDirection.RotateAngleAxis(Angle, FVector::UpVector);
        FVector TestPosition = MyPosition + (TestDirection * Speed);
        TestPosition.Z = MyPosition.Z; // Zachowaj wysokość

        // Sprawdź czy pozycja jest wolna
        bool bPositionClear = true;
        for (ABaseUnit* Unit : NearbyUnits)
        {
            if (!Unit || Unit == this || !Unit->bIsAlive)
                continue;

            float DistanceToTestPos = FVector::Dist(TestPosition, Unit->GetActorLocation());
            if (DistanceToTestPos < 100.0f) // Za blisko innej jednostki
            {
                bPositionClear = false;
                break;
            }
        }

        // Jeśli pozycja wolna i w granicach, użyj jej
        if (bPositionClear && CanMoveToWorldPosition(TestPosition))
        {
            UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL MOVEMENT: Unit %s found alternative path at angle %f ==="),
                *GetName(), Angle);
            return TestPosition;
        }
    }

    // Nie znaleziono wolnej alternatywy
    return FVector::ZeroVector;
}

/// <summary>
/// Ruch w danym kierunku
/// </summary>
/// <param name="Target">Aktualny cel ataku</param>
void ABaseUnit::MoveTowardsTarget(ABaseUnit* Target)
{
    // Tylko serwer kontroluje ruch
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("=== MOVEMENT ERROR: Client tried to call MoveTowardsTarget! Unit: %s ==="), *GetName());
        return;
    }

    if (!Target || !bIsAlive || !bCanMove)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== MOVEMENT DEBUG: MoveTowardsTarget early exit - Target: %s, bIsAlive: %s, bCanMove: %s ==="),
            Target ? *Target->GetName() : TEXT("NULL"),
            bIsAlive ? TEXT("TRUE") : TEXT("FALSE"),
            bCanMove ? TEXT("TRUE") : TEXT("FALSE"));
        return;
    }

    // Oblicz odległość do celu
    FVector MyPosition = GetActorLocation();
    FVector TargetPosition = Target->GetActorLocation();
    float DistanceToTarget = FVector::Dist(MyPosition, TargetPosition);

    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Unit %s -> Target %s ==="), *GetName(), *Target->GetName());
    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: MyPos(%f,%f,%f) -> TargetPos(%f,%f,%f) ==="),
        MyPosition.X, MyPosition.Y, MyPosition.Z, TargetPosition.X, TargetPosition.Y, TargetPosition.Z);
    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Distance: %f, AttackRange: %f ==="), DistanceToTarget, AttackRange);

    // Sprawdz czy jest w zasięgu
    if (DistanceToTarget <= AttackRange)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Unit %s is in attack range, stopping movement ==="), *GetName());
        // W zasięgu - obróć się do celu i zatrzymaj
        RotateTowardsTarget(Target);
        bIsMovingToTarget = false;
        SetAnimationState(EAnimationState::Idle);
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastMovementTime < MovementInterval)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Movement on cooldown - Time: %f, LastMove: %f, Interval: %f ==="),
            CurrentTime, LastMovementTime, MovementInterval);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Starting movement towards target ==="));

    // Ustaw cel i animacje
    SetTarget(Target);
    bIsMovingToTarget = true;
    SetAnimationState(EAnimationState::Moving);

    // Oblicz kierunek ruchu
    FVector Direction = (TargetPosition - MyPosition);
    Direction.Z = 0; // Zeruj składową Z (ruch tylko w płaszczyźnie XY)
    Direction = Direction.GetSafeNormal();

    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Movement direction: (%f,%f,%f) ==="),
        Direction.X, Direction.Y, Direction.Z);

    // Obróć się w kierunku celu
    RotateTowardsTarget(Target);

    // Oblicz następną pozycję
    FVector NextPosition = MyPosition + (Direction * Speed);
    NextPosition.Z = MyPosition.Z; // Zachowaj wysokość

    UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: NextPosition calculated: (%f,%f,%f) ==="),
        NextPosition.X, NextPosition.Y, NextPosition.Z);

    // Wykonaj ruch
    if (CanMoveToWorldPosition(NextPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Can move to position, calling MoveToWorldPosition ==="));
        bool bMoveSuccess = MoveToWorldPosition(NextPosition);

        if (bMoveSuccess)
        {
            LastMovementTime = CurrentTime;
            UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Movement successful, LastMovementTime updated to %f ==="), LastMovementTime);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("=== SERVER MOVEMENT ERROR: MoveToWorldPosition returned false! ==="));
        }
    }
    else
    {
        // Nie można się ruszyć - obróć się i czekaj
        UE_LOG(LogTemp, Warning, TEXT("=== SERVER MOVEMENT DEBUG: Cannot move to position, stopping movement ==="));
        RotateTowardsTarget(Target);
        bIsMovingToTarget = false;
        SetAnimationState(EAnimationState::Idle);
    }
}

/// <summary>
/// Sprawdzenie czy można zaatakowac cel
/// </summary>
/// <param name="Target">Cel ataku</param>
/// <returns>true - w przypadku powodzenia, false - wpp</returns>
bool ABaseUnit::CanAttackTarget(ABaseUnit* Target) const
{
    // Walidacja celu
    if (!Target || !bIsAlive || !bCanAttack || Target->TeamID == TeamID)
        return false;

    // Sprawdź czy cel żyje
    if (!Target->bIsAlive)
        return false;

    // Sprawdź odległość
    float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
    if (Distance > AttackRange)
        return false;

    // Sprawdź cooldown ataku
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastAttackTime < AttackCooldown)
        return false;

    return true;
}

/// <summary>
/// Wykonanie ataku
/// </summary>
/// <param name="Target">Aktualny cel ataku</param>
void ABaseUnit::PerformAttack(ABaseUnit* Target)
{
    // Tylko serwer wykonuje ataki
    if (!HasAuthority())
        return;

    // Sprawdź czy można zaatakować
    if (!CanAttackTarget(Target))
        return;

    // Rozpocznij atak
    bIsAttacking = true;
    SetAnimationState(EAnimationState::Attacking);
    LastAttackTime = GetWorld()->GetTimeSeconds();

    // Wykonaj atak
    if (AttackTarget(Target))
    {
        UE_LOG(LogTemp, Warning, TEXT("=== ATTACK EXECUTED: Unit %s attacked target %s ==="),
            *GetName(), *Target->GetName());
    }

    // Timer resetujący stan ataku
    FTimerHandle AttackResetTimer;
    GetWorldTimerManager().SetTimer(AttackResetTimer, [this]()
        {
            bIsAttacking = false;
            // Jeśli nie porusza się do celu, wróć do bezczynności
            if (!bIsMovingToTarget)
            {
                SetAnimationState(EAnimationState::Idle);
            }
        }, 0.5f, false);
}


/// <summary>
/// Aktualizacja zachowania bojowego
/// </summary>
/// <param name="DeltaTime">Czas między aktualizacjami</param>
void ABaseUnit::UpdateCombatBehavior(float DeltaTime)
{
    // Tylko serwer wykonuje logikę walki
    if (!HasAuthority())
        return;

    // Sprawdź czy jednostka może walczyć
    if (!bIsAlive || !bAutoCombatEnabled)
        return;

    // Sprawdz stan obecnego celu
    if (HasValidTarget())
    {
        float Distance = GetDistanceToCurrentTarget();
        UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Distance to target %s: %f, AttackRange: %f ==="),
            *CurrentTarget->GetName(), Distance, AttackRange);

        // Atak - Jeśli w zasięgu i gotowy do ataku 
        if (CanAttackTarget(CurrentTarget))
        {
            UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Unit %s can attack, stopping movement ==="), *GetName());
            bIsMovingToTarget = false;
            PerformAttack(CurrentTarget);
        }
        // Ruch - Jeśli poruszamy się do celu 
        else if (bIsMovingToTarget)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Unit %s continuing movement towards target ==="), *GetName());

            // Jeśli wciąż za daleko, kontynuuj ruch
            if (Distance > AttackRange)
            {
                FVector MyPosition = GetActorLocation();
                FVector TargetPosition = CurrentTarget->GetActorLocation();
                FVector Direction = (TargetPosition - MyPosition);
                Direction.Z = 0;
                Direction = Direction.GetSafeNormal();

                // Oblicz następną pozycję
                FVector NextPosition = MyPosition + (Direction * Speed);
                NextPosition.Z = MyPosition.Z;

                UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Calculated next position: (%f,%f,%f) ==="),
                    NextPosition.X, NextPosition.Y, NextPosition.Z);

                // Próbuj się poruszyć
                if (CanMoveToWorldPosition(NextPosition))
                {
                    UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Moving unit %s from (%f,%f,%f) to (%f,%f,%f) ==="),
                        *GetName(), MyPosition.X, MyPosition.Y, MyPosition.Z,
                        NextPosition.X, NextPosition.Y, NextPosition.Z);

                    bool bMoveSuccess = MoveToWorldPosition(NextPosition);
                    if (bMoveSuccess)
                    {
                        LastMovementTime = GetWorld()->GetTimeSeconds();
                        UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Movement successful ==="));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("=== COMBAT BEHAVIOR: Movement failed! ==="));
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Cannot move to calculated position ==="));
                }
            }
            else
            {
                // Wystarczająco blisko - zatrzymaj się
                UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Unit %s close enough to target, stopping movement ==="), *GetName());
                bIsMovingToTarget = false;
                SetAnimationState(EAnimationState::Idle);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Unit %s has target but not moving or attacking ==="), *GetName());
            ClearTarget();
        }
    }
    else
    {
        // Brak celu - Zatrzymaj ruch jeśli był aktywny
        if (bIsMovingToTarget)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== COMBAT BEHAVIOR: Unit %s lost target, stopping movement ==="), *GetName());
            bIsMovingToTarget = false;
            SetAnimationState(EAnimationState::Idle);
        }
    }
}

/// <summary>
/// Sprawdzenie czy jednostka moze poruszczyć się do danej pozycji
/// </summary>
/// <param name="NewWorldPosition">Nowa pozycja</param>
/// <returns>true - w przypadku powodzenia, false - wpp</returns>
bool ABaseUnit::CanMoveToWorldPosition(FVector NewWorldPosition) const
{
    // Sprawdź podstawowe warunki
    if (!bIsAlive || !bCanMove)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== CanMoveToWorldPosition: Unit %s cannot move - bIsAlive: %s, bCanMove: %s ==="),
            *GetName(), bIsAlive ? TEXT("TRUE") : TEXT("FALSE"), bCanMove ? TEXT("TRUE") : TEXT("FALSE"));
        return false;
    }

    // Sprawdź dystans ruchu
    float MovementDistance = FVector::Dist(GetActorLocation(), NewWorldPosition);
    float MaxMovement = Speed * 1.5f; // Maksymalna odległość za jeden ruch

    UE_LOG(LogTemp, Warning, TEXT("=== CanMoveToWorldPosition: Unit %s - Distance: %f, MaxMovement: %f ==="),
        *GetName(), MovementDistance, MaxMovement);

    // Nie pozwól na zbyt duże skoki
    if (MovementDistance > MaxMovement)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== CanMoveToWorldPosition: Movement distance too large for unit %s ==="), *GetName());
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== CanMoveToWorldPosition: Unit %s can move to position ==="), *GetName());
    return true;
}

/// <summary>
/// Sprawdzenie czy pozycja jest w granicach pola bitwy
/// </summary>
/// <param name="Position">Pozycja do sprawdzenia</param>
/// <returns>true - w przypadku pozycji wewnatrz mapy, false - wpp</returns>
bool ABaseUnit::IsWithinBattlefieldBounds(FVector Position) const
{
    // Sprawdź wszystkie 3 osie (X, Y, Z)
    return Position.X >= MinWorldBounds.X && Position.X <= MaxWorldBounds.X &&
        Position.Y >= MinWorldBounds.Y && Position.Y <= MaxWorldBounds.Y &&
        Position.Z >= MinWorldBounds.Z && Position.Z <= MaxWorldBounds.Z;
}

/// <summary>
/// Sprawdzenie prawidlowosci celu
/// </summary>
/// <returns>true - w przypadku prawidlowego celu, false = wpp</returns>
bool ABaseUnit::HasValidTarget() const
{
    // Cel jest prawidłowy jeśli istnieje, żyje i jest wrogiem
    return CurrentTarget &&
        CurrentTarget->bIsAlive &&
        CurrentTarget->TeamID != TeamID;
}

/// <summary>
/// Obrót jednostki w kierunku
/// </summary>
/// <param name="Direction">Kierunek</param>
void ABaseUnit::RotateTowardsDirection(FVector Direction)
{
    // Sprawdź czy kierunek jest prawidłowy
    if (Direction.IsNearlyZero())
        return;

    // Zeruj składową Z 
    Direction.Z = 0;
    Direction = Direction.GetSafeNormal();

    // Oblicz docelową rotację
    FRotator TargetRotation = Direction.Rotation();
    // Korekta kąta
    TargetRotation.Yaw -= 150.0f;

    // Zastosuj rotacje
    if (bSmoothRotation)
    {
        // Płynna rotacja - interpolacja
        FRotator CurrentRotation = GetActorRotation();
        float DeltaTime = GetWorld()->GetDeltaSeconds();
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed);
        SetActorRotation(NewRotation);
    }
    else
    {
        // Natychmiastowa rotacja
        SetActorRotation(TargetRotation);
    }
}

/// <summary>
/// Obrót jednostki w danym kierunku
/// </summary>
/// <param name="Direction">Kierunek</param>
/// <param name="DeltaTime">Czas wykonania</param>
void ABaseUnit::RotateTowardsDirection(FVector Direction, float DeltaTime)
{
    if (Direction.IsNearlyZero())
        return;

    Direction.Z = 0;
    Direction = Direction.GetSafeNormal();

    FRotator TargetRotation = Direction.Rotation();

    if (bSmoothRotation)
    {
        FRotator CurrentRotation = GetActorRotation();
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed);
        SetActorRotation(NewRotation);
    }
    else
    {
        SetActorRotation(TargetRotation);
    }
}

/// <summary>
/// Obrót jednostki w kierunku celu
/// </summary>
/// <param name="Target">Aktualny cel</param>
void ABaseUnit::RotateTowardsTarget(ABaseUnit* Target)
{
    if (!Target)
        return;

    // Oblicz kierunek do celu
    FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    // Obróć się w tym kierunku
    RotateTowardsDirection(Direction);
}

/// <summary>
/// Ukrycie jednostki
/// </summary>
void ABaseUnit::HideUnit()
{
    // Ukryj wizualnie w grze
    SetActorHiddenInGame(true);

    // Wyłącz kolizje
    SetActorEnableCollision(false);

    // Wyłącz przetwarzanie Tick (optymalizacja)
    SetActorTickEnabled(false);

    // Ukryj pasek zdrowia
    HideHealthBar();
}