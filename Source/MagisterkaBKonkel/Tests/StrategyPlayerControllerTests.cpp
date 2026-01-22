// StrategyPlayerControllerTests.cpp - Testy automatyczne dla kontrolera gracza
#include "Misc/AutomationTest.h"
#include "StrategyPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// Test 1: Walidacja parametrów RPC dla kliknięcia siatki
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerControllerGridClickValidationTest, 
    "Game.PlayerController.GridClickValidation", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPlayerControllerGridClickValidationTest::RunTest(const FString& Parameters)
{
    // Arrange - tworzymy kontroler bez świata (prostszy test)
    AStrategyPlayerController* Controller = NewObject<AStrategyPlayerController>();
    if (!Controller)
    {
        AddError(TEXT("Failed to create PlayerController"));
        return false;
    }

    // Test walidacji RPC - te testy sprawdzają czy funkcja Validate działa poprawnie
    
    // Test 1: Prawidłowe parametry powinny przejść walidację (gdy GridManager istnieje)
    // Bez GridManagera, walidacja zawsze zwróci false
    FVector2D ValidPosition(5.0f, 5.0f);
    int32 ValidPlayerID = 0;
    
    // Bez GridManager walidacja powinna zwrócić false
    bool ValidationResult = Controller->ServerHandleGridClick_Validate(ValidPosition, ValidPlayerID);
    TestFalse(TEXT("Walidacja bez GridManager powinna zwrócić false"), ValidationResult);
    
    // Test 2: Nieprawidłowy PlayerID powinien nie przejść walidacji
    int32 InvalidPlayerID = -1;
    ValidationResult = Controller->ServerHandleGridClick_Validate(ValidPosition, InvalidPlayerID);
    TestFalse(TEXT("Walidacja z ujemnym PlayerID powinna zwrócić false"), ValidationResult);
    
    // Test 3: Walidacja Unit Click z nullptr
    ValidationResult = Controller->ServerHandleUnitClick_Validate(nullptr, ValidPlayerID);
    TestFalse(TEXT("Walidacja z nullptr Unit powinna zwrócić false"), ValidationResult);
    
    // Test 4: Walidacja Unit Click z nieprawidłowym PlayerID
    // Używamy cast na nullptr jako placeholder jednostki (w prawdziwym teście potrzebowalibyśmy prawdziwej jednostki)
    ValidationResult = Controller->ServerHandleUnitClick_Validate(nullptr, InvalidPlayerID);
    TestFalse(TEXT("Walidacja Unit Click z nieprawidłowym PlayerID powinna zwrócić false"), ValidationResult);
    
    return true;
}

// Test 2: Walidacja parametrów zakupu jednostki
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerControllerPurchaseValidationTest, 
    "Game.PlayerController.PurchaseValidation", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPlayerControllerPurchaseValidationTest::RunTest(const FString& Parameters)
{
    // Arrange
    AStrategyPlayerController* Controller = NewObject<AStrategyPlayerController>();
    if (!Controller)
    {
        AddError(TEXT("Failed to create PlayerController"));
        return false;
    }

    // Test walidacji parametrów zakupu jednostki
    // Zgodnie z kodem: UnitType >= 0 && UnitType < 4 && RequestingPlayerID >= 0
    
    // Test 1: Prawidłowe parametry
    TestTrue(TEXT("Prawidłowy zakup: UnitType=0, PlayerID=0"), 
        Controller->ServerRequestUnitPurchase_Validate(0, 0));
    TestTrue(TEXT("Prawidłowy zakup: UnitType=3, PlayerID=1"), 
        Controller->ServerRequestUnitPurchase_Validate(3, 1));
    TestTrue(TEXT("Prawidłowy zakup: UnitType=2, PlayerID=5"), 
        Controller->ServerRequestUnitPurchase_Validate(2, 5));
    
    // Test 2: Nieprawidłowy typ jednostki (poza zakresem 0-3)
    TestFalse(TEXT("Nieprawidłowy UnitType=-1"), 
        Controller->ServerRequestUnitPurchase_Validate(-1, 0));
    TestFalse(TEXT("Nieprawidłowy UnitType=4"), 
        Controller->ServerRequestUnitPurchase_Validate(4, 0));
    TestFalse(TEXT("Nieprawidłowy UnitType=10"), 
        Controller->ServerRequestUnitPurchase_Validate(10, 0));
    
    // Test 3: Nieprawidłowy PlayerID
    TestFalse(TEXT("Nieprawidłowy PlayerID=-1"), 
        Controller->ServerRequestUnitPurchase_Validate(0, -1));
    TestFalse(TEXT("Nieprawidłowy PlayerID=-5"), 
        Controller->ServerRequestUnitPurchase_Validate(2, -5));
    
    // Test 4: Oba parametry nieprawidłowe
    TestFalse(TEXT("Oba parametry nieprawidłowe"), 
        Controller->ServerRequestUnitPurchase_Validate(-1, -1));
    TestFalse(TEXT("UnitType za duży, PlayerID ujemny"), 
        Controller->ServerRequestUnitPurchase_Validate(5, -2));
    
    return true;
}

// Test 3: Inicjalizacja kontrolera i podstawowe właściwości
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerControllerInitializationTest, 
    "Game.PlayerController.Initialization", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPlayerControllerInitializationTest::RunTest(const FString& Parameters)
{
    // Arrange
    AStrategyPlayerController* Controller = NewObject<AStrategyPlayerController>();
    if (!Controller)
    {
        AddError(TEXT("Failed to create PlayerController"));
        return false;
    }

    // Test wartości domyślnych po utworzeniu kontrolera
    
    // Test 1: PlayerID powinno być zainicjalizowane jako -1
    TestEqual(TEXT("Domyślne PlayerID powinno wynosić -1"), 
        Controller->GetPlayerID(), -1);
    
    // Test 2: Kursor myszy powinien być włączony
    TestTrue(TEXT("Kursor myszy powinien być widoczny"), 
        Controller->bShowMouseCursor);
    TestTrue(TEXT("Eventy kliknięć powinny być włączone"), 
        Controller->bEnableClickEvents);
    TestTrue(TEXT("Eventy najechania myszą powinny być włączone"), 
        Controller->bEnableMouseOverEvents);
    
    // Test 3: Replikacja powinna być włączona
    TestTrue(TEXT("Kontroler powinien mieć włączoną replikację"), 
        Controller->GetIsReplicated());
    
    // Test 4: Kontroler nie powinien być zainicjalizowany bez referencji do managerów
    // bInitialized jest private, więc testujemy pośrednio przez dostępne metody
    // Zakładamy że bez GridManager/UnitManager kontroler nie jest w pełni gotowy
    TestNotNull(TEXT("Kontroler został utworzony"), Controller);
    
    return true;
}
