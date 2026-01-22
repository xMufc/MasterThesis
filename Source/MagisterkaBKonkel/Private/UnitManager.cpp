// UnitManager.cpp - Zarządzanie jednostkami z systemem partycjonowania przestrzennego i optymalizacją pamięci podręcznej
#include "UnitManager.h"
#include "StrategyPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "SpatialGrid.h"

/// <summary>
/// Konstruktor klasy AUnitManager.
/// </summary>
AUnitManager::AUnitManager()
{
    PrimaryActorTick.bCanEverTick = true;

    // Inicjalizacja podstawowych referencji i stanu
    GridManagerRef = nullptr;
    SelectedUnit = nullptr;
    SelectedUnitPlayerID = -1;
    bInitialized = false;

    // Inicjalizacja parametrów fazy walki
    bCombatPhaseActive = false;
    CombatUpdateInterval = 1.0f;  // Częstotliwość aktualizacji walki w sekundach
    TotalCombatUnits = 0;

    // Klasy jednostek - przypisywane w Blueprincie
    TankUnitClass = nullptr;
    NinjaUnitClass = nullptr;
    SwordUnitClass = nullptr;
    ArmorUnitClass = nullptr;

    // Konfiguracja systemu partycjonowania przestrzennego dla optymalizacji wykrywania kolizji
    SpatialGrid = nullptr;
    bUseSpatialPartitioning = true;  // Domyślnie włączone dla zwiększenia wydajności
    SpatialGridCellSize = 200.0f;    // Rozmiar komórki siatki w jednostkach Unreal
    SpatialGridWorldMin = FVector2D(-2000, -2000);  // Dolne granice świata gry
    SpatialGridWorldMax = FVector2D(2000, 2000);    // Górne granice świata gry

    // Inicjalizacja systemu optymalizacji pamięci podręcznej
    MaxCombatUnits = 0;
    ActiveCombatUnitsCount = 0;

    // Konfiguracja replikacji sieciowej
    bReplicates = true;
    bAlwaysRelevant = true;  // Manager zawsze widoczny dla wszystkich klientów
    SetReplicateMovement(false);  // Manager jest statyczny, nie replikujemy ruchu
    SetNetUpdateFrequency(10.0f);  // 10 aktualizacji sieciowych na sekundę
}

/// <summary>
/// Definiuje które właściwości są synchronizowane przez sieć.
/// </summary>
/// <param name="OutLifetimeProps">Tablica właściwości do replikacji</param>
void AUnitManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replikacja stanu zaznaczenia jednostki
    DOREPLIFETIME(AUnitManager, SelectedUnit);
    DOREPLIFETIME(AUnitManager, SelectedUnitPlayerID);

    // Replikacja stanu fazy walki
    DOREPLIFETIME(AUnitManager, bCombatPhaseActive);
    DOREPLIFETIME(AUnitManager, CombatUpdateInterval);
}

/// <summary>
/// Wywoływane przy starcie gry po utworzeniu aktora.
/// </summary>
void AUnitManager::BeginPlay()
{
    Super::BeginPlay();

    // Inicjalizacja siatki przestrzennej
    if (bUseSpatialPartitioning)
    {
        InitializeSpatialGrid();
    }

    // Inicjalizacja systemu optymalizacji pamięci dla walki
    InitializeCombatDataLocality();

    // Ustawienie opóźnionego timera inicjalizacji GridManager
    GetWorldTimerManager().SetTimer(
        InitializeGridManagerHandle,
        FTimerDelegate::CreateUObject(this, &AUnitManager::InitializeGridManager),
        0.1f,  // Odczekaj 0.1 sekundy
        false  // Wykonaj tylko raz
    );

    UE_LOG(LogTemp, Warning, TEXT("UnitManager: BeginPlay zakończone - Autorytet: %s, PartycjonowaniePrzestrzenne: %s"),
        HasAuthority() ? TEXT("SERWER") : TEXT("KLIENT"),
        bUseSpatialPartitioning ? TEXT("WŁĄCZONE") : TEXT("WYŁĄCZONE"));
}

/// <summary>
/// Główna pętla aktualizacji wykonywana co klatkę.
/// </summary>
/// <param name="DeltaTime">Czas jaki upłynął od ostatniej klatki</param>
void AUnitManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Tylko serwer monitoruje warunki zakończenia walki
    if (HasAuthority() && bCombatPhaseActive)
    {
        CheckCombatEndConditions();
    }

    // Klienci ukrywają jednostki podczas walki
    if (!HasAuthority() && bCombatPhaseActive && SpawnedUnits.Num() != 0)
    {
        TArray<ABaseUnit*> UnitsToHide;
        for (const FSpawnedUnitData& UnitData : SpawnedUnits)
        {
            if (UnitData.Unit && IsValid(UnitData.Unit))
            {
                UnitsToHide.Add(UnitData.Unit);
            }
        }

        for (ABaseUnit* Unit : UnitsToHide)
        {
            Unit->HideUnit();
        }
    }
}

/// <summary>
/// Tworzy i inicjalizuje siatkę przestrzenną dla optymalizacji wykrywania pobliskich jednostek.
/// </summary>
void AUnitManager::InitializeSpatialGrid()
{
    // Tylko serwer inicjalizuje siatkę przestrzenną
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Klient pomija inicjalizację siatki ==="));
        return;
    }

    // Sprawdzenie czy siatka nie została już utworzona
    if (SpatialGrid)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Już zainicjalizowana ==="));
        return;
    }

    // Utworzenie nowego obiektu siatki przestrzennej
    SpatialGrid = NewObject<USpatialGrid>(this);
    if (SpatialGrid)
    {
        // Inicjalizacja z określonymi granicami świata i rozmiarem komórki
        SpatialGrid->InitializeGrid(SpatialGridWorldMin, SpatialGridWorldMax, SpatialGridCellSize);
        UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Pomyślnie zainicjalizowana ==="));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("=== SIATKA PRZESTRZENNA: Nie udało się utworzyć obiektu siatki ==="));
        bUseSpatialPartitioning = false;
    }
}

/// <summary>
/// Wypełnia siatkę przestrzenną wszystkimi aktywnymi jednostkami.
/// </summary>
void AUnitManager::PopulateSpatialGridWithAllUnits()
{
    if (!SpatialGrid)
    {
        UE_LOG(LogTemp, Error, TEXT("=== SIATKA PRZESTRZENNA: Nie można wypełnić - SpatialGrid jest null ==="));
        return;
    }

    int32 AddedUnits = 0;

    // Dodanie każdej żywej jednostki do siatki przestrzennej
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit) && UnitData.Unit->bIsAlive)
        {
            SpatialGrid->AddUnit(UnitData.Unit);
            AddedUnits++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Wypełniono %d jednostkami ==="), AddedUnits);

    // Wyświetlenie statystyk siatki dla debugowania
    if (AddedUnits > 0)
    {
        SpatialGrid->DebugPrintGridStats();
    }
}

/// <summary>
/// Usuwa wszystkie jednostki z siatki przestrzennej.
/// </summary>
void AUnitManager::ClearSpatialGrid()
{
    if (!SpatialGrid)
    {
        return;
    }

    // Usunięcie wszystkich jednostek z siatki
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit))
        {
            SpatialGrid->RemoveUnit(UnitData.Unit);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Wyczyszczono wszystkie jednostki ==="));
}

/// <summary>
/// Aktualizuje pozycje jednostek w siatce przestrzennej gdy się przemieszczają.
/// </summary>
void AUnitManager::UpdateSpatialGridPositions()
{
    if (!SpatialGrid)
    {
        return;
    }

    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit) && UnitData.Unit->bIsAlive)
        {
            // Pobranie aktualnej pozycji jednostki
            FVector CurrentPosition = UnitData.Unit->GetActorLocation();
            FVector LastPosition = CurrentPosition - FVector(10, 10, 0);
            SpatialGrid->UpdateUnitPosition(UnitData.Unit, LastPosition, CurrentPosition);
        }
    }
}

/// <summary>
/// Rozpoczyna fazę automatycznej walki między wszystkimi jednostkami.
/// </summary>
void AUnitManager::StartCombatPhase()
{
    UE_LOG(LogTemp, Warning, TEXT("=== FAZA WALKI: Rozpoczynanie fazy walki ==="));

    // Tylko serwer może rozpocząć walkę
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== FAZA WALKI: Klient nie może rozpocząć fazy walki ==="));
        return;
    }

    bCombatPhaseActive = true;
    TotalCombatUnits = SpawnedUnits.Num();

    // Wypełnienie siatki przestrzennej wszystkimi jednostkami dla optymalizacji
    if (bUseSpatialPartitioning && SpatialGrid)
    {
        PopulateSpatialGridWithAllUnits();
    }

    // Włączenie automatycznej walki dla wszystkich jednostek
    EnableAutoCombatForAllUnits();

    // Uruchomienie timera aktualizacji walki
    GetWorldTimerManager().SetTimer(
        CombatUpdateTimer,
        FTimerDelegate::CreateUObject(this, bUseSpatialPartitioning ?
            &AUnitManager::UpdateCombatUsingSpatialGrid :  // Metoda zoptymalizowana
            &AUnitManager::UpdateAllUnitsCombat),          // Metoda podstawowa
        CombatUpdateInterval,
        true  // Powtarzanie co określony interwał
    );

    // Uruchomienie timera monitorowania zdarzeń walki
    GetWorldTimerManager().SetTimer(
        CombatMonitorTimer,
        FTimerDelegate::CreateUObject(this, &AUnitManager::MonitorCombatEvents),
        0.5f,  // Sprawdzanie co pół sekundy
        true   // Ciągłe powtarzanie
    );

    // Podpięcie zdarzeń walki dla wszystkich jednostek
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit))
        {
            BindUnitCombatEvents(UnitData.Unit);
        }
    }

    // Replikacja rozpoczęcia walki do wszystkich klientów
    MulticastCombatStarted(GetAliveUnitCount(0) + GetAliveUnitCount(1), CombatUpdateInterval);

    UE_LOG(LogTemp, Warning, TEXT("=== FAZA WALKI: Rozpoczęto - jednostki: %d, interwał: %f, partycjonowanie: %s ==="),
        TotalCombatUnits, CombatUpdateInterval, bUseSpatialPartitioning ? TEXT("WŁĄCZONE") : TEXT("WYŁĄCZONE"));
}

