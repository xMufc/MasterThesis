// SpatialGrid.cpp - Implementacja hierarchicznego systemu podzialu przestrzennego
#include "SpatialGrid.h"
#include "BaseUnit.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

USpatialGrid::USpatialGrid()
{
    BaseGridCellSize = 200.0f;
    BaseGridWidth = 0;
    BaseGridHeight = 0;
    MegaCellsPerDimension = 3; // Domyslnie: 3x3 komorki bazowe na jedna mega-komorkê
    MegaGridWidth = 0;
    MegaGridHeight = 0;
    MegaCellSize = 0.0f;
    WorldMin = FVector2D::ZeroVector;
    WorldMax = FVector2D::ZeroVector;
}

/// <summary>
/// Inicjalizuje siatkê przestrzenna na podstawie granic swiata i rozmiaru komorki.
/// </summary>
/// <param name="InWorldMin">Minimalne wspolrzêdne swiata (lewy dolny rog)</param>
/// <param name="InWorldMax">Maksymalne wspolrzêdne swiata (prawy gorny rog)</param>
/// <param name="InCellSize">Rozmiar pojedynczej komorki bazowej w jednostkach swiata</param>
void USpatialGrid::InitializeGrid(FVector2D InWorldMin, FVector2D InWorldMax, float InCellSize)
{
    WorldMin = InWorldMin;
    WorldMax = InWorldMax;
    BaseGridCellSize = InCellSize;

    // Obliczanie wymiarow siatki bazowej
    FVector2D WorldSize = WorldMax - WorldMin;
    BaseGridWidth = FMath::CeilToInt(WorldSize.X / BaseGridCellSize);
    BaseGridHeight = FMath::CeilToInt(WorldSize.Y / BaseGridCellSize);

    InitializeGridFromBaseGrid(WorldMin, BaseGridWidth, BaseGridHeight, BaseGridCellSize);
}

/// <summary>
/// Inicjalizuje siatkê na podstawie bezposrednio podanych wymiarow siatki bazowej.
/// Tworzy hierarchiczna strukturê: siatka bazowa + mega-siatka (grupy komorek bazowych).
/// </summary>
/// <param name="InWorldMin">Punkt poczatkowy swiata</param>
/// <param name="InBaseGridWidth">Liczba komorek bazowych w poziomie</param>
/// <param name="InBaseGridHeight">Liczba komorek bazowych w pionie</param>
/// <param name="InBaseGridCellSize">Rozmiar pojedynczej komorki bazowej</param>
void USpatialGrid::InitializeGridFromBaseGrid(FVector2D InWorldMin, int32 InBaseGridWidth, int32 InBaseGridHeight, float InBaseGridCellSize)
{
    WorldMin = InWorldMin;
    BaseGridWidth = InBaseGridWidth;
    BaseGridHeight = InBaseGridHeight;
    BaseGridCellSize = InBaseGridCellSize;

    // Obliczanie wymiarow mega-siatki (kazda mega-komorka zawiera 3x3 komorki bazowe)
    MegaGridWidth = FMath::CeilToInt(static_cast<float>(BaseGridWidth) / MegaCellsPerDimension);
    MegaGridHeight = FMath::CeilToInt(static_cast<float>(BaseGridHeight) / MegaCellsPerDimension);
    MegaCellSize = BaseGridCellSize * MegaCellsPerDimension;

    // Obliczanie rzeczywistego maksimum swiata na podstawie siatki bazowej
    WorldMax = WorldMin + FVector2D(BaseGridWidth * BaseGridCellSize, BaseGridHeight * BaseGridCellSize);

    UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Inicjalizacja hierarchicznej siatki ==="));
    UE_LOG(LogTemp, Warning, TEXT("=== Siatka bazowa: %dx%d komorek, Rozmiar komorki: %f ==="),
        BaseGridWidth, BaseGridHeight, BaseGridCellSize);
    UE_LOG(LogTemp, Warning, TEXT("=== Mega-siatka: %dx%d mega-komorek (%dx%d komorek bazowych na mega-komorkê) ==="),
        MegaGridWidth, MegaGridHeight, MegaCellsPerDimension, MegaCellsPerDimension);
    UE_LOG(LogTemp, Warning, TEXT("=== Rozmiar mega-komorki: %f ==="), MegaCellSize);
    UE_LOG(LogTemp, Warning, TEXT("=== Granice swiata: (%f,%f) do (%f,%f) ==="),
        WorldMin.X, WorldMin.Y, WorldMax.X, WorldMax.Y);

    // Inicjalizacja struktury mega-siatki
    InitializeMegaCells();

    UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Siatka zainicjalizowana z %d calkowita liczba mega-komorek ==="), MegaCells.Num());
}

