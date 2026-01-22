// SpatialGridTests.cpp - Testy automatyczne dla systemu SpatialGrid
#include "Misc/AutomationTest.h"
#include "SpatialGrid.h"
#include "BaseUnit.h"
#include "Tests/AutomationCommon.h"

// Test 1: Inicjalizacja siatki i podstawowe obliczenia
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialGridInitializationTest, 
    "Game.SpatialGrid.Initialization", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSpatialGridInitializationTest::RunTest(const FString& Parameters)
{
    // Arrange - przygotowanie
    USpatialGrid* Grid = NewObject<USpatialGrid>();
    FVector2D WorldMin(0.0f, 0.0f);
    FVector2D WorldMax(3000.0f, 3000.0f);
    float CellSize = 200.0f;

    // Act - wykonanie
    Grid->InitializeGrid(WorldMin, WorldMax, CellSize);

    // Assert - sprawdzenie wyników używając publicznych metod
    
    // Test 1: Sprawdź całkowitą liczbę jednostek (powinna być 0 po inicjalizacji)
    TestEqual(TEXT("Początkowa liczba jednostek powinna wynosić 0"), Grid->GetTotalUnitCount(), 0);
    
    // Test 2: Sprawdź liczbę aktywnych mega-komórek (powinna być 0)
    TestEqual(TEXT("Początkowa liczba aktywnych mega-komórek powinna wynosić 0"), Grid->GetActiveMegaCellCount(), 0);
    
    // Test 3: Sprawdź prawidłowość współrzędnych mega-komórki
    TestTrue(TEXT("Współrzędne (0,0) powinny być prawidłowe"), Grid->IsValidMegaCellCoordinate(0, 0));
    TestTrue(TEXT("Współrzędne (4,4) powinny być prawidłowe"), Grid->IsValidMegaCellCoordinate(4, 4));
    TestFalse(TEXT("Współrzędne (10,10) powinny być nieprawidłowe"), Grid->IsValidMegaCellCoordinate(10, 10));
    
    // Test 4: Sprawdź prawidłowość współrzędnych siatki bazowej
    TestTrue(TEXT("Współrzędne bazowe (0,0) powinny być prawidłowe"), Grid->IsValidBaseGridCoordinate(0, 0));
    TestTrue(TEXT("Współrzędne bazowe (14,14) powinny być prawidłowe"), Grid->IsValidBaseGridCoordinate(14, 14));
    TestFalse(TEXT("Współrzędne bazowe (20,20) powinny być nieprawidłowe"), Grid->IsValidBaseGridCoordinate(20, 20));

    return true;
}

// Test 2: Konwersja współrzędnych
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialGridCoordinateConversionTest, 
    "Game.SpatialGrid.CoordinateConversion", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSpatialGridCoordinateConversionTest::RunTest(const FString& Parameters)
{
    // Arrange
    USpatialGrid* Grid = NewObject<USpatialGrid>();
    FVector2D WorldMin(0.0f, 0.0f);
    FVector2D WorldMax(3000.0f, 3000.0f);
    float CellSize = 200.0f;
    Grid->InitializeGrid(WorldMin, WorldMax, CellSize);

    // Test 1: Konwersja pozycji świata na współrzędne komórki bazowej
    FVector TestPosition1(250.0f, 450.0f, 0.0f); // Powinno być w komórce (1, 2)
    FVector2D BaseCoords = Grid->GetBaseGridCoordinates(TestPosition1);
    
    TestEqual(TEXT("BaseGrid X dla pozycji (250, 450)"), (int32)BaseCoords.X, 1);
    TestEqual(TEXT("BaseGrid Y dla pozycji (250, 450)"), (int32)BaseCoords.Y, 2);

    // Test 2: Konwersja pozycji świata na współrzędne mega-komórki
    FVector TestPosition2(1500.0f, 1200.0f, 0.0f); // Powinno być w mega-komórce (2, 2)
    FVector2D MegaCoords = Grid->GetMegaCellCoordinates(TestPosition2);
    
    TestEqual(TEXT("MegaCell X dla pozycji (1500, 1200)"), (int32)MegaCoords.X, 2);
    TestEqual(TEXT("MegaCell Y dla pozycji (1500, 1200)"), (int32)MegaCoords.Y, 2);

    // Test 3: Konwersja komórki bazowej na mega-komórkę
    // Komórka bazowa (7, 8) powinna być w mega-komórce (2, 2)
    FVector2D ConvertedMega = Grid->BaseGridToMegaCell(7, 8);
    
    TestEqual(TEXT("Konwersja bazowej (7,8) na mega X"), (int32)ConvertedMega.X, 2);
    TestEqual(TEXT("Konwersja bazowej (7,8) na mega Y"), (int32)ConvertedMega.Y, 2);

    // Test 4: Walidacja współrzędnych
    TestTrue(TEXT("Współrzędne mega (2,2) powinny być prawidłowe"), 
        Grid->IsValidMegaCellCoordinate(2, 2));
    TestFalse(TEXT("Współrzędne mega (10,10) powinny być nieprawidłowe"), 
        Grid->IsValidMegaCellCoordinate(10, 10));
    
    TestTrue(TEXT("Współrzędne bazowe (7,8) powinny być prawidłowe"), 
        Grid->IsValidBaseGridCoordinate(7, 8));
    TestFalse(TEXT("Współrzędne bazowe (20,20) powinny być nieprawidłowe"), 
        Grid->IsValidBaseGridCoordinate(20, 20));

    // Test 5: Pozycje brzegowe
    FVector EdgePosition(0.0f, 0.0f, 0.0f); // Lewy dolny róg
    FVector2D EdgeCoords = Grid->GetMegaCellCoordinates(EdgePosition);
    TestEqual(TEXT("Lewy dolny róg - Mega X"), (int32)EdgeCoords.X, 0);
    TestEqual(TEXT("Lewy dolny róg - Mega Y"), (int32)EdgeCoords.Y, 0);

    FVector MaxEdgePosition(2999.0f, 2999.0f, 0.0f); // Prawy górny róg
    FVector2D MaxEdgeCoords = Grid->GetMegaCellCoordinates(MaxEdgePosition);
    TestEqual(TEXT("Prawy górny róg - Mega X"), (int32)MaxEdgeCoords.X, 4);
    TestEqual(TEXT("Prawy górny róg - Mega Y"), (int32)MaxEdgeCoords.Y, 4);

    return true;
}