/// <summary>
/// Zatrzymuje fazę automatycznej walki i przywraca jednostki do stanu spoczynku.
/// </summary>
void AUnitManager::StopCombatPhase()
{
    UE_LOG(LogTemp, Warning, TEXT("=== WALKA: Zatrzymywanie fazy walki ==="));

    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== WALKA: Klient nie może zatrzymać fazy walki ==="));
        return;
    }

    bCombatPhaseActive = false;

    // Zatrzymanie wszystkich timerów związanych z walką
    GetWorldTimerManager().ClearTimer(CombatUpdateTimer);
    GetWorldTimerManager().ClearTimer(CombatMonitorTimer);

    // Wyczyszczenie siatki przestrzennej z jednostek
    if (bUseSpatialPartitioning && SpatialGrid)
    {
        ClearSpatialGrid();
    }

    // Wyłączenie automatycznej walki dla wszystkich jednostek
    DisableAutoCombatForAllUnits();

    // Odpięcie zdarzeń walki i przywrócenie animacji spoczynku
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit))
        {
            UnbindUnitCombatEvents(UnitData.Unit);
            UnitData.Unit->StopAutoCombat();
            UnitData.Unit->SetAnimationState(EAnimationState::Idle);
        }
    }

    // Replikacja zatrzymania walki do wszystkich klientów
    MulticastCombatStopped();

    UE_LOG(LogTemp, Warning, TEXT("=== WALKA: Faza walki zatrzymana - wszystkie jednostki w stanie spoczynku ==="));
}

/// <summary>
/// Aktualizacja walki z wykorzystaniem siatki przestrzennej
/// Metoda zoptymalizowana używająca partycjonowania przestrzennego dla znacznie lepszej wydajności.
/// </summary>
void AUnitManager::UpdateCombatUsingSpatialGrid()
{
    // Walidacja warunków wstępnych
    if (!HasAuthority() || !bCombatPhaseActive || !SpatialGrid)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== WALKA PRZESTRZENNA: Nie można aktualizować - Autorytet: %s, WalkaAktywna: %s, SiatkaP: %s ==="),
            HasAuthority() ? TEXT("TAK") : TEXT("NIE"),
            bCombatPhaseActive ? TEXT("TAK") : TEXT("NIE"),
            SpatialGrid ? TEXT("ISTNIEJE") : TEXT("NULL"));
        return;
    }

    // Pobranie wszystkich żywych jednostek
    TArray<ABaseUnit*> AliveUnits = GetAliveUnits();

    if (AliveUnits.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== AKTUALIZACJA WALKI: Brak żywych jednostek, zatrzymywanie walki ==="));
        StopCombatPhase();
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== AKTUALIZACJA WALKI: Przetwarzanie %d żywych jednostek za pomocą siatki przestrzennej ==="), AliveUnits.Num());

    // Aktualizacja pozycji jednostek w siatce przestrzennej 
    UpdateSpatialGridPositions();

    ProcessSpatialCombat(AliveUnits);

    // Replikacja aktualizacji walki do klientów dla synchronizacji wizualnej
    MulticastCombatUpdate(AliveUnits);

    // Okresowe logowanie statystyk siatki dla monitorowania wydajności 
    if (GetWorld() && FMath::IsNearlyZero(FMath::Fmod(GetWorld()->GetTimeSeconds(), 5.0f), 0.1f))
    {
        SpatialGrid->DebugPrintGridStats();
    }
}

/// <summary>
/// Przetwarza logikę walki wszystkich jednostek z wykorzystaniem siatki przestrzennej.
/// </summary>
/// <param name="AliveUnits">Tablica wszystkich żywych jednostek do przetworzenia</param>
void AUnitManager::ProcessSpatialCombat(const TArray<ABaseUnit*>& AliveUnits)
{
    if (!SpatialGrid)
    {
        return;
    }

    // Pozwól siatce przestrzennej obsłużyć całą walkę efektywnie
    SpatialGrid->HandleAllCombat();

    // Ulepszone przetwarzanie jednostka-po-jednostce z zapytaniami przestrzennymi
    // Daje więcej kontroli nad indywidualnym zachowaniem jednostek
    for (ABaseUnit* Unit : AliveUnits)
    {
        // Pomijanie nieważnych lub nieaktywnych jednostek
        if (!Unit || !Unit->bIsAlive || !Unit->bAutoCombatEnabled)
        {
            continue;
        }

        ProcessUnitCombatWithSpatialGrid(Unit);
    }
}

/// <summary>
/// Przetwarza logikę walki dla pojedynczej jednostki z wykorzystaniem siatki przestrzennej.
/// </summary>
/// <param name="Unit">Jednostka do przetworzenia</param>
void AUnitManager::ProcessUnitCombatWithSpatialGrid(ABaseUnit* Unit)
{
    if (!Unit || !SpatialGrid || !Unit->bIsAlive || !Unit->bAutoCombatEnabled)
    {
        return;
    }

    // Sprawdzenie czy jednostka ma aktualny cel i może go zaatakować
    if (Unit->HasValidTarget() && Unit->CanAttackTarget(Unit->CurrentTarget))
    {
        UE_LOG(LogTemp, Warning, TEXT("=== WALKA PRZESTRZENNA: Jednostka %s atakuje aktualny cel %s ==="),
            *Unit->GetName(), *Unit->CurrentTarget->GetName());
        Unit->PerformAttack(Unit->CurrentTarget);
        return;
    }

    // Jeśli brak aktualnego celu, użyj siatki przestrzennej do szybkiego znalezienia najbliższego wroga
    if (!Unit->HasValidTarget())
    {
        // Wyszukiwanie tylko w pobliskich komórkach siatki
        // zamiast sprawdzania wszystkich jednostek na mapie
        ABaseUnit* NearestEnemy = SpatialGrid->FindNearestEnemy(Unit, Unit->SearchRange);

        if (NearestEnemy)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== WALKA PRZESTRZENNA: Jednostka %s znalazła nowy cel %s za pomocą siatki ==="),
                *Unit->GetName(), *NearestEnemy->GetName());

            Unit->SetTarget(NearestEnemy);
            Unit->MoveTowardsTarget(NearestEnemy);
        }
        else
        {
            // Brak wrogów w zasięgu, powrót do stanu spoczynku
            Unit->ClearTarget();
            Unit->SetAnimationState(EAnimationState::Idle);
        }
    }
}

/// <summary>
/// Aktualizacja walki wszystkich jednostek
/// Oryginalna metoda bez optymalizacji przestrzennej.
/// </summary>
void AUnitManager::UpdateAllUnitsCombat()
{
    if (!HasAuthority() || !bCombatPhaseActive)
        return;

    // Pobranie wszystkich jednostek
    TArray<ABaseUnit*> AllUnits = GetAllUnits();
    TArray<ABaseUnit*> AliveUnits = GetAliveUnits();

    if (AliveUnits.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== AKTUALIZACJA WALKI: Brak żywych jednostek, zatrzymywanie ==="));
        StopCombatPhase();
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== AKTUALIZACJA WALKI: Aktualizacja dla %d jednostek (Metoda Podstawowa O(n²)) ==="), AliveUnits.Num());

    // Aktualizacja zachowania bojowego każdej jednostki
    for (ABaseUnit* Unit : AliveUnits)
    {
        if (Unit && Unit->bAutoCombatEnabled)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== AKTUALIZACJA WALKI: Przetwarzanie jednostki %s na pozycji (%f,%f) ==="),
                *Unit->GetName(), Unit->GridPosition.X, Unit->GridPosition.Y);
            Unit->FindAndAttackNearestEnemy(AliveUnits);
        }
    }

    // Replikacja aktualizacji do klientów
    MulticastCombatUpdate(AliveUnits);
}

/// <summary>
/// Monitoruje zdarzenia walki i obsługuje usuwanie martwych jednostek.
/// </summary>
void AUnitManager::MonitorCombatEvents()
{
    if (!HasAuthority() || !bCombatPhaseActive)
        return;

    // Zebranie wszystkich martwych jednostek
    TArray<ABaseUnit*> UnitsToRemove;

    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && !UnitData.Unit->bIsAlive)
        {
            UnitsToRemove.Add(UnitData.Unit);
        }
    }

    // Obsługa każdej martwej jednostki
    for (ABaseUnit* DeadUnit : UnitsToRemove)
    {
        HandleUnitDeath(DeadUnit);
    }
}

/// <summary>
/// Obsługuje wszystkie operacje związane ze śmiercią jednostki.
/// </summary>
/// <param name="DeadUnit">Jednostka która zginęła</param>
void AUnitManager::HandleUnitDeath(ABaseUnit* DeadUnit)
{
    if (!HasAuthority() || !DeadUnit)
        return;

    UE_LOG(LogTemp, Warning, TEXT("=== ŚMIERĆ W WALCE: Jednostka %s zginęła ==="), *DeadUnit->GetName());

    // Usunięcie z siatki przestrzennej
    if (bUseSpatialPartitioning && SpatialGrid)
    {
        SpatialGrid->RemoveUnit(DeadUnit);
        UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Usunięto martwą jednostkę %s z siatki ==="), *DeadUnit->GetName());
    }

    // Usunięcie z tablicy optymalizacji pamięci
    RemoveUnitFromCombatArray(DeadUnit);

    // Odpięcie zdarzeń walki
    UnbindUnitCombatEvents(DeadUnit);

    // Replikacja zdarzenia śmierci do wszystkich klientów
    MulticastUnitKilled(DeadUnit);

    // Broadcast zdarzenia śmierci dla UI, statystyk, achievementów etc.
    OnUnitKilled.Broadcast(DeadUnit);

    // Sprawdzenie czy walka powinna się zakończyć
    CheckCombatEndConditions();
}

/// <summary>
/// Sprawdza warunki zakończenia walki.
/// </summary>
void AUnitManager::CheckCombatEndConditions()
{
    if (!HasAuthority() || !bCombatPhaseActive)
        return;

    // Zliczenie żywych jednostek każdego gracza
    int32 Player0AliveCount = GetAliveUnitCount(0);
    int32 Player1AliveCount = GetAliveUnitCount(1);

    UE_LOG(LogTemp, Warning, TEXT("=== SPRAWDZENIE WALKI: Gracz 0: %d żywe, Gracz 1: %d żywe ==="),
        Player0AliveCount, Player1AliveCount);

    // Zakończenie walki jeśli jedna strona nie ma jednostek
    if (Player0AliveCount == 0 || Player1AliveCount == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== KONIEC WALKI: Zakończenie - jedna strona wyeliminowana ==="));
        StopCombatPhase();
    }
    else if (Player0AliveCount + Player1AliveCount < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== KONIEC WALKI: Zakończenie - niewystarczająca liczba jednostek ==="));
        StopCombatPhase();
    }
}

/// <summary>
/// Zwraca liczbę żywych jednostek dla określonego gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>Liczba żywych jednostek gracza</returns>
int32 AUnitManager::GetAliveUnitCount(int32 PlayerID) const
{
    int32 Count = 0;
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.PlayerID == PlayerID && UnitData.Unit && UnitData.Unit->bIsAlive)
        {
            Count++;
        }
    }
    return Count;
}

/// <summary>
/// Zwraca tablicę wszystkich żywych jednostek.
/// </summary>
/// <returns>Tablica wskaźników do żywych jednostek</returns>
TArray<ABaseUnit*> AUnitManager::GetAliveUnits() const
{
    TArray<ABaseUnit*> AliveUnits;

    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit) && UnitData.Unit->bIsAlive)
        {
            AliveUnits.Add(UnitData.Unit);
        }
    }

    return AliveUnits;
}