/// <summary>
/// Tworzy i konfiguruje wszystkie mega-komorki w siatce.
/// Dla kazdej mega - komorki oblicza :
/// Wspolrzêdne w mega - siatce
/// Zakres pokrywanych komorek bazowych
/// Granice w przestrzeni swiata
/// </summary>
void USpatialGrid::InitializeMegaCells()
{
    int32 TotalMegaCells = MegaGridWidth * MegaGridHeight;
    MegaCells.Empty(TotalMegaCells);
    MegaCells.SetNum(TotalMegaCells);

    // Inicjalizacja kazdej mega-komorki z odpowiednimi granicami i wspolrzêdnymi siatki bazowej
    for (int32 mx = 0; mx < MegaGridWidth; mx++)
    {
        for (int32 my = 0; my < MegaGridHeight; my++)
        {
            int32 Index = GetMegaCellIndex(mx, my);
            if (Index >= 0 && Index < MegaCells.Num())
            {
                FSpatialCell& MegaCell = MegaCells[Index];
                MegaCell.MegaCellX = mx;
                MegaCell.MegaCellY = my;

                // Obliczanie wspolrzêdnych siatki bazowej pokrywanych przez tê mega-komorkê
                MegaCell.BaseGridStartX = mx * MegaCellsPerDimension;
                MegaCell.BaseGridStartY = my * MegaCellsPerDimension;

                // Wspolrzêdne koncowe (wlacznie), ograniczone do rzeczywistego rozmiaru siatki bazowej
                MegaCell.BaseGridEndX = FMath::Min((mx + 1) * MegaCellsPerDimension - 1, BaseGridWidth - 1);
                MegaCell.BaseGridEndY = FMath::Min((my + 1) * MegaCellsPerDimension - 1, BaseGridHeight - 1);

                // Obliczanie granic swiata dla tej mega-komorki
                MegaCell.MinBounds = WorldMin + FVector2D(
                    MegaCell.BaseGridStartX * BaseGridCellSize,
                    MegaCell.BaseGridStartY * BaseGridCellSize
                );

                MegaCell.MaxBounds = WorldMin + FVector2D(
                    (MegaCell.BaseGridEndX + 1) * BaseGridCellSize,
                    (MegaCell.BaseGridEndY + 1) * BaseGridCellSize
                );

                MegaCell.Units.Empty();

                int32 BaseCellsInMega = (MegaCell.BaseGridEndX - MegaCell.BaseGridStartX + 1) *
                    (MegaCell.BaseGridEndY - MegaCell.BaseGridStartY + 1);

                UE_LOG(LogTemp, Log, TEXT("Mega-komorka (%d,%d): Pokrywa siatkê bazowa (%d,%d) do (%d,%d) = %d komorek bazowych"),
                    mx, my, MegaCell.BaseGridStartX, MegaCell.BaseGridStartY,
                    MegaCell.BaseGridEndX, MegaCell.BaseGridEndY, BaseCellsInMega);
            }
        }
    }
}

/// <summary>
/// Dodaje jednostkê do odpowiedniej mega-komorki na podstawie jej pozycji.
/// </summary>
/// <param name="Unit">Wskaznik do jednostki do dodania</param>
void USpatialGrid::AddUnit(ABaseUnit* Unit)
{
    if (!Unit)
    {
        return;
    }

    FVector UnitPosition = Unit->GetActorLocation();
    FVector2D MegaCellCoords = GetMegaCellCoordinates(UnitPosition);

    if (!IsValidMegaCellCoordinate(MegaCellCoords.X, MegaCellCoords.Y))
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Pozycja jednostki %s (%f,%f) poza granicami siatki ==="),
            *Unit->GetName(), UnitPosition.X, UnitPosition.Y);
        return;
    }

    FSpatialCell* MegaCell = GetMegaCell(MegaCellCoords.X, MegaCellCoords.Y);
    if (MegaCell)
    {
        MegaCell->AddUnit(Unit);
        UnitToMegaCellMap.Add(Unit, MegaCellCoords);

        FVector2D BaseGridCoords = GetBaseGridCoordinates(UnitPosition);
        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Dodano jednostkê %s do mega-komorki (%d,%d) [siatka bazowa: (%d,%d)] - Mega-komorka ma teraz %d jednostek ==="),
            *Unit->GetName(), (int32)MegaCellCoords.X, (int32)MegaCellCoords.Y,
            (int32)BaseGridCoords.X, (int32)BaseGridCoords.Y, MegaCell->GetUnitCount());
    }
}

/// <summary>
/// Usuwa jednostkê z siatki przestrzennej.
/// </summary>
/// <param name="Unit">Wskaznik do jednostki do usuniêcia</param>
void USpatialGrid::RemoveUnit(ABaseUnit* Unit)
{
    if (!Unit)
    {
        return;
    }

    FVector2D* MegaCellCoords = UnitToMegaCellMap.Find(Unit);
    if (!MegaCellCoords)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Jednostka %s nie zostala znaleziona w sledzeniu siatki ==="), *Unit->GetName());
        return;
    }

    FSpatialCell* MegaCell = GetMegaCell(MegaCellCoords->X, MegaCellCoords->Y);
    if (MegaCell)
    {
        MegaCell->RemoveUnit(Unit);
        UnitToMegaCellMap.Remove(Unit);

        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Usuniêto jednostkê %s z mega-komorki (%d,%d) - Mega-komorka ma teraz %d jednostek ==="),
            *Unit->GetName(), (int32)MegaCellCoords->X, (int32)MegaCellCoords->Y, MegaCell->GetUnitCount());
    }
}