// Test 3: Dodawanie i usuwanie jednostek
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialGridUnitManagementTest, 
    "Game.SpatialGrid.UnitManagement", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSpatialGridUnitManagementTest::RunTest(const FString& Parameters)
{
    // Arrange
    USpatialGrid* Grid = NewObject<USpatialGrid>();
    FVector2D WorldMin(0.0f, 0.0f);
    FVector2D WorldMax(3000.0f, 3000.0f);
    float CellSize = 200.0f;
    Grid->InitializeGrid(WorldMin, WorldMax, CellSize);

    // Test początkowego stanu
    TestEqual(TEXT("Początkowa liczba jednostek powinna wynosić 0"), 
        Grid->GetTotalUnitCount(), 0);
    TestEqual(TEXT("Początkowa liczba aktywnych mega-komórek powinna wynosić 0"), 
        Grid->GetActiveMegaCellCount(), 0);

    // Test pobierania jednostek w różnych obszarach
    TArray<ABaseUnit*> UnitsInCell = Grid->GetUnitsInMegaCell(0, 0);
    TestEqual(TEXT("Pusta mega-komórka powinna zwrócić 0 jednostek"), UnitsInCell.Num(), 0);

    // Test wyszukiwania jednostek w zasięgu
    FVector TestPosition(500.0f, 500.0f, 0.0f);
    TArray<ABaseUnit*> UnitsInRange = Grid->GetUnitsInRange(TestPosition, 1000.0f);
    TestEqual(TEXT("Wyszukiwanie w pustej siatce powinno zwrócić 0 jednostek"), UnitsInRange.Num(), 0);

    return true;
}

// Test 4: Obliczenia centrum mega-komórki
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialGridMegaCellCenterTest, 
    "Game.SpatialGrid.MegaCellCenter", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSpatialGridMegaCellCenterTest::RunTest(const FString& Parameters)
{
    // Arrange
    USpatialGrid* Grid = NewObject<USpatialGrid>();
    FVector2D WorldMin(0.0f, 0.0f);
    FVector2D WorldMax(3000.0f, 3000.0f);
    float CellSize = 200.0f;
    Grid->InitializeGrid(WorldMin, WorldMax, CellSize);

    // Test centrum mega-komórki (0,0)
    // Granice: (0,0) do (600,600), centrum: (300, 300)
    FVector2D Center00 = Grid->GetMegaCellCenter(0, 0);
    TestEqual(TEXT("Centrum mega-komórki (0,0) X"), (float)Center00.X, 300.0f);
    TestEqual(TEXT("Centrum mega-komórki (0,0) Y"), (float)Center00.Y, 300.0f);

    // Test centrum mega-komórki (1,1)
    // Granice: (600,600) do (1200,1200), centrum: (900, 900)
    FVector2D Center11 = Grid->GetMegaCellCenter(1, 1);
    TestEqual(TEXT("Centrum mega-komórki (1,1) X"), (float)Center11.X, 900.0f);
    TestEqual(TEXT("Centrum mega-komórki (1,1) Y"), (float)Center11.Y, 900.0f);

    return true;
}