/// <summary>
/// Zwraca tablicę wszystkich jednostek (żywych i martwych).
/// </summary>
/// <returns>Tablica wskaźników do wszystkich jednostek</returns>
TArray<ABaseUnit*> AUnitManager::GetAllUnits() const
{
    TArray<ABaseUnit*> AllUnits;

    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit))
        {
            AllUnits.Add(UnitData.Unit);
        }
    }

    return AllUnits;
}

/// <summary>
/// Inicjalizuje system optymalizacji pamięci podręcznej dla wydajnej walki.
/// </summary>
void AUnitManager::InitializeCombatDataLocality()
{
    // Tylko serwer inicjalizuje system optymalizacji
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Klient pomija inicjalizację ==="));
        return;
    }

    if (!GridManagerRef)
    {
        UE_LOG(LogTemp, Error, TEXT("=== OPTYMALIZACJA CACHE: Nie można zainicjalizować - GridManagerRef jest null ==="));
        return;
    }

    // Obliczenie maksymalnej liczby jednostek bojowych z pozycji spawnu obu graczy
    TArray<FVector2D> Player0SpawnPositions = GridManagerRef->GetValidSpawnPositions(0);
    TArray<FVector2D> Player1SpawnPositions = GridManagerRef->GetValidSpawnPositions(1);

    MaxCombatUnits = Player0SpawnPositions.Num() + Player1SpawnPositions.Num();

    // Pre-alokacja tablicy jednostek bojowych dla optymalnej wydajności
    // Wszystkie jednostki będą obok siebie w pamięci
    CombatUnitsArray.Empty(MaxCombatUnits);
    CombatUnitsArray.SetNumZeroed(MaxCombatUnits);
    ActiveCombatUnitsCount = 0;

    UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Zainicjalizowano tablicę z pojemnością: %d (Gracz0: %d, Gracz1: %d) ==="),
        MaxCombatUnits, Player0SpawnPositions.Num(), Player1SpawnPositions.Num());
}

/// <summary>
/// Dodaje jednostkę do spakowanej tablicy optymalizacji cache.
/// </summary>
/// <param name="Unit">Jednostka do dodania</param>
void AUnitManager::AddUnitToCombatArray(ABaseUnit* Unit)
{
    if (!HasAuthority() || !Unit || !Unit->bIsAlive)
    {
        return;
    }

    // Sprawdzenie czy tablica jest zainicjalizowana
    if (CombatUnitsArray.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Tablica niezainicjalizowana, inicjalizacja teraz ==="));
        InitializeCombatDataLocality();
    }

    if (CombatUnitsArray.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("=== OPTYMALIZACJA CACHE: Nie udało się zainicjalizować tablicy ==="));
        return;
    }

    // Znalezienie pierwszego wolnego slotu w tablicy
    for (int32 i = 0; i < MaxCombatUnits; i++)
    {
        if (CombatUnitsArray[i] == nullptr)
        {
            CombatUnitsArray[i] = Unit;
            ActiveCombatUnitsCount++;

            UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Dodano jednostkę %s pod indeksem %d (aktywne: %d) ==="),
                *Unit->GetName(), i, ActiveCombatUnitsCount);
            return;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("=== OPTYMALIZACJA CACHE: Nie udało się dodać jednostki %s - tablica pełna ==="),
        *Unit->GetName());
}

/// <summary>
/// Usuwa jednostkę ze spakowanej tablicy optymalizacji cache.
/// </summary>
/// <param name="Unit">Jednostka do usunięcia</param>
void AUnitManager::RemoveUnitFromCombatArray(ABaseUnit* Unit)
{
    if (!HasAuthority() || !Unit)
    {
        return;
    }

    if (CombatUnitsArray.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Tablica niezainicjalizowana, nie można usunąć jednostki %s ==="),
            *Unit->GetName());
        return;
    }

    // Znalezienie i usunięcie jednostki z tablicy
    for (int32 i = 0; i < MaxCombatUnits; i++)
    {
        if (CombatUnitsArray[i] == Unit)
        {
            CombatUnitsArray[i] = nullptr;
            ActiveCombatUnitsCount--;

            UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Usunięto jednostkę %s z indeksu %d (aktywne: %d) ==="),
                *Unit->GetName(), i, ActiveCombatUnitsCount);
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Jednostka %s nie znaleziona w tablicy ==="),
        *Unit->GetName());
}

/// <summary>
/// Pakuje aktywne jednostki na początek tablicy dla maksymalnej wydajności cache.
/// </summary>
void AUnitManager::UpdateCombatArrayOrder()
{
    if (!HasAuthority())
    {
        return;
    }

    if (CombatUnitsArray.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Tablica niezainicjalizowana, nie można zaktualizować kolejności ==="));
        return;
    }

    // Pakowanie aktywnych jednostek na początek tablicy
    int32 WriteIndex = 0;

    for (int32 i = 0; i < MaxCombatUnits; i++)
    {
        if (CombatUnitsArray[i] != nullptr && CombatUnitsArray[i]->bIsAlive)
        {
            // Przeniesienie aktywnej jednostki na początek jeśli nie jest już tam
            if (WriteIndex != i)
            {
                CombatUnitsArray[WriteIndex] = CombatUnitsArray[i];
                CombatUnitsArray[i] = nullptr;
            }
            WriteIndex++;
        }
        else if (CombatUnitsArray[i] != nullptr && !CombatUnitsArray[i]->bIsAlive)
        {
            // Usunięcie martwych jednostek
            CombatUnitsArray[i] = nullptr;
        }
    }

    ActiveCombatUnitsCount = WriteIndex;

    UE_LOG(LogTemp, Warning, TEXT("=== OPTYMALIZACJA CACHE: Zaktualizowano kolejność - aktywne jednostki: %d zapakowane na początku tablicy ==="),
        ActiveCombatUnitsCount);
}

/// <summary>
/// Włącza automatyczną walkę dla wszystkich jednostek z optymalizacją cache.
/// </summary>
void AUnitManager::EnableAutoCombatForAllUnits()
{
    // Sprawdzenie czy tablica walki jest zainicjalizowana
    if (CombatUnitsArray.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== WŁĄCZANIE WALKI: Tablica walki niezainicjalizowana, powrót do metody podstawowej ==="));

        // Fallback do oryginalnej metody
        for (const FSpawnedUnitData& UnitData : SpawnedUnits)
        {
            if (UnitData.Unit && IsValid(UnitData.Unit) && UnitData.Unit->bIsAlive)
            {
                UnitData.Unit->StartAutoCombat();
            }
        }
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== WŁĄCZANIE WALKI: Włączanie auto-walki z optymalizacją cache dla %d aktywnych jednostek ==="),
        ActiveCombatUnitsCount);

    // Aktualizacja kolejności w tablicy aby zapakować aktywne jednostki na początku
    UpdateCombatArrayOrder();

    // Iteracja przez ciągłą tablicę aktywnych jednostek
    for (int32 i = 0; i < ActiveCombatUnitsCount; i++)
    {
        ABaseUnit* Unit = CombatUnitsArray[i];
        if (Unit && IsValid(Unit) && Unit->bIsAlive)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== WŁĄCZANIE WALKI: Włączanie auto-walki dla jednostki %s (indeks cache: %d) ==="),
                *Unit->GetName(), i);
            Unit->StartAutoCombat();
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== WŁĄCZANIE WALKI: Optymalizacja cache - przetworzono %d jednostek w ciągłej pamięci ==="),
        ActiveCombatUnitsCount);
}

/// <summary>
/// Wyłącza automatyczną walkę dla wszystkich jednostek z optymalizacją cache.
/// </summary>
void AUnitManager::DisableAutoCombatForAllUnits()
{
    // Sprawdzenie czy tablica walki jest zainicjalizowana
    if (CombatUnitsArray.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== WYŁĄCZANIE WALKI: Tablica walki niezainicjalizowana, powrót do metody podstawowej ==="));

        // Fallback do oryginalnej metody
        for (const FSpawnedUnitData& UnitData : SpawnedUnits)
        {
            if (UnitData.Unit && IsValid(UnitData.Unit))
            {
                UnitData.Unit->StopAutoCombat();
            }
        }
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== WYŁĄCZANIE WALKI: Wyłączanie auto-walki z optymalizacją cache dla %d aktywnych jednostek ==="),
        ActiveCombatUnitsCount);

    // Iteracja przez ciągłą tablicę aktywnych jednostek
    for (int32 i = 0; i < ActiveCombatUnitsCount; i++)
    {
        ABaseUnit* Unit = CombatUnitsArray[i];
        if (Unit && IsValid(Unit))
        {
            UE_LOG(LogTemp, Warning, TEXT("=== WYŁĄCZANIE WALKI: Wyłączanie auto-walki dla jednostki %s (indeks cache: %d) ==="),
                *Unit->GetName(), i);
            Unit->StopAutoCombat();
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== WYŁĄCZANIE WALKI: Optymalizacja cache - przetworzono %d jednostek w ciągłej pamięci ==="),
        ActiveCombatUnitsCount);
}

/// <summary>
/// Tworzy nową jednostkę dla określonego gracza.
/// </summary>
/// <param name="PlayerID">ID gracza (0 lub 1)</param>
/// <param name="UnitType">Typ jednostki do utworzenia</param>
/// <returns>Wskaźnik do utworzonej jednostki lub nullptr jeśli się nie powiodło</returns>
ABaseUnit* AUnitManager::SpawnUnitForPlayer(int32 PlayerID, EBaseUnitType UnitType)
{
    // Tylko serwer może tworzyć jednostki
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("Klient próbował utworzyć jednostkę"));
        return nullptr;
    }

    // Sprawdzenie czy GridManager jest dostępny
    if (!GridManagerRef)
    {
        InitializeGridManager();
        if (!GridManagerRef)
        {
            UE_LOG(LogTemp, Error, TEXT("Brak GridManagera"));
            return nullptr;
        }
    }

    // Znalezienie pierwszej wolnej pozycji do spawnu
    FVector2D SpawnPosition = FindFirstFreeSpawnPosition(PlayerID);
    if (SpawnPosition == FVector2D(-1, -1))
    {
        UE_LOG(LogTemp, Warning, TEXT("Brak wolnej pozycji spawnu dla Gracza %d"), PlayerID);
        return nullptr;
    }

    // Pobranie klasy jednostki na podstawie typu
    TSubclassOf<ABaseUnit> UnitClass = GetUnitClassByType(UnitType);
    if (!UnitClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Klasa jednostki nie ustawiona dla typu %d"), (int32)UnitType);
        return nullptr;
    }

    // Obliczenie pozycji świata i podniesienie nad podłoże
    FVector WorldLocation = GetWorldLocationFromGrid(SpawnPosition);
    WorldLocation.Z += 20.0f;

    // Obliczenie rotacji - Gracz 1 obrócony o 180 stopni
    FRotator SpawnRotation = FRotator::ZeroRotator;
    if (PlayerID == 1)
    {
        SpawnRotation.Yaw = 180.0f;
        UE_LOG(LogTemp, Warning, TEXT("=== SPAWN: Gracz 1 - jednostka obrócona o 180 stopni ==="));
    }
    else
    {
        SpawnRotation.Yaw = 0.0f;
        UE_LOG(LogTemp, Warning, TEXT("=== SPAWN: Gracz 0 - domyślna rotacja ==="));
    }

    // Parametry spawnu z automatycznym dostosowaniem pozycji przy kolizji
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // Utworzenie aktora jednostki w świecie
    ABaseUnit* SpawnedUnit = GetWorld()->SpawnActor<ABaseUnit>(UnitClass, WorldLocation, SpawnRotation, SpawnParams);

    if (SpawnedUnit)
    {
        // Ustawienie podstawowych właściwości jednostki
        SpawnedUnit->TeamID = PlayerID;
        SpawnedUnit->GridPosition = SpawnPosition;
        SpawnedUnit->UnitType = UnitType;

        // Dodanie do tablicy utworzonych jednostek
        FSpawnedUnitData UnitData;
        UnitData.Unit = SpawnedUnit;
        UnitData.PlayerID = PlayerID;
        UnitData.GridPosition = SpawnPosition;
        UnitData.UnitType = UnitType;
        SpawnedUnits.Add(UnitData);

        // Dodanie do siatki przestrzennej jeśli walka jest aktywna
        if (bUseSpatialPartitioning && SpatialGrid && bCombatPhaseActive)
        {
            SpatialGrid->AddUnit(SpawnedUnit);
            UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Dodano utworzoną jednostkę %s do siatki ==="),
                *SpawnedUnit->GetName());
        }

        // Dodanie do tablicy optymalizacji cache
        AddUnitToCombatArray(SpawnedUnit);

        // Jeśli walka jest aktywna, podłączenie zdarzeń i uruchomienie auto-walki
        if (bCombatPhaseActive)
        {
            BindUnitCombatEvents(SpawnedUnit);
            SpawnedUnit->StartAutoCombat();
            UE_LOG(LogTemp, Warning, TEXT("=== SPAWN: Nowa jednostka %s dodana do aktywnej walki ==="),
                *SpawnedUnit->GetName());
        }

        // Broadcast zdarzeń spawnu 
        OnUnitSpawned.Broadcast(SpawnedUnit, PlayerID, SpawnPosition);
        MulticastUnitSpawned(SpawnedUnit, PlayerID, SpawnPosition, UnitType);

        UE_LOG(LogTemp, Warning, TEXT("Serwer utworzył jednostkę typu %d dla Gracza %d z rotacją %f stopni"),
            (int32)UnitType, PlayerID, SpawnRotation.Yaw);
        return SpawnedUnit;
    }

    UE_LOG(LogTemp, Error, TEXT("Nie udało się utworzyć jednostki"));
    return nullptr;
}