/// <summary>
/// Aktualizuje pozycjê jednostki w siatce, przenoszac ja miêdzy mega-komorkami jesli to konieczne.
/// </summary>
/// <param name="Unit">Wskaznik do jednostki</param>
/// <param name="OldPosition">Poprzednia pozycja jednostki</param>
/// <param name="NewPosition">Nowa pozycja jednostki</param>
void USpatialGrid::UpdateUnitPosition(ABaseUnit* Unit, FVector OldPosition, FVector NewPosition)
{
    if (!Unit)
    {
        return;
    }

    // Obliczanie starych i nowych wspolrzêdnych mega-komorki
    FVector2D OldMegaCellCoords = GetMegaCellCoordinates(OldPosition);
    FVector2D NewMegaCellCoords = GetMegaCellCoordinates(NewPosition);

    // Jesli jednostka nie zmienila mega-komorki, aktualizacja nie jest potrzebna
    if (OldMegaCellCoords.Equals(NewMegaCellCoords, 0.1f))
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Jednostka %s przesunêla siê z mega-komorki (%d,%d) do (%d,%d) ==="),
        *Unit->GetName(), (int32)OldMegaCellCoords.X, (int32)OldMegaCellCoords.Y,
        (int32)NewMegaCellCoords.X, (int32)NewMegaCellCoords.Y);

    // Usuniêcie ze starej mega-komorki
    if (IsValidMegaCellCoordinate(OldMegaCellCoords.X, OldMegaCellCoords.Y))
    {
        FSpatialCell* OldMegaCell = GetMegaCell(OldMegaCellCoords.X, OldMegaCellCoords.Y);
        if (OldMegaCell && OldMegaCell->ContainsUnit(Unit))
        {
            OldMegaCell->RemoveUnit(Unit);
        }
    }

    // Dodanie do nowej mega-komorki
    if (IsValidMegaCellCoordinate(NewMegaCellCoords.X, NewMegaCellCoords.Y))
    {
        FSpatialCell* NewMegaCell = GetMegaCell(NewMegaCellCoords.X, NewMegaCellCoords.Y);
        if (NewMegaCell)
        {
            NewMegaCell->AddUnit(Unit);
            UnitToMegaCellMap.Add(Unit, NewMegaCellCoords);
        }
    }
    else
    {
        // Jednostka przemiescila siê poza granice siatki
        UnitToMegaCellMap.Remove(Unit);
        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Jednostka %s przemiescila siê poza granice siatki ==="), *Unit->GetName());
    }
}

/// <summary>
/// Zwraca wszystkie jednostki w okreslonym zasiêgu od danej pozycji.
/// Uzywa hierarchicznego podejscia : najpierw identyfikuje mega - komorki w zasiêgu,
/// nastêpnie sprawdza dokladna odleglosæ dla kazdej jednostki.
/// </summary>
/// <param name="Position">Pozycja srodkowa do wyszukiwania</param>
/// <param name="Range">Zasiêg wyszukiwania</param>
/// <returns>Tablica jednostek znajdujacych siê w zasiêgu</returns>
TArray<ABaseUnit*> USpatialGrid::GetUnitsInRange(FVector Position, float Range) const
{
    TArray<ABaseUnit*> UnitsInRange;

    if (Range <= 0 || MegaCells.Num() == 0)
    {
        return UnitsInRange;
    }

    // Obliczanie ktore mega-komorki sprawdziæ na podstawie zasiêgu
    FVector2D CenterMegaCell = GetMegaCellCoordinates(Position);
    int32 MegaCellRadius = FMath::CeilToInt(Range / MegaCellSize) + 1; // Dodanie 1 dla marginesu bezpieczenstwa

    // Sprawdzanie wszystkich mega-komorek w promieniu
    for (int32 mx = CenterMegaCell.X - MegaCellRadius; mx <= CenterMegaCell.X + MegaCellRadius; mx++)
    {
        for (int32 my = CenterMegaCell.Y - MegaCellRadius; my <= CenterMegaCell.Y + MegaCellRadius; my++)
        {
            if (IsValidMegaCellCoordinate(mx, my))
            {
                const FSpatialCell* MegaCell = GetMegaCell(mx, my);
                if (MegaCell && !MegaCell->IsEmpty())
                {
                    // Sprawdzanie kazdej jednostki w mega-komorce
                    for (ABaseUnit* Unit : MegaCell->Units)
                    {
                        if (Unit && Unit->bIsAlive)
                        {
                            float Distance = FVector::Dist(Position, Unit->GetActorLocation());
                            if (Distance <= Range)
                            {
                                UnitsInRange.Add(Unit);
                            }
                        }
                    }
                }
            }
        }
    }

    return UnitsInRange;
}

/// <summary>
/// Zwraca wszystkie zywe jednostki w okreslonej mega-komorce.
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y mega-komorki</param>
/// <returns>Tablica zywych jednostek w mega-komorce</returns>
TArray<ABaseUnit*> USpatialGrid::GetUnitsInMegaCell(int32 MegaCellX, int32 MegaCellY) const
{
    TArray<ABaseUnit*> CellUnits;

    if (IsValidMegaCellCoordinate(MegaCellX, MegaCellY))
    {
        const FSpatialCell* MegaCell = GetMegaCell(MegaCellX, MegaCellY);
        if (MegaCell)
        {
            // Zwracanie tylko zywych jednostek
            for (ABaseUnit* Unit : MegaCell->Units)
            {
                if (Unit && Unit->bIsAlive)
                {
                    CellUnits.Add(Unit);
                }
            }
        }
    }

    return CellUnits;
}