/// <summary>
/// Przenosi jednostkę na nową pozycję siatki.
/// </summary>
/// <param name="Unit">Jednostka do przeniesienia</param>
/// <param name="NewGridPosition">Nowa pozycja na siatce</param>
/// <param name="PlayerID">ID gracza wykonującego ruch</param>
/// <returns>True jeśli ruch się powiódł, false w przeciwnym razie</returns>
bool AUnitManager::MoveUnitToPosition(ABaseUnit* Unit, FVector2D NewGridPosition, int32 PlayerID)
{
    // Tylko serwer może przenosić jednostki
    if (!HasAuthority())
    {
        return false;
    }

    // Walidacja możliwości ruchu
    if (!Unit || !CanMoveUnitToPosition(Unit, NewGridPosition, PlayerID))
    {
        return false;
    }

    // Znalezienie danych jednostki
    FSpawnedUnitData* UnitData = FindUnitData(Unit);
    if (!UnitData)
    {
        return false;
    }

    // Zapisanie starych pozycji
    FVector2D OldPosition = UnitData->GridPosition;
    FVector OldWorldPosition = Unit->GetActorLocation();

    // Aktualizacja danych na serwerze
    UnitData->GridPosition = NewGridPosition;
    Unit->GridPosition = NewGridPosition;

    // Przeniesienie jednostki w świecie
    FVector NewWorldLocation = GetWorldLocationFromGrid(NewGridPosition);
    NewWorldLocation.Z = Unit->GetActorLocation().Z;
    Unit->SetActorLocation(NewWorldLocation);

    // Aktualizacja pozycji w siatce przestrzennej
    if (bUseSpatialPartitioning && SpatialGrid)
    {
        SpatialGrid->UpdateUnitPosition(Unit, OldWorldPosition, NewWorldLocation);
        UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Zaktualizowano pozycję jednostki %s w siatce ==="),
            *Unit->GetName());
    }

    // Ukrycie starego podświetlenia
    HideAllHighlights();

    // Wywołanie zdarzeń lokalnych
    OnUnitMoved.Broadcast(Unit, OldPosition, NewGridPosition, PlayerID);

    // Wysłanie RPC ruchu do wszystkich klientów
    MulticastUnitMovedByPosition(OldPosition, NewGridPosition, PlayerID);

    // Wysłanie RPC podświetlenia tylko do gracza który wykonał ruch
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        if (AStrategyPlayerController* StratPC = Cast<AStrategyPlayerController>(Iterator->Get()))
        {
            if (StratPC->GetPlayerID() == PlayerID)
            {
                StratPC->ClientShowUnitSelectionByPosition(NewGridPosition, PlayerID);
                break;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== SERWER: Jednostka przesunięta pomyślnie ==="));
    return true;
}

/// <summary>
/// Usuwa jednostkę z gry wraz z czyszczeniem wszystkich referencji.
/// </summary>
/// <param name="Unit">Jednostka do usunięcia</param>
void AUnitManager::RemoveUnit(ABaseUnit* Unit)
{
    if (!HasAuthority() || !Unit)
        return;

    // Usunięcie z siatki przestrzennej
    if (bUseSpatialPartitioning && SpatialGrid)
    {
        SpatialGrid->RemoveUnit(Unit);
        UE_LOG(LogTemp, Warning, TEXT("=== SIATKA PRZESTRZENNA: Usunięto jednostkę %s z siatki ==="),
            *Unit->GetName());
    }

    // Usunięcie z tablicy optymalizacji cache
    RemoveUnitFromCombatArray(Unit);

    // Odpięcie zdarzeń walki
    if (bCombatPhaseActive)
    {
        UnbindUnitCombatEvents(Unit);
    }

    // Usunięcie z głównej tablicy jednostek
    RemoveUnitFromArray(Unit);

    // Odznaczenie jeśli była zaznaczona
    if (SelectedUnit == Unit)
    {
        DeselectUnit();
    }

    // Replikacja usunięcia do klientów
    MulticastUnitRemoved(Unit);

    // Zniszczenie aktora
    Unit->Destroy();

    // Sprawdzenie warunków zakończenia walki
    if (bCombatPhaseActive)
    {
        CheckCombatEndConditions();
    }

    UE_LOG(LogTemp, Log, TEXT("Serwer usunął jednostkę z gry"));
}

/// <summary>
/// Wyświetla statystyki i wizualizację siatki podziału przestrzennego.
/// </summary>
void AUnitManager::DebugSpatialGrid()
{
    if (SpatialGrid)
    {
        // Wydrukuj statystyki siatki do konsoli
        SpatialGrid->DebugPrintGridStats();

        if (GetWorld())
        {
            // Narysuj siatkę w świecie gry na 5 sekund
            SpatialGrid->DebugDrawGrid(GetWorld(), 5.0f);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("=== DEBUG: Siatka przestrzenna nie została zainicjalizowana ==="));
    }
}

/// <summary>
/// Przełącza włączenie/wyłączenie systemu podziału przestrzennego.
/// </summary>
void AUnitManager::ToggleSpatialPartitioning()
{
    if (HasAuthority())
    {
        // Przełącz flagę
        bUseSpatialPartitioning = !bUseSpatialPartitioning;

        // Jeśli włączamy i siatka nie istnieje, zainicjalizuj ją
        if (bUseSpatialPartitioning && !SpatialGrid)
        {
            InitializeSpatialGrid();
        }

        UE_LOG(LogTemp, Warning, TEXT("=== PODZIAŁ PRZESTRZENNY: %s ==="),
            bUseSpatialPartitioning ? TEXT("WŁĄCZONY") : TEXT("WYŁĄCZONY"));
    }
}

/// <summary>
/// Inicjalizacja GridManager.
/// </summary>

void AUnitManager::InitializeGridManager()
{
    TArray<AActor*> FoundManagers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGridManager::StaticClass(), FoundManagers);

    if (FoundManagers.Num() > 0)
    {
        GridManagerRef = Cast<AGridManager>(FoundManagers[0]);
        if (GridManagerRef)
        {
            UE_LOG(LogTemp, Log, TEXT("GridManager found and initialized in UnitManager."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GridManager not found. Retrying in 1 second."));
        RetryInitializeGridManager();
    }
}

/// <summary>
/// Ustawia timer do ponownej próby inicjalizacji GridManager.
/// </summary>
void AUnitManager::RetryInitializeGridManager()
{
    GetWorldTimerManager().SetTimer(
        RetryInitializeGridManagerHandle,
        FTimerDelegate::CreateUObject(this, &AUnitManager::DelayedGridManagerInit),
        1.0f,  
        false  
    );
}

/// <summary>
/// Opóźniona inicjalizacja GridManager.
/// </summary>
void AUnitManager::DelayedGridManagerInit()
{
    InitializeGridManager();
}


/// <summary>
/// Podpina zdarzenia walki dla konkretnej jednostki.
/// </summary>
/// <param name="Unit">Jednostka, dla której bindujemy zdarzenia</param>
void AUnitManager::BindUnitCombatEvents(ABaseUnit* Unit)
{
    if (!Unit || !HasAuthority())
        return;

    // Podpnij zdarzenie śmierci jednostki
    Unit->OnUnitDied.AddDynamic(this, &AUnitManager::OnUnitDeathEvent);

    // Podpnij zdarzenie otrzymania obrażeń
    Unit->OnUnitDamaged.AddDynamic(this, &AUnitManager::OnUnitDamagedEvent);

    UE_LOG(LogTemp, Warning, TEXT("=== ZDARZENIA WALKI: Podpięto zdarzenia dla jednostki %s ==="), *Unit->GetName());
}

/// <summary>
/// Odpina zdarzenia walki dla konkretnej jednostki.
/// </summary>
/// <param name="Unit">Jednostka, dla której odpinamy zdarzenia</param>
void AUnitManager::UnbindUnitCombatEvents(ABaseUnit* Unit)
{
    if (!Unit || !HasAuthority())
        return;

    // Odepnij zdarzenie śmierci jednostki
    Unit->OnUnitDied.RemoveDynamic(this, &AUnitManager::OnUnitDeathEvent);

    // Odepnij zdarzenie otrzymania obrażeń
    Unit->OnUnitDamaged.RemoveDynamic(this, &AUnitManager::OnUnitDamagedEvent);

    UE_LOG(LogTemp, Warning, TEXT("=== ZDARZENIA WALKI: Odpięto zdarzenia dla jednostki %s ==="), *Unit->GetName());
}

/// <summary>
/// Callback wywoływany gdy jednostka umiera.
/// </summary>
/// <param name="DeadUnit">Jednostka, która umarła</param>
void AUnitManager::OnUnitDeathEvent(ABaseUnit* DeadUnit)
{
    if (HasAuthority())
    {
        HandleUnitDeath(DeadUnit);
    }
}

/// <summary>
/// Callback wywoływany gdy jednostka otrzymuje obrażenia.
/// </summary>
/// <param name="DamagedUnit">Jednostka, która otrzymała obrażenia</param>
/// <param name="Damage">Ilość otrzymanych obrażeń</param>
void AUnitManager::OnUnitDamagedEvent(ABaseUnit* DamagedUnit, int32 Damage)
{
    if (!HasAuthority())
        return;

    UE_LOG(LogTemp, Warning, TEXT("=== ZDARZENIE WALKI: Jednostka %s otrzymała %d obrażeń ==="),
        *DamagedUnit->GetName(), Damage);

    // Wyemituj zdarzenie ataku (atakujący jest nullptr - można to rozbudować)
    OnUnitAttacked.Broadcast(nullptr, DamagedUnit, Damage);
}


/// <summary>
/// Multicast RPC informujący wszystkich klientów o rozpoczęciu fazy walki.
/// </summary>
/// <param name="PlayerCount">Liczba jednostek uczestniczących w walce</param>
/// <param name="UpdateInterval">Interwał aktualizacji walki w sekundach</param>
void AUnitManager::MulticastCombatStarted_Implementation(int32 PlayerCount, float UpdateInterval)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST DEBUG WALKI: Otrzymano MulticastCombatStarted ==="));
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST DEBUG WALKI: LiczbaGraczy: %d, InterwałAktualizacji: %f ==="), PlayerCount, UpdateInterval);
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST DEBUG WALKI: HasAuthority: %s ==="), HasAuthority() ? TEXT("SERWER") : TEXT("KLIENT"));

    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== DEBUG WALKI KLIENTA: Przetwarzanie rozpoczęcia walki na kliencie ==="));

        // Ustaw stan walki na kliencie
        bCombatPhaseActive = true;
        CombatUpdateInterval = UpdateInterval;
        TotalCombatUnits = PlayerCount;

        // Zaloguj stan jednostek po stronie klienta
        UE_LOG(LogTemp, Warning, TEXT("=== DEBUG WALKI KLIENTA: Klient ma %d jednostek w tablicy ==="), SpawnedUnits.Num());
        for (int32 i = 0; i < SpawnedUnits.Num(); i++)
        {
            const FSpawnedUnitData& UnitData = SpawnedUnits[i];
            if (UnitData.Unit && IsValid(UnitData.Unit))
            {
                FVector UnitPos = UnitData.Unit->GetActorLocation();
                UE_LOG(LogTemp, Warning, TEXT("=== DEBUG WALKI KLIENTA: Jednostka %d - %s na pozycji świata (%f,%f,%f) ==="),
                    i, *UnitData.Unit->GetName(), UnitPos.X, UnitPos.Y, UnitPos.Z);
            }
        }

        // Wyemituj zdarzenie rozpoczęcia walki dla klientów
        OnCombatStarted.Broadcast(PlayerCount, UpdateInterval);

        UE_LOG(LogTemp, Warning, TEXT("=== DEBUG WALKI KLIENTA: Faza walki rozpoczęta na kliencie ==="));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("=== DEBUG WALKI SERWERA: Serwer otrzymał własny multicast (normalne) ==="));
    }
}

/// <summary>
/// Multicast RPC informujący wszystkich klientów o zakończeniu fazy walki.
/// </summary>
void AUnitManager::MulticastCombatStopped_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST: Walka zatrzymana ==="));

    if (!HasAuthority())
    {
        // Zresetuj stan walki na kliencie
        bCombatPhaseActive = false;
        TotalCombatUnits = 0;

        // Wyemituj zdarzenie zakończenia walki dla klientów
        OnCombatStopped.Broadcast();

        UE_LOG(LogTemp, Warning, TEXT("=== KLIENT: Faza walki zatrzymana ==="));
    }
}

/// <summary>
/// Multicast RPC informujący o ataku jednostki.
/// </summary>
/// <param name="Attacker">Jednostka atakująca</param>
/// <param name="Target">Jednostka atakowana</param>
/// <param name="Damage">Zadane obrażenia</param>
void AUnitManager::MulticastUnitAttacked_Implementation(ABaseUnit* Attacker, ABaseUnit* Target, int32 Damage)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST: Atak jednostki - Atakujący: %s, Cel: %s, Obrażenia: %d ==="),
        Attacker ? *Attacker->GetName() : TEXT("NIEZNANY"),
        Target ? *Target->GetName() : TEXT("NIEZNANY"),
        Damage);

    if (!HasAuthority())
    {
        // Wyemituj zdarzenie ataku na klientach dla aktualizacji UI
        OnUnitAttacked.Broadcast(Attacker, Target, Damage);
    }
}

/// <summary>
/// Multicast RPC informujący o śmierci jednostki.
/// </summary>
/// <param name="DeadUnit">Jednostka, która zginęła</param>
void AUnitManager::MulticastUnitKilled_Implementation(ABaseUnit* DeadUnit)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST: Jednostka zabita - %s ==="),
        DeadUnit ? *DeadUnit->GetName() : TEXT("NIEZNANY"));

    if (!HasAuthority())
    {
        // Wyemituj zdarzenie śmierci na klientach dla aktualizacji UI
        OnUnitKilled.Broadcast(DeadUnit);

        UE_LOG(LogTemp, Warning, TEXT("=== KLIENT: Zdarzenie śmierci jednostki %s przetworzone ==="),
            DeadUnit ? *DeadUnit->GetName() : TEXT("NIEZNANY"));
    }
}

/// <summary>
/// Multicast RPC wysyłający aktualizację stanu walki.
/// </summary>
/// <param name="CombatUnits">Tablica jednostek uczestniczących w walce</param>
void AUnitManager::MulticastCombatUpdate_Implementation(const TArray<ABaseUnit*>& CombatUnits)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MULTICAST: Aktualizacja walki dla %d jednostek ==="), CombatUnits.Num());

    if (!HasAuthority())
    {
        // Aktualizuj stan walki po stronie klienta jeśli potrzeba
        for (ABaseUnit* Unit : CombatUnits)
        {
            if (Unit && IsValid(Unit))
            {
                UE_LOG(LogTemp, Warning, TEXT("=== AKTUALIZACJA WALKI KLIENTA: Jednostka %s - Zdrowie: %d, Cel: %s ==="),
                    *Unit->GetName(),
                    Unit->CurrentHealth,
                    Unit->CurrentTarget ? *Unit->CurrentTarget->GetName() : TEXT("BRAK"));
            }
        }
    }
}

/// <summary>
/// Znajduje dane jednostki w tablicy SpawnedUnits.
/// </summary>
/// <param name="Unit">Jednostka do znalezienia</param>
/// <returns>Wskaźnik do danych jednostki lub nullptr jeśli nie znaleziono</returns>
FSpawnedUnitData* AUnitManager::FindUnitData(ABaseUnit* Unit)
{
    for (FSpawnedUnitData& Data : SpawnedUnits)
    {
        if (Data.Unit == Unit)
        {
            return &Data;
        }
    }
    return nullptr;
}

/// <summary>
/// Sprawdza czy jednostka może być przeniesiona na podaną pozycję.
/// </summary>
/// <param name="Unit">Jednostka do przeniesienia</param>
/// <param name="NewGridPosition">Nowa pozycja na siatce</param>
/// <param name="PlayerID">ID gracza właściciela jednostki</param>
/// <returns>True jeśli ruch jest możliwy, false wpp</returns>
bool AUnitManager::CanMoveUnitToPosition(ABaseUnit* Unit, FVector2D NewGridPosition, int32 PlayerID) const
{
    if (!Unit || !GridManagerRef)
        return false;

    // Sprawdź czy pozycja jest w granicach siatki
    if (!GridManagerRef->IsValidGridPosition(NewGridPosition))
        return false;

    // Sprawdź czy pozycja należy do strefy spawnu gracza
    if (!IsValidPlayerSpawnPosition(NewGridPosition, PlayerID))
        return false;

    // Sprawdź czy pozycja jest zajęta przez inną jednostkę
    if (IsPositionOccupied(NewGridPosition))
    {
        ABaseUnit* UnitAtPosition = GetUnitAtPosition(NewGridPosition);
        if (UnitAtPosition != Unit)
            return false;
    }

    return true;
}

/// <summary>
/// Znajduje pierwszą wolną pozycję spawnu dla danego gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>Współrzędne wolnej pozycji lub (-1,-1) jeśli nie znaleziono</returns>
FVector2D AUnitManager::FindFirstFreeSpawnPosition(int32 PlayerID) const
{
    if (!GridManagerRef)
        return FVector2D(-1, -1);

    TArray<FVector2D> ValidPositions = GridManagerRef->GetValidSpawnPositions(PlayerID);

    for (const FVector2D& Position : ValidPositions)
    {
        if (!IsPositionOccupied(Position))
        {
            return Position;
        }
    }

    return FVector2D(-1, -1);
}

/// <summary>
/// Sprawdza czy pozycja na siatce jest zajęta przez jednostkę.
/// </summary>
/// <param name="GridPosition">Pozycja do sprawdzenia</param>
/// <returns>True jeśli pozycja jest zajęta, false w przeciwnym razie</returns>
bool AUnitManager::IsPositionOccupied(FVector2D GridPosition) const
{
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && UnitData.GridPosition == GridPosition)
        {
            return true;
        }
    }
    return false;
}

/// <summary>
/// Zwraca jednostkę znajdującą się na podanej pozycji siatki.
/// </summary>
/// <param name="GridPosition">Pozycja do sprawdzenia</param>
/// <returns>Wskaźnik do jednostki lub nullptr jeśli pozycja jest pusta</returns>
ABaseUnit* AUnitManager::GetUnitAtPosition(FVector2D GridPosition) const
{
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && UnitData.GridPosition == GridPosition)
        {
            return UnitData.Unit;
        }
    }
    return nullptr;
}

/// <summary>
/// Zwraca wszystkie jednostki należące do konkretnego gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>Tablica jednostek gracza</returns>
TArray<ABaseUnit*> AUnitManager::GetPlayerUnits(int32 PlayerID) const
{
    TArray<ABaseUnit*> PlayerUnits;

    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && UnitData.PlayerID == PlayerID)
        {
            PlayerUnits.Add(UnitData.Unit);
        }
    }

    return PlayerUnits;
}

/// <summary>
/// Usuwa jednostkę z tablicy SpawnedUnits.
/// Przeszukuje tablicę od końca i usuwa pierwszy znaleziony wpis.
/// </summary>
/// <param name="Unit">Jednostka do usunięcia</param>
void AUnitManager::RemoveUnitFromArray(ABaseUnit* Unit)
{
    for (int32 i = SpawnedUnits.Num() - 1; i >= 0; i--)
    {
        if (SpawnedUnits[i].Unit == Unit)
        {
            SpawnedUnits.RemoveAt(i);
            break;
        }
    }
}