/// <summary>
/// Zwraca wszystkie jednostki znajdujace siê w okreslonej komorce bazowej.
/// Najpierw lokalizuje odpowiednia mega - komorkê, nastêpnie filtruje jednostki znajdujace siê w konkretnej komorce bazowej.
/// </summary>
/// <param name="BaseGridX">Wspolrzêdna X komorki bazowej</param>
/// <param name="BaseGridY">Wspolrzêdna Y komorki bazowej</param>
/// <returns>Tablica jednostek w komorce bazowej</returns>
TArray<ABaseUnit*> USpatialGrid::GetUnitsInBaseGridCell(int32 BaseGridX, int32 BaseGridY) const
{
    TArray<ABaseUnit*> CellUnits;

    if (!IsValidBaseGridCoordinate(BaseGridX, BaseGridY))
    {
        return CellUnits;
    }

    // Znalezienie mega-komorki zawierajacej tê komorkê bazowa
    FVector2D MegaCellCoords = BaseGridToMegaCell(BaseGridX, BaseGridY);
    const FSpatialCell* MegaCell = GetMegaCell(MegaCellCoords.X, MegaCellCoords.Y);

    if (!MegaCell)
    {
        return CellUnits;
    }

    // Obliczanie granic tej konkretnej komorki bazowej
    FVector2D BaseCellMin = WorldMin + FVector2D(BaseGridX * BaseGridCellSize, BaseGridY * BaseGridCellSize);
    FVector2D BaseCellMax = BaseCellMin + FVector2D(BaseGridCellSize, BaseGridCellSize);

    // Filtrowanie jednostek rzeczywiscie znajdujacych siê w tej komorce bazowej
    for (ABaseUnit* Unit : MegaCell->Units)
    {
        if (Unit && Unit->bIsAlive)
        {
            FVector UnitPos = Unit->GetActorLocation();
            FVector2D UnitPos2D(UnitPos.X, UnitPos.Y);

            if (UnitPos2D.X >= BaseCellMin.X && UnitPos2D.X < BaseCellMax.X &&
                UnitPos2D.Y >= BaseCellMin.Y && UnitPos2D.Y < BaseCellMax.Y)
            {
                CellUnits.Add(Unit);
            }
        }
    }

    return CellUnits;
}

/// <summary>
/// Znajduje wszystkie wrogie jednostki w okreslonym zasiêgu od danej jednostki.
/// </summary>
/// <param name="Unit">Jednostka wyszukujaca wrogow</param>
/// <param name="SearchRange">Zasiêg wyszukiwania</param>
/// <returns>Tablica wrogich jednostek w zasiêgu</returns>
TArray<ABaseUnit*> USpatialGrid::GetNearbyEnemies(ABaseUnit* Unit, float SearchRange) const
{
    TArray<ABaseUnit*> Enemies;

    if (!Unit || !Unit->bIsAlive)
    {
        return Enemies;
    }

    TArray<ABaseUnit*> NearbyUnits = GetUnitsInRange(Unit->GetActorLocation(), SearchRange);

    // Filtrowanie tylko wrogow
    for (ABaseUnit* NearbyUnit : NearbyUnits)
    {
        if (NearbyUnit && NearbyUnit != Unit && NearbyUnit->bIsAlive &&
            NearbyUnit->TeamID != Unit->TeamID)
        {
            Enemies.Add(NearbyUnit);
        }
    }

    return Enemies;
}

/// <summary>
/// Znajduje najblizszego wroga dla danej jednostki w okreslonym maksymalnym zasiêgu.
/// </summary>
/// <param name="Unit">Jednostka wyszukujaca najblizszego wroga</param>
/// <param name="MaxRange">Maksymalny zasiêg wyszukiwania</param>
/// <returns>Wskaznik do najblizszego wroga lub nullptr jesli nie znaleziono</returns>
ABaseUnit* USpatialGrid::FindNearestEnemy(ABaseUnit* Unit, float MaxRange) const
{
    if (!Unit || !Unit->bIsAlive)
    {
        return nullptr;
    }

    TArray<ABaseUnit*> NearbyEnemies = GetNearbyEnemies(Unit, MaxRange);

    if (NearbyEnemies.Num() == 0)
    {
        return nullptr;
    }

    ABaseUnit* NearestEnemy = nullptr;
    float NearestDistance = MaxRange + 1.0f;
    FVector UnitPosition = Unit->GetActorLocation();

    for (ABaseUnit* Enemy : NearbyEnemies)
    {
        if (Enemy && Enemy->bIsAlive)
        {
            float Distance = FVector::Dist(UnitPosition, Enemy->GetActorLocation());
            if (Distance < NearestDistance)
            {
                NearestDistance = Distance;
                NearestEnemy = Enemy;
            }
        }
    }

    if (NearestEnemy)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SPATIAL GRID: Jednostka %s znalazla najblizszego wroga %s w odleglosci %f ==="),
            *Unit->GetName(), *NearestEnemy->GetName(), NearestDistance);
    }

    return NearestEnemy;
}