/// <summary>
/// Zaznacza jednostkę dla danego gracza.
/// </summary>
/// <param name="Unit">Jednostka do zaznaczenia</param>
/// <param name="PlayerID">ID gracza zaznaczającego</param>
void AUnitManager::SelectUnit(ABaseUnit* Unit, int32 PlayerID)
{
    UE_LOG(LogTemp, Error, TEXT("=== UNITMANAGER: SelectUnit wywołana ==="));

    if (!HasAuthority() || !Unit)
        return;

    // Sprawdź czy jednostka należy do gracza
    if (!DoesUnitBelongToPlayer(Unit, PlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("=== UNITMANAGER: Gracz nie może zaznaczyć jednostki ==="));
        return;
    }

    // Odznacz poprzednią jednostkę
    DeselectUnit();

    // Ustaw nową selekcję
    SelectedUnit = Unit;
    SelectedUnitPlayerID = PlayerID;

    // Wyemituj zdarzenia selekcji
    OnUnitSelected.Broadcast(Unit, PlayerID);
    MulticastUnitSelected(Unit, PlayerID);

    FVector2D UnitPosition = Unit->GridPosition;

    // Pokaż podświetlenie selekcji dla lokalnego kontrolera gracza
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        if (AStrategyPlayerController* StratPC = Cast<AStrategyPlayerController>(Iterator->Get()))
        {
            if (StratPC->GetPlayerID() == PlayerID)
            {
                StratPC->ClientShowUnitSelectionByPosition(UnitPosition, PlayerID);
                break;
            }
        }
    }
}

/// <summary>
/// Sprawdza czy jednostka należy do danego gracza.
/// </summary>
/// <param name="Unit">Jednostka do sprawdzenia</param>
/// <param name="PlayerID">ID gracza</param>
/// <returns>True jeśli jednostka należy do gracza, false wpp</returns>
bool AUnitManager::DoesUnitBelongToPlayer(ABaseUnit* Unit, int32 PlayerID) const
{
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit == Unit && UnitData.PlayerID == PlayerID)
        {
            return true;
        }
    }
    return false;
}

/// <summary>
/// Odznacza aktualnie zaznaczoną jednostkę.
/// </summary>
void AUnitManager::DeselectUnit()
{
    if (!HasAuthority())
        return;

    if (SelectedUnit)
    {
        MulticastUnitDeselected();
    }

    SelectedUnit = nullptr;
    SelectedUnitPlayerID = -1;
}

/// <summary>
/// Obsługuje kliknięcie na pole siatki.
/// </summary>
/// <param name="GridPosition">Pozycja klikniętego pola</param>
/// <param name="PlayerID">ID gracza klikającego</param>
void AUnitManager::HandleGridClick(FVector2D GridPosition, int32 PlayerID)
{
    if (!HasAuthority() || !GridManagerRef)
        return;

    ABaseUnit* UnitAtPosition = GetUnitAtPosition(GridPosition);

    if (UnitAtPosition)
    {
        // Kliknięto jednostkę
        HandleUnitClick(UnitAtPosition, PlayerID);
    }
    else if (SelectedUnit && SelectedUnitPlayerID == PlayerID)
    {
        // Próba przeniesienia zaznaczonej jednostki
        if (CanMoveUnitToPosition(SelectedUnit, GridPosition, PlayerID))
        {
            MoveUnitToPosition(SelectedUnit, GridPosition, PlayerID);
        }
    }
}

/// <summary>
/// Obsługuje kliknięcie na jednostkę.
/// </summary>
/// <param name="ClickedUnit">Kliknięta jednostka</param>
/// <param name="PlayerID">ID gracza klikającego</param>
void AUnitManager::HandleUnitClick(ABaseUnit* ClickedUnit, int32 PlayerID)
{
    if (!HasAuthority() || !ClickedUnit)
        return;

    if (DoesUnitBelongToPlayer(ClickedUnit, PlayerID))
    {
        SelectUnit(ClickedUnit, PlayerID);
    }
}

/// <summary>
/// Wymusza odznaczenie wszystkich jednostek.
/// </summary>
void AUnitManager::ForceDeselectAllUnits()
{
    UE_LOG(LogTemp, Warning, TEXT("ForceDeselectAllUnits wywołana"));

    SelectedUnit = nullptr;
    SelectedUnitPlayerID = -1;
    HideAllHighlights();

    OnUnitDeselected.Broadcast(nullptr);

    UE_LOG(LogTemp, Warning, TEXT("Wszystkie jednostki wymuszone odznaczone"));
}

/// <summary>
/// Czyści wszystkie jednostki z gry.
/// </summary>
void AUnitManager::ClearAllUnits()
{
    UE_LOG(LogTemp, Warning, TEXT("ClearAllUnits wywołana - HasAuthority: %s"),
        HasAuthority() ? TEXT("SERWER") : TEXT("KLIENT"));

    if (HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("SERWER: ClearAllUnits - czyszczenie danych serwera"));

        // Wyczyść tablicę walki z lokalności danych
        CombatUnitsArray.Empty();
        ActiveCombatUnitsCount = 0;

        SpawnedUnits.Empty();
        ForceDeselectAllUnits();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("KLIENT: ClearAllUnits - niszczenie pozostałych jednostek"));

        TArray<ABaseUnit*> UnitsToDestroy;

        // Zbierz wszystkie ważne jednostki
        for (const FSpawnedUnitData& UnitData : SpawnedUnits)
        {
            if (UnitData.Unit && IsValid(UnitData.Unit))
            {
                UnitsToDestroy.Add(UnitData.Unit);
            }
        }

        // Zniszcz jednostki
        for (ABaseUnit* Unit : UnitsToDestroy)
        {
            UE_LOG(LogTemp, Warning, TEXT("KLIENT: Wymuszenie zniszczenia pozostałej jednostki"));
            Unit->Destroy();
        }

        SpawnedUnits.Empty();
        ForceDeselectAllUnits();
    }

    UE_LOG(LogTemp, Warning, TEXT("Tablica jednostek wyczyszczona - nowy rozmiar: %d"), SpawnedUnits.Num());
}

/// <summary>
/// Przygotowuje system do czyszczenia jednostek.
/// </summary>
void AUnitManager::PrepareForUnitClearing()
{
    UE_LOG(LogTemp, Warning, TEXT("KLIENT: PrepareForUnitClearing - czyszczenie selekcji"));

    ForceDeselectAllUnits();
    HideAllHighlights();

    UE_LOG(LogTemp, Warning, TEXT("KLIENT: Aktualne jednostki w tablicy: %d"), SpawnedUnits.Num());

    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit))
        {
            UE_LOG(LogTemp, Warning, TEXT("KLIENT: Jednostka na (%f,%f) gotowa do zniszczenia"),
                UnitData.GridPosition.X, UnitData.GridPosition.Y);
        }
    }
}

/// <summary>
/// Finalizuje proces czyszczenia jednostek na kliencie.
/// </summary>
void AUnitManager::FinalizeUnitClearing()
{
    UE_LOG(LogTemp, Warning, TEXT("KLIENT: FinalizeUnitClearing - zapewnienie że wszystkie jednostki są wyczyszczone"));

    TArray<ABaseUnit*> RemainingUnits;

    // Znajdź wszystkie pozostałe jednostki
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit && IsValid(UnitData.Unit))
        {
            RemainingUnits.Add(UnitData.Unit);
            UE_LOG(LogTemp, Warning, TEXT("KLIENT: Znaleziono pozostałą jednostkę na (%f,%f)"),
                UnitData.GridPosition.X, UnitData.GridPosition.Y);
        }
    }

    // Zniszcz pozostałe jednostki
    for (ABaseUnit* Unit : RemainingUnits)
    {
        UE_LOG(LogTemp, Warning, TEXT("KLIENT: Wymuszenie zniszczenia pozostałej jednostki w finalizacji"));
        Unit->Destroy();
    }

    int32 PreviousCount = SpawnedUnits.Num();
    SpawnedUnits.Empty();

    UE_LOG(LogTemp, Warning, TEXT("KLIENT: Wyczyszczono %d jednostek z lokalnej tablicy"), PreviousCount);

    ForceDeselectAllUnits();
    HideAllHighlights();
}

/// <summary>
/// Niszczy jednostki na określonych pozycjach.
/// </summary>
/// <param name="Positions">Tablica pozycji siatki gdzie należy zniszczyć jednostki</param>
void AUnitManager::DestroyUnitsAtPositions(const TArray<FVector2D>& Positions)
{
    UE_LOG(LogTemp, Warning, TEXT("KLIENT: DestroyUnitsAtPositions wywołana z %d pozycjami"), Positions.Num());

    if (HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("SERWER: Pomijanie DestroyUnitsAtPositions - serwer obsługuje niszczenie osobno"));
        return;
    }

    TArray<ABaseUnit*> UnitsToDestroy;

    // Przeszukaj pozycje i znajdź jednostki do zniszczenia
    for (const FVector2D& Position : Positions)
    {
        UE_LOG(LogTemp, Warning, TEXT("KLIENT: Szukanie jednostki na pozycji (%f,%f)"), Position.X, Position.Y);

        for (int32 i = 0; i < SpawnedUnits.Num(); i++)
        {
            const FSpawnedUnitData& UnitData = SpawnedUnits[i];

            if (UnitData.Unit && IsValid(UnitData.Unit))
            {
                bool bPositionMatch = false;

                // Sprawdź dokładne dopasowanie pozycji
                if (UnitData.GridPosition.Equals(Position, 0.01f))
                {
                    bPositionMatch = true;
                    UE_LOG(LogTemp, Warning, TEXT("KLIENT: Dokładne dopasowanie pozycji na (%f,%f)"), Position.X, Position.Y);
                }
                else
                {
                    // Sprawdź dopasowanie z tolerancją
                    float Distance = FVector2D::Distance(UnitData.GridPosition, Position);
                    if (Distance < 0.5f)
                    {
                        bPositionMatch = true;
                        UE_LOG(LogTemp, Warning, TEXT("KLIENT: Dopasowanie pozycji z tolerancją na (%f,%f), odległość: %f"),
                            Position.X, Position.Y, Distance);
                    }
                }

                if (bPositionMatch)
                {
                    UE_LOG(LogTemp, Warning, TEXT("KLIENT: Znaleziono jednostkę do zniszczenia na pozycji (%f,%f)"), Position.X, Position.Y);
                    UnitsToDestroy.AddUnique(UnitData.Unit);
                    break;
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("KLIENT: Znaleziono %d jednostek do zniszczenia"), UnitsToDestroy.Num());

    // Zniszcz znalezione jednostki
    for (ABaseUnit* Unit : UnitsToDestroy)
    {
        if (Unit && IsValid(Unit))
        {
            UE_LOG(LogTemp, Warning, TEXT("KLIENT: Niszczenie jednostki na pozycji (%f,%f)"),
                Unit->GridPosition.X, Unit->GridPosition.Y);

            // Jeśli niszczona jednostka jest zaznaczona, wyczyść selekcję
            if (SelectedUnit == Unit)
            {
                UE_LOG(LogTemp, Warning, TEXT("KLIENT: Czyszczenie selekcji dla niszczonej jednostki"));
                SelectedUnit = nullptr;
                SelectedUnitPlayerID = -1;
                HideAllHighlights();
            }

            RemoveUnitFromClientArray(Unit);
            Unit->Destroy();
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("KLIENT: Niszczenie jednostek zakończone. Pozostałe jednostki: %d"), SpawnedUnits.Num());
}

/// <summary>
/// Usuwa jednostkę z lokalnej tablicy klienta.
/// </summary>
/// <param name="Unit">Jednostka do usunięcia z tablicy</param>
void AUnitManager::RemoveUnitFromClientArray(ABaseUnit* Unit)
{
    if (!Unit)
        return;

    int32 RemovedCount = 0;

    for (int32 i = SpawnedUnits.Num() - 1; i >= 0; i--)
    {
        if (SpawnedUnits[i].Unit == Unit)
        {
            UE_LOG(LogTemp, Warning, TEXT("KLIENT: Usuwanie jednostki z tablicy na indeksie %d"), i);
            SpawnedUnits.RemoveAt(i);
            RemovedCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("KLIENT: Usunięto %d wpisów jednostki z tablicy"), RemovedCount);
}

/// <summary>
/// Multicast RPC informujący klientów o zespawnowaniu nowej jednostki.
/// </summary>
/// <param name="SpawnedUnit">Jednostka zespawnowana na serwerze (używana tylko na serwerze)</param>
/// <param name="PlayerID">ID gracza właściciela jednostki</param>
/// <param name="GridPosition">Pozycja na siatce gdzie jednostka została zespawnowana</param>
/// <param name="UnitType">Typ jednostki do zespawnowania</param>
void AUnitManager::MulticastUnitSpawned_Implementation(ABaseUnit* SpawnedUnit, int32 PlayerID, FVector2D GridPosition, EBaseUnitType UnitType)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("Klient: Otrzymano żądanie spawnu jednostki typu %d dla gracza %d"), (int32)UnitType, PlayerID);

        // Pobierz klasę jednostki na podstawie typu
        TSubclassOf<ABaseUnit> UnitClass = GetUnitClassByType(UnitType);
        if (!UnitClass)
        {
            UE_LOG(LogTemp, Error, TEXT("Klient: Brak klasy dla jednostki typu %d"), (int32)UnitType);
            return;
        }

        // Oblicz pozycję w świecie
        FVector WorldLocation = GetWorldLocationFromGrid(GridPosition);
        WorldLocation.Z += 20.0f; // Lekkie uniesienie nad ziemię

        // Ustaw rotację - gracz 1 jest obrócony o 180 stopni
        FRotator SpawnRotation = FRotator::ZeroRotator;
        if (PlayerID == 1)
        {
            SpawnRotation.Yaw = 180.0f;
        }

        // Przygotuj parametry spawnu
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        // Zespawnuj jednostkę po stronie klienta
        ABaseUnit* ClientSpawnedUnit = GetWorld()->SpawnActor<ABaseUnit>(UnitClass, WorldLocation, SpawnRotation, SpawnParams);

        if (ClientSpawnedUnit)
        {
            // Zainicjalizuj właściwości jednostki
            ClientSpawnedUnit->TeamID = PlayerID;
            ClientSpawnedUnit->GridPosition = GridPosition;
            ClientSpawnedUnit->UnitType = UnitType;

            // Dodaj do lokalnej tablicy jednostek
            FSpawnedUnitData UnitData;
            UnitData.Unit = ClientSpawnedUnit;
            UnitData.PlayerID = PlayerID;
            UnitData.GridPosition = GridPosition;
            UnitData.UnitType = UnitType;
            SpawnedUnits.Add(UnitData);

            UE_LOG(LogTemp, Warning, TEXT("Klient: Pomyślnie zespawnowano jednostkę typu %d dla gracza %d"), (int32)UnitType, PlayerID);
        }
    }
}

/// <summary>
/// Multicast RPC informujący o przeniesieniu jednostki.
/// </summary>
/// <param name="MovedUnit">Jednostka która została przeniesiona</param>
/// <param name="OldPosition">Poprzednia pozycja jednostki</param>
/// <param name="NewPosition">Nowa pozycja jednostki</param>
/// <param name="PlayerID">ID gracza właściciela jednostki</param>
void AUnitManager::MulticastUnitMoved_Implementation(ABaseUnit* MovedUnit, FVector2D OldPosition, FVector2D NewPosition, int32 PlayerID)
{
    if (!HasAuthority())
    {
        if (MovedUnit)
        {
            // Aktualizuj pozycję na siatce
            MovedUnit->GridPosition = NewPosition;

            // Oblicz nową pozycję w świecie
            FVector CurrentWorldLocation = MovedUnit->GetActorLocation();
            FVector NewWorldLocation = GetWorldLocationFromGrid(NewPosition);
            NewWorldLocation.Z = CurrentWorldLocation.Z;

            MovedUnit->SetActorLocation(NewWorldLocation);
            UpdateUnitDataPosition(MovedUnit, NewPosition);

            // Jeśli jednostka jest zaznaczona, odśwież podświetlenia
            if (SelectedUnit == MovedUnit)
            {
                HideAllHighlights();
                ShowValidMovePositions(MovedUnit, PlayerID);
            }
        }
    }
}

/// <summary>
/// Multicast RPC informujący o przeniesieniu jednostki przez pozycję.
/// </summary>
/// <param name="OldPosition">Pozycja gdzie jednostka się znajdowała</param>
/// <param name="NewPosition">Nowa pozycja jednostki</param>
/// <param name="PlayerID">ID gracza właściciela jednostki</param>
void AUnitManager::MulticastUnitMovedByPosition_Implementation(FVector2D OldPosition, FVector2D NewPosition, int32 PlayerID)
{
    if (!HasAuthority())
    {
        // Znajdź jednostkę na starej pozycji
        ABaseUnit* UnitToMove = GetUnitAtPosition(OldPosition);

        // Jeśli nie znaleziono dokładnie, szukaj z tolerancją
        if (!UnitToMove)
        {
            for (const FSpawnedUnitData& UnitData : SpawnedUnits)
            {
                if (UnitData.Unit && UnitData.PlayerID == PlayerID)
                {
                    float Distance = FVector2D::Distance(UnitData.GridPosition, OldPosition);
                    if (Distance < 0.5f)
                    {
                        UnitToMove = UnitData.Unit;
                        break;
                    }
                }
            }
        }

        if (UnitToMove)
        {
            // Aktualizuj pozycję jednostki
            UnitToMove->GridPosition = NewPosition;

            FVector CurrentWorldPos = UnitToMove->GetActorLocation();
            FVector NewWorldLocation = GetWorldLocationFromGrid(NewPosition);
            NewWorldLocation.Z = CurrentWorldPos.Z;

            UnitToMove->SetActorLocation(NewWorldLocation);
            UpdateUnitDataPosition(UnitToMove, NewPosition);

            // Odśwież podświetlenia jeśli jednostka jest zaznaczona
            if (SelectedUnit == UnitToMove)
            {
                HideAllHighlights();
                ShowValidMovePositions(UnitToMove, PlayerID);
            }
        }
    }
}

/// <summary>
/// Aktualizuje pozycję jednostki w tablicy SpawnedUnits.
/// </summary>
/// <param name="Unit">Jednostka do zaktualizowania</param>
/// <param name="NewPosition">Nowa pozycja na siatce</param>
void AUnitManager::UpdateUnitDataPosition(ABaseUnit* Unit, FVector2D NewPosition)
{
    for (FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.Unit == Unit)
        {
            UnitData.GridPosition = NewPosition;
            break;
        }
    }
}

/// <summary>
/// Multicast RPC informujący o zaznaczeniu jednostki.
/// </summary>
/// <param name="Unit">Jednostka która została zaznaczona</param>
/// <param name="PlayerID">ID gracza który zaznaczył jednostkę</param>
void AUnitManager::MulticastUnitSelected_Implementation(ABaseUnit* Unit, int32 PlayerID)
{
    if (!HasAuthority())
    {
        SelectedUnit = Unit;
        SelectedUnitPlayerID = PlayerID;

        // Znajdź lokalnego kontrolera gracza i pokaż podświetlenia
        for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
        {
            if (AStrategyPlayerController* PC = Cast<AStrategyPlayerController>(Iterator->Get()))
            {
                bool bIsLocal = PC->IsLocalPlayerController();
                int32 PCPlayerID = PC->GetPlayerID();

                // Tylko dla lokalnego gracza który zaznaczył jednostkę
                if (bIsLocal && PCPlayerID == PlayerID)
                {
                    TArray<ABaseUnit*> PlayerUnits = GetPlayerUnits(PlayerID);

                    if (PlayerUnits.Num() > 0)
                    {
                        ABaseUnit* LocalUnit = PlayerUnits.Last();

                        // Wyświetl podświetlenia możliwych ruchów
                        HideAllHighlights();
                        ShowValidMovePositions(LocalUnit, PlayerID);
                        ShowSelectionHighlight(LocalUnit);
                    }
                    break;
                }
            }
        }
    }
}

/// <summary>
/// Multicast RPC informujący o odznaczeniu jednostki.
/// </summary>
void AUnitManager::MulticastUnitDeselected_Implementation()
{
    if (!HasAuthority())
    {
        SelectedUnit = nullptr;
        SelectedUnitPlayerID = -1;
        HideAllHighlights();
    }
}

/// <summary>
/// Multicast RPC informujący o usunięciu jednostki.
/// </summary>
/// <param name="Unit">Jednostka do usunięcia</param>
void AUnitManager::MulticastUnitRemoved_Implementation(ABaseUnit* Unit)
{
    if (!HasAuthority() && Unit)
    {
        // Usuń z lokalnej tablicy
        RemoveUnitFromClientArray(Unit);

        // Jeśli była zaznaczona, wyczyść selekcję
        if (SelectedUnit == Unit)
        {
            SelectedUnit = nullptr;
            SelectedUnitPlayerID = -1;
            HideAllHighlights();
        }

        // Zniszcz aktora
        Unit->Destroy();
    }
}

/// <summary>
/// Multicast RPC czyszczący wszystkie jednostki.
/// </summary>
void AUnitManager::MulticastClearAllUnits_Implementation()
{
    if (!HasAuthority())
    {
        // Zniszcz wszystkie jednostki
        for (auto& UnitData : SpawnedUnits)
        {
            if (UnitData.Unit)
            {
                UnitData.Unit->Destroy();
            }
        }

        // Wyczyść dane lokalne
        SpawnedUnits.Empty();
        SelectedUnit = nullptr;
        SelectedUnitPlayerID = -1;
        HideAllHighlights();
    }
}