/// <summary>
/// Obsluguje walki w okreslonej mega-komorce oraz z sasiednimi mega-komorkami.
/// Sprawdza wszystkie mozliwe pary jednostek wrogich w zasiêgu ataku.
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y mega-komorki</param>
void USpatialGrid::HandleCombatInMegaCell(int32 MegaCellX, int32 MegaCellY)
{
    if (!IsValidMegaCellCoordinate(MegaCellX, MegaCellY))
    {
        return;
    }

    FSpatialCell* MegaCell = GetMegaCell(MegaCellX, MegaCellY);
    if (!MegaCell || MegaCell->IsEmpty())
    {
        return;
    }

    HandleCombatInMegaCellInternal(MegaCell);

    // Sprawdzanie rowniez sasiednich mega-komorek dla walk miêdzykomorkowych
    TArray<FVector2D> NeighborCoords;
    GetNeighboringMegaCells(MegaCellX, MegaCellY, NeighborCoords);

    for (const FVector2D& NeighborCoord : NeighborCoords)
    {
        if (IsValidMegaCellCoordinate(NeighborCoord.X, NeighborCoord.Y))
        {
            const FSpatialCell* NeighborMegaCell = GetMegaCell(NeighborCoord.X, NeighborCoord.Y);
            if (NeighborMegaCell && !NeighborMegaCell->IsEmpty())
            {
                // Sprawdzanie walk miêdzy biezaca mega-komorka a sasiednia mega-komorka
                for (ABaseUnit* Unit : MegaCell->Units)
                {
                    if (Unit && Unit->bIsAlive && Unit->bAutoCombatEnabled)
                    {
                        for (ABaseUnit* NeighborUnit : NeighborMegaCell->Units)
                        {
                            if (NeighborUnit && NeighborUnit->bIsAlive &&
                                NeighborUnit->TeamID != Unit->TeamID)
                            {
                                float Distance = FVector::Dist(Unit->GetActorLocation(),
                                    NeighborUnit->GetActorLocation());
                                if (Distance <= Unit->AttackRange)
                                {
                                    if (Unit->CanAttackTarget(NeighborUnit))
                                    {
                                        Unit->PerformAttack(NeighborUnit);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/// <summary>
/// Przetwarza walki we wszystkich aktywnych mega-komorkach w siatce.
/// </summary>
void USpatialGrid::HandleAllCombat()
{
    // Przetwarzanie walk dla kazdej aktywnej mega-komorki
    for (int32 mx = 0; mx < MegaGridWidth; mx++)
    {
        for (int32 my = 0; my < MegaGridHeight; my++)
        {
            HandleCombatInMegaCell(mx, my);
        }
    }
}

/// <summary>
/// Wewnêtrzna funkcja obslugujaca walki miêdzy jednostkami w tej samej mega-komorce.
/// Sprawdza wszystkie pary jednostek i wykonuje ataki jesli sa w zasiêgu.
/// </summary>
/// <param name="MegaCell">Wskaznik do mega-komorki do przetworzenia</param>
void USpatialGrid::HandleCombatInMegaCellInternal(FSpatialCell* MegaCell)
{
    if (!MegaCell || MegaCell->IsEmpty())
    {
        return;
    }

    const TArray<ABaseUnit*>& CellUnits = MegaCell->Units;

    // Przetwarzanie walk wewnatrz mega-komorki
    for (int32 i = 0; i < CellUnits.Num(); i++)
    {
        ABaseUnit* Unit = CellUnits[i];
        if (!Unit || !Unit->bIsAlive || !Unit->bAutoCombatEnabled)
        {
            continue;
        }

        // Sprawdzanie przeciwko innym jednostkom w tej samej mega-komorce
        for (int32 j = i + 1; j < CellUnits.Num(); j++)
        {
            ABaseUnit* OtherUnit = CellUnits[j];
            if (!OtherUnit || !OtherUnit->bIsAlive || OtherUnit->TeamID == Unit->TeamID)
            {
                continue;
            }

            float Distance = FVector::Dist(Unit->GetActorLocation(), OtherUnit->GetActorLocation());

            // Sprawdzanie czy Unit moze zaatakowaæ OtherUnit
            if (Distance <= Unit->AttackRange && Unit->CanAttackTarget(OtherUnit))
            {
                Unit->PerformAttack(OtherUnit);
            }

            // Sprawdzanie czy OtherUnit moze zaatakowaæ Unit (sprawdzanie odwrotne)
            if (Distance <= OtherUnit->AttackRange && OtherUnit->CanAttackTarget(Unit))
            {
                OtherUnit->PerformAttack(Unit);
            }
        }
    }
}

/// <summary>
/// Konwertuje pozycjê swiata na wspolrzêdne mega-komorki.
/// </summary>
/// <param name="WorldPosition">Pozycja 3D w przestrzeni swiata</param>
/// <returns>Wspolrzêdne 2D mega-komorki (X, Y)</returns>
FVector2D USpatialGrid::GetMegaCellCoordinates(FVector WorldPosition) const
{
    FVector2D Position2D(WorldPosition.X, WorldPosition.Y);
    FVector2D RelativePos = Position2D - WorldMin;

    int32 MegaCellX = FMath::FloorToInt(RelativePos.X / MegaCellSize);
    int32 MegaCellY = FMath::FloorToInt(RelativePos.Y / MegaCellSize);

    // Ograniczenie do granic siatki
    MegaCellX = FMath::Clamp(MegaCellX, 0, MegaGridWidth - 1);
    MegaCellY = FMath::Clamp(MegaCellY, 0, MegaGridHeight - 1);

    return FVector2D(MegaCellX, MegaCellY);
}

/// <summary>
/// Konwertuje pozycjê swiata na wspolrzêdne komorki bazowej.
/// </summary>
/// <param name="WorldPosition">Pozycja 3D w przestrzeni swiata</param>
/// <returns>Wspolrzêdne 2D komorki bazowej (X, Y)</returns>
FVector2D USpatialGrid::GetBaseGridCoordinates(FVector WorldPosition) const
{
    FVector2D Position2D(WorldPosition.X, WorldPosition.Y);
    FVector2D RelativePos = Position2D - WorldMin;

    int32 BaseGridX = FMath::FloorToInt(RelativePos.X / BaseGridCellSize);
    int32 BaseGridY = FMath::FloorToInt(RelativePos.Y / BaseGridCellSize);

    // Ograniczenie do granic siatki
    BaseGridX = FMath::Clamp(BaseGridX, 0, BaseGridWidth - 1);
    BaseGridY = FMath::Clamp(BaseGridY, 0, BaseGridHeight - 1);

    return FVector2D(BaseGridX, BaseGridY);
}

/// <summary>
/// Konwertuje wspolrzêdne komorki bazowej na wspolrzêdne mega-komorki.
/// </summary>
/// <param name="BaseGridX">Wspolrzêdna X komorki bazowej</param>
/// <param name="BaseGridY">Wspolrzêdna Y komorki bazowej</param>
/// <returns>Wspolrzêdne 2D mega-komorki zawierajacej dana komorkê bazowa</returns>
FVector2D USpatialGrid::BaseGridToMegaCell(int32 BaseGridX, int32 BaseGridY) const
{
    int32 MegaCellX = BaseGridX / MegaCellsPerDimension;
    int32 MegaCellY = BaseGridY / MegaCellsPerDimension;

    // Ograniczenie do granic mega-siatki
    MegaCellX = FMath::Clamp(MegaCellX, 0, MegaGridWidth - 1);
    MegaCellY = FMath::Clamp(MegaCellY, 0, MegaGridHeight - 1);

    return FVector2D(MegaCellX, MegaCellY);
}

/// <summary>
/// Sprawdza czy podane wspolrzêdne mega-komorki sa prawidlowe.
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y mega-komorki</param>
/// <returns>true jesli wspolrzêdne sa w granicach mega-siatki</returns>
bool USpatialGrid::IsValidMegaCellCoordinate(int32 MegaCellX, int32 MegaCellY) const
{
    return MegaCellX >= 0 && MegaCellX < MegaGridWidth && MegaCellY >= 0 && MegaCellY < MegaGridHeight;
}

/// <summary>
/// Sprawdza czy podane wspolrzêdne komorki bazowej sa prawidlowe.
/// </summary>
/// <param name="BaseGridX">Wspolrzêdna X komorki bazowej</param>
/// <param name="BaseGridY">Wspolrzêdna Y komorki bazowej</param>
/// <returns>true jesli wspolrzêdne sa w granicach siatki bazowej</returns>
bool USpatialGrid::IsValidBaseGridCoordinate(int32 BaseGridX, int32 BaseGridY) const
{
    return BaseGridX >= 0 && BaseGridX < BaseGridWidth && BaseGridY >= 0 && BaseGridY < BaseGridHeight;
}

/// <summary>
/// Oblicza punkt centralny mega-komorki w przestrzeni swiata.
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y mega-komorki</param>
/// <returns>Pozycja 2D srodka mega-komorki</returns>
FVector2D USpatialGrid::GetMegaCellCenter(int32 MegaCellX, int32 MegaCellY) const
{
    if (!IsValidMegaCellCoordinate(MegaCellX, MegaCellY))
    {
        return FVector2D::ZeroVector;
    }

    const FSpatialCell* MegaCell = GetMegaCell(MegaCellX, MegaCellY);
    if (MegaCell)
    {
        return (MegaCell->MinBounds + MegaCell->MaxBounds) * 0.5f;
    }

    return FVector2D::ZeroVector;
}

/// <summary>
/// Zwraca calkowita liczbê jednostek we wszystkich mega-komorkach.
/// </summary>
/// <returns>Calkowita liczba jednostek w siatce</returns>
int32 USpatialGrid::GetTotalUnitCount() const
{
    int32 TotalCount = 0;
    for (const FSpatialCell& MegaCell : MegaCells)
    {
        TotalCount += MegaCell.GetUnitCount();
    }
    return TotalCount;
}

/// <summary>
/// Zwraca liczbê mega-komorek zawierajacych przynajmniej jedna jednostkê.
/// </summary>
/// <returns>Liczba aktywnych (niepustych) mega-komorek</returns>
int32 USpatialGrid::GetActiveMegaCellCount() const
{
    int32 ActiveCount = 0;
    for (const FSpatialCell& MegaCell : MegaCells)
    {
        if (!MegaCell.IsEmpty())
        {
            ActiveCount++;
        }
    }
    return ActiveCount;
}

/// <summary>
/// Wyswietla szczegolowe statystyki siatki przestrzennej w logach.
/// </summary>
void USpatialGrid::DebugPrintGridStats() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== STATYSTYKI SIATKI PRZESTRZENNEJ ==="));
    UE_LOG(LogTemp, Warning, TEXT("Siatka bazowa: %dx%d komorek (rozmiar komorki: %f)"), BaseGridWidth, BaseGridHeight, BaseGridCellSize);
    UE_LOG(LogTemp, Warning, TEXT("Mega-siatka: %dx%d mega-komorek (%d calkowita liczba mega-komorek)"), MegaGridWidth, MegaGridHeight, MegaCells.Num());
    UE_LOG(LogTemp, Warning, TEXT("Rozmiar mega-komorki: %f"), MegaCellSize);
    UE_LOG(LogTemp, Warning, TEXT("Calkowita liczba jednostek: %d"), GetTotalUnitCount());
    UE_LOG(LogTemp, Warning, TEXT("Aktywne mega-komorki: %d"), GetActiveMegaCellCount());

    // Wyswietlanie rozkladu jednostek na mega-komorkê
    TMap<int32, int32> UnitDistribution;
    for (const FSpatialCell& MegaCell : MegaCells)
    {
        int32 UnitCount = MegaCell.GetUnitCount();
        if (UnitDistribution.Contains(UnitCount))
        {
            UnitDistribution[UnitCount]++;
        }
        else
        {
            UnitDistribution.Add(UnitCount, 1);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== Rozklad jednostek na mega-komorkê ==="));
    for (const auto& Pair : UnitDistribution)
    {
        UE_LOG(LogTemp, Warning, TEXT("Mega-komorki z %d jednostkami: %d"), Pair.Key, Pair.Value);
    }

    // Wyswietlanie szczegolow zapelnionych mega-komorek
    UE_LOG(LogTemp, Warning, TEXT("=== Zapelnione mega-komorki ==="));
    for (const FSpatialCell& MegaCell : MegaCells)
    {
        if (!MegaCell.IsEmpty())
        {
            int32 BaseCellsCovered = (MegaCell.BaseGridEndX - MegaCell.BaseGridStartX + 1) *
                (MegaCell.BaseGridEndY - MegaCell.BaseGridStartY + 1);
            UE_LOG(LogTemp, Warning, TEXT("Mega-komorka (%d,%d): %d jednostek, pokrywa %d komorek bazowych [(%d,%d) do (%d,%d)]"),
                MegaCell.MegaCellX, MegaCell.MegaCellY, MegaCell.GetUnitCount(), BaseCellsCovered,
                MegaCell.BaseGridStartX, MegaCell.BaseGridStartY,
                MegaCell.BaseGridEndX, MegaCell.BaseGridEndY);
        }
    }
}

/// <summary>
/// Rysuje wizualizacjê debugowa siatki w swiecie gry.
/// </summary>
/// <param name="World">Wskaznik do swiata gry</param>
/// <param name="Duration">Czas wyswietlania debugowania w sekundach</param>
void USpatialGrid::DebugDrawGrid(UWorld* World, float Duration) const
{
    if (!World)
    {
        return;
    }

    // Rysowanie linii siatki bazowej (cienkie, zielone)
    for (int32 x = 0; x <= BaseGridWidth; x++)
    {
        FVector Start(WorldMin.X + x * BaseGridCellSize, WorldMin.Y, 0);
        FVector End(WorldMin.X + x * BaseGridCellSize, WorldMax.Y, 0);
        DrawDebugLine(World, Start, End, FColor::Green, false, Duration, 0, 1.0f);
    }

    for (int32 y = 0; y <= BaseGridHeight; y++)
    {
        FVector Start(WorldMin.X, WorldMin.Y + y * BaseGridCellSize, 0);
        FVector End(WorldMax.X, WorldMin.Y + y * BaseGridCellSize, 0);
        DrawDebugLine(World, Start, End, FColor::Green, false, Duration, 0, 1.0f);
    }

    // Rysowanie granic mega-komorek (grube, czerwone)
    DebugDrawMegaCellBoundaries(World, Duration);

    // Rysowanie liczby jednostek w kazdej mega-komorce
    for (int32 mx = 0; mx < MegaGridWidth; mx++)
    {
        for (int32 my = 0; my < MegaGridHeight; my++)
        {
            const FSpatialCell* MegaCell = GetMegaCell(mx, my);
            if (MegaCell && !MegaCell->IsEmpty())
            {
                FVector2D MegaCellCenter2D = GetMegaCellCenter(mx, my);
                FVector MegaCellCenter(MegaCellCenter2D.X, MegaCellCenter2D.Y, 100);

                FString UnitCountText = FString::Printf(TEXT("MK(%d,%d): %d jedn."), mx, my, MegaCell->GetUnitCount());
                DrawDebugString(World, MegaCellCenter, UnitCountText, nullptr, FColor::Yellow, Duration, true, 1.5f);
            }
        }
    }
}

/// <summary>
/// Rysuje granice mega-komorek i ich etykiety w swiecie gry.
/// </summary>
/// <param name="World">Wskaznik do swiata gry</param>
/// <param name="Duration">Czas wyswietlania debugowania w sekundach</param>
void USpatialGrid::DebugDrawMegaCellBoundaries(UWorld* World, float Duration) const
{
    if (!World)
    {
        return;
    }

    // Rysowanie linii siatki mega-komorek (grube, czerwone/pomaranczowe)
    for (int32 mx = 0; mx <= MegaGridWidth; mx++)
    {
        float WorldX = WorldMin.X + mx * MegaCellSize;
        FVector Start(WorldX, WorldMin.Y, 10);
        FVector End(WorldX, WorldMax.Y, 10);
        DrawDebugLine(World, Start, End, FColor::Red, false, Duration, 0, 5.0f);
    }

    for (int32 my = 0; my <= MegaGridHeight; my++)
    {
        float WorldY = WorldMin.Y + my * MegaCellSize;
        FVector Start(WorldMin.X, WorldY, 10);
        FVector End(WorldMax.X, WorldY, 10);
        DrawDebugLine(World, Start, End, FColor::Red, false, Duration, 0, 5.0f);
    }

    // Etykietowanie kazdej mega-komorki
    for (int32 mx = 0; mx < MegaGridWidth; mx++)
    {
        for (int32 my = 0; my < MegaGridHeight; my++)
        {
            const FSpatialCell* MegaCell = GetMegaCell(mx, my);
            if (MegaCell)
            {
                FVector2D Center = GetMegaCellCenter(mx, my);
                FVector TextPos(Center.X, Center.Y, 50);

                FString Label = FString::Printf(TEXT("MK(%d,%d)\nBaza: (%d,%d)-(%d,%d)"),
                    mx, my,
                    MegaCell->BaseGridStartX, MegaCell->BaseGridStartY,
                    MegaCell->BaseGridEndX, MegaCell->BaseGridEndY);

                DrawDebugString(World, TextPos, Label, nullptr, FColor::Orange, Duration, true, 1.2f);
            }
        }
    }
}

/// <summary>
/// Konwertuje wspolrzêdne mega-komorki na indeks w tablicy MegaCells.
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y mega-komorki</param>
/// <returns>Indeks tablicy lub -1 jesli wspolrzêdne sa nieprawidlowe</returns>
int32 USpatialGrid::GetMegaCellIndex(int32 MegaCellX, int32 MegaCellY) const
{
    if (!IsValidMegaCellCoordinate(MegaCellX, MegaCellY))
    {
        return -1;
    }
    return MegaCellY * MegaGridWidth + MegaCellX;
}


/// <summary>
/// Pobiera wskaznik do mega-komorki na podstawie wspolrzêdnych
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y mega-komorki</param>
/// <returns>Wskaznik do mega-komorki lub nullptr jesli wspolrzêdne sa nieprawidlowe</returns>
FSpatialCell* USpatialGrid::GetMegaCell(int32 MegaCellX, int32 MegaCellY)
{
    int32 Index = GetMegaCellIndex(MegaCellX, MegaCellY);
    if (Index >= 0 && Index < MegaCells.Num())
    {
        return &MegaCells[Index];
    }
    return nullptr;
}

/// <summary>
/// Pobiera wskaznik do mega-komorki na podstawie wspolrzêdnych.
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y mega-komorki</param>
/// <returns>Staly wskaznik do mega-komorki lub nullptr jesli wspolrzêdne sa nieprawidlowe</returns>
const FSpatialCell* USpatialGrid::GetMegaCell(int32 MegaCellX, int32 MegaCellY) const
{
    int32 Index = GetMegaCellIndex(MegaCellX, MegaCellY);
    if (Index >= 0 && Index < MegaCells.Num())
    {
        return &MegaCells[Index];
    }
    return nullptr;
}

/// <summary>
/// Pobiera wspolrzêdne sasiednich mega-komorek.
/// Sprawdza 4 sasiednie komorki(bez przekatnych) aby uniknaæ duplikatow sprawdzen.
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X centralnej mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y centralnej mega-komorki</param>
/// <param name="NeighborCoords">Tablica wyjsciowa zawierajaca wspolrzêdne sasiadow</param>
void USpatialGrid::GetNeighboringMegaCells(int32 MegaCellX, int32 MegaCellY, TArray<FVector2D>& NeighborCoords) const
{
    NeighborCoords.Empty();

    // Sprawdzanie 4 przyleglych mega-komorek (bez przekatnych) aby uniknaæ duplikatow sprawdzen
    const TArray<FVector2D> Directions = {
        FVector2D(-1, -1),  // Lewy gorny
        FVector2D(-1, 0),   // Lewy
        FVector2D(0, -1),   // Gorny
        FVector2D(-1, 1)    // Lewy dolny
    };

    for (const FVector2D& Direction : Directions)
    {
        int32 NeighborX = MegaCellX + Direction.X;
        int32 NeighborY = MegaCellY + Direction.Y;

        if (IsValidMegaCellCoordinate(NeighborX, NeighborY))
        {
            NeighborCoords.Add(FVector2D(NeighborX, NeighborY));
        }
    }
}

/// <summary>
/// Sprawdza czy mega-komorka znajduje siê w granicach mega-siatki.
/// </summary>
/// <param name="MegaCellX">Wspolrzêdna X mega-komorki</param>
/// <param name="MegaCellY">Wspolrzêdna Y mega-komorki</param>
/// <returns>true jesli mega-komorka jest w granicach</returns>
bool USpatialGrid::IsWithinMegaGridBounds(int32 MegaCellX, int32 MegaCellY) const
{
    return IsValidMegaCellCoordinate(MegaCellX, MegaCellY);
}

/// <summary>
/// Czysci wszystkie jednostki z siatki przestrzennej.
/// Usuwa jednostki ze wszystkich mega - komorek i resetuje mapowanie jednostek.
/// </summary>
void USpatialGrid::ClearGrid()
{
    for (FSpatialCell& MegaCell : MegaCells)
    {
        MegaCell.Units.Empty();
    }
    UnitToMegaCellMap.Empty();
}