/// <summary>
/// Multicast RPC potwierdzający wyczyszczenie wszystkich jednostek.
/// </summary>
void AUnitManager::MulticastAllUnitsCleared_Implementation()
{
    if (!HasAuthority())
    {
        TArray<ABaseUnit*> RemainingUnits;

        // Zbierz wszystkie pozostałe jednostki
        for (const FSpawnedUnitData& UnitData : SpawnedUnits)
        {
            if (UnitData.Unit && IsValid(UnitData.Unit))
            {
                RemainingUnits.Add(UnitData.Unit);
            }
        }

        // Zniszcz je
        for (ABaseUnit* Unit : RemainingUnits)
        {
            Unit->Destroy();
        }

        // Wyczyść wszystko
        SpawnedUnits.Empty();
        SelectedUnit = nullptr;
        SelectedUnitPlayerID = -1;
        HideAllHighlights();
    }
}

/// <summary>
/// Pokazuje możliwe pozycje ruchu dla jednostki.
/// </summary>
/// <param name="Unit">Jednostka dla której pokazujemy możliwe ruchy</param>
/// <param name="PlayerID">ID gracza właściciela jednostki</param>
void AUnitManager::ShowValidMovePositions(ABaseUnit* Unit, int32 PlayerID)
{
    // Gracz 1 nie widzi podświetleń na serwerze
    if (HasAuthority() && PlayerID == 1)
    {
        return;
    }

    if (!Unit || !GridManagerRef)
        return;

    ClearHighlightComponents();

    // Pobierz wszystkie ważne pozycje spawnu dla gracza
    TArray<FVector2D> ValidPositions = GridManagerRef->GetValidSpawnPositions(PlayerID);

    for (const FVector2D& Position : ValidPositions)
    {
        UMaterialInterface* MaterialToUse = nullptr;

        // Wybierz odpowiedni materiał w zależności od stanu pozycji
        if (!IsPositionOccupied(Position))
        {
            // Pozycja wolna - dozwolony ruch
            MaterialToUse = ValidMoveHighlightMaterial;
        }
        else if (Position == Unit->GridPosition)
        {
            // Aktualna pozycja jednostki
            MaterialToUse = SelectionHighlightMaterial;
        }
        else
        {
            // Pozycja zajęta - niedozwolony ruch
            MaterialToUse = InvalidMoveHighlightMaterial;
        }

        if (MaterialToUse)
        {
            HighlightGridPosition(Position, MaterialToUse);
        }
    }
}

/// <summary>
/// Ukrywa wszystkie aktywne podświetlenia.
/// </summary>
void AUnitManager::HideAllHighlights()
{
    ClearHighlightComponents();
}

/// <summary>
/// Podświetla pojedyncze pole siatki określonym materiałem.
/// </summary>
/// <param name="GridPosition">Pozycja do podświetlenia</param>
/// <param name="Material">Materiał do użycia dla podświetlenia</param>
void AUnitManager::HighlightGridPosition(FVector2D GridPosition, UMaterialInterface* Material)
{
    if (Material)
    {
        CreateHighlightComponent(GridPosition, Material);
    }
}

/// <summary>
/// Pokazuje podświetlenie selekcji dla jednostki.
/// </summary>
/// <param name="Unit">Jednostka do podświetlenia</param>
void AUnitManager::ShowSelectionHighlight(ABaseUnit* Unit)
{
    if (Unit && SelectionHighlightMaterial)
    {
        HighlightGridPosition(Unit->GridPosition, SelectionHighlightMaterial);
    }
}

/// <summary>
/// Tworzy komponent podświetlenia na określonej pozycji siatki.
/// </summary>
/// <param name="GridPosition">Pozycja na siatce gdzie utworzyć podświetlenie</param>
/// <param name="Material">Materiał do zastosowania na mesh</param>
void AUnitManager::CreateHighlightComponent(FVector2D GridPosition, UMaterialInterface* Material)
{
    if (!Material)
        return;

    // Utwórz nowy komponent mesh
    UStaticMeshComponent* HighlightComp = NewObject<UStaticMeshComponent>(this);
    if (!HighlightComp)
        return;

    // Podepnij do root component
    HighlightComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    // Ustaw pozycję w świecie
    FVector WorldLocation = GetWorldLocationFromGrid(GridPosition);
    WorldLocation.Z += 1.0f;
    HighlightComp->SetWorldLocation(WorldLocation);

    // Oblicz skalowanie na podstawie rozmiaru komórki siatki
    if (GridManagerRef)
    {
        float CellSize = GridManagerRef->CellSize;
        float HighlightSize = CellSize * HighlightSizeMultiplier;
        float ScaleValue = HighlightSize / 100.0f; // Mesh bazowy ma 100 jednostek
        FVector ScaleVector(ScaleValue, ScaleValue, 1.0f);
        HighlightComp->SetWorldScale3D(ScaleVector);
    }

    // Ustaw mesh i materiał
    if (HighlightPlaneMesh)
    {
        HighlightComp->SetStaticMesh(HighlightPlaneMesh);
    }

    HighlightComp->SetMaterial(0, Material);
    HighlightComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HighlightComp->SetVisibility(true);
    HighlightComp->RegisterComponent();

    // Dodaj do tablicy komponentów
    HighlightComponents.Add(HighlightComp);
}

/// <summary>
/// Niszczy wszystkie aktywne komponenty podświetleń.
/// </summary>
void AUnitManager::ClearHighlightComponents()
{
    for (UStaticMeshComponent* Component : HighlightComponents)
    {
        if (Component)
        {
            Component->DestroyComponent();
        }
    }
    HighlightComponents.Empty();
}

/// <summary>
/// Sprawdza czy pozycja należy do strefy spawnu danego gracza.
/// </summary>
/// <param name="GridPosition">Pozycja do sprawdzenia</param>
/// <param name="PlayerID">ID gracza</param>
/// <returns>True jeśli pozycja jest w strefie spawnu gracza</returns>
bool AUnitManager::IsValidPlayerSpawnPosition(FVector2D GridPosition, int32 PlayerID) const
{
    if (!GridManagerRef)
        return false;

    if (PlayerID == 0)
        return GridManagerRef->IsPositionInPlayer1SpawnZone(GridPosition);
    else if (PlayerID == 1)
        return GridManagerRef->IsPositionInPlayer2SpawnZone(GridPosition);

    return false;
}

/// <summary>
/// Zwraca klasę jednostki na podstawie typu enum.
/// </summary>
/// <param name="UnitType">Typ jednostki</param>
/// <returns>Klasa Blueprint jednostki</returns>
TSubclassOf<ABaseUnit> AUnitManager::GetUnitClassByType(EBaseUnitType UnitType) const
{
    switch (UnitType)
    {
    case EBaseUnitType::Tank: return TankUnitClass;
    case EBaseUnitType::Ninja: return NinjaUnitClass;
    case EBaseUnitType::Sword: return SwordUnitClass;
    case EBaseUnitType::Armor: return ArmorUnitClass;
    default: return TankUnitClass;
    }
}

/// <summary>
/// Zwraca liczbę jednostek należących do gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>Liczba jednostek gracza</returns>
int32 AUnitManager::GetUnitCount(int32 PlayerID) const
{
    int32 Count = 0;
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.PlayerID == PlayerID && UnitData.Unit)
        {
            Count++;
        }
    }
    return Count;
}

/// <summary>
/// Zwraca liczbę jednostek danego typu należących do gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="UnitType">Typ jednostki do policzenia</param>
/// <returns>Liczba jednostek określonego typu</returns>
int32 AUnitManager::GetUnitCountByType(int32 PlayerID, EBaseUnitType UnitType) const
{
    int32 Count = 0;
    for (const FSpawnedUnitData& UnitData : SpawnedUnits)
    {
        if (UnitData.PlayerID == PlayerID && UnitData.UnitType == UnitType && UnitData.Unit)
        {
            Count++;
        }
    }
    return Count;
}

/// <summary>
/// Konwertuje współrzędne siatki na pozycję w świecie 3D.
/// </summary>
/// <param name="GridPosition">Pozycja na siatce (X, Y)</param>
/// <returns>Pozycja w świecie 3D (środek komórki)</returns>
FVector AUnitManager::GetWorldLocationFromGrid(FVector2D GridPosition) const
{
    if (GridManagerRef)
    {
        // Użyj GridManager jeśli dostępny
        FVector GridCorner = GridManagerRef->GetWorldLocationFromGrid(GridPosition);
        float HalfCellSize = GridManagerRef->CellSize * 0.5f;
        return GridCorner + FVector(HalfCellSize, HalfCellSize, 0.0f);
    }

    // Fallback - użyj domyślnego rozmiaru komórki
    const float CellSize = 100.0f;
    const float HalfCellSize = CellSize * 0.5f;
    float WorldX = (GridPosition.X * CellSize) + HalfCellSize;
    float WorldY = (GridPosition.Y * CellSize) + HalfCellSize;

    return FVector(WorldX, WorldY, 0.0f);
}

/// <summary>
/// Funkcja diagnostyczna dla problemów z siecią.
/// </summary>
void AUnitManager::DiagnoseNetworkIssues()
{
    UE_LOG(LogTemp, Error, TEXT("=== POCZĄTEK DIAGNOSTYKI SIECI ==="));
    UE_LOG(LogTemp, Error, TEXT("=== SIEĆ: HasAuthority: %s ==="), HasAuthority() ? TEXT("PRAWDA") : TEXT("FAŁSZ"));
    UE_LOG(LogTemp, Error, TEXT("=== SIEĆ: GetLocalRole: %d ==="), (int32)GetLocalRole());
    UE_LOG(LogTemp, Error, TEXT("=== SIEĆ: GetRemoteRole: %d ==="), (int32)GetRemoteRole());
    UE_LOG(LogTemp, Error, TEXT("=== SIEĆ: bReplicates: %s ==="), bReplicates ? TEXT("PRAWDA") : TEXT("FAŁSZ"));

    if (GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("=== SIEĆ: Tryb sieciowy świata: %d ==="), (int32)GetWorld()->GetNetMode());
        UE_LOG(LogTemp, Error, TEXT("=== SIEĆ: AuthorityGameMode świata: %s ==="),
            GetWorld()->GetAuthGameMode() ? TEXT("ISTNIEJE") : TEXT("NULL"));
    }

    // Sprawdź indywidualne jednostki
    for (int32 i = 0; i < SpawnedUnits.Num(); i++)
    {
        if (SpawnedUnits[i].Unit)
        {
            ABaseUnit* Unit = SpawnedUnits[i].Unit;
            UE_LOG(LogTemp, Error, TEXT("=== JEDNOSTKA SIECIOWA[%d]: %s - HasAuthority:%s, LocalRole:%d, RemoteRole:%d, bReplicates:%s ==="),
                i, *Unit->GetName(),
                Unit->HasAuthority() ? TEXT("PRAWDA") : TEXT("FAŁSZ"),
                (int32)Unit->GetLocalRole(),
                (int32)Unit->GetRemoteRole(),
                Unit->GetIsReplicated() ? TEXT("PRAWDA") : TEXT("FAŁSZ"));
        }
    }
    UE_LOG(LogTemp, Error, TEXT("=== KONIEC DIAGNOSTYKI SIECI ==="));
}