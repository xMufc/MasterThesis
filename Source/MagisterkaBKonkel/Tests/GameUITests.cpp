// GameUITests.cpp - Testy automatyczne dla interfejsu użytkownika
#include "Misc/AutomationTest.h"
#include "GameUI.h"
#include "Tests/AutomationCommon.h"

// Test 1: Zarządzanie stanem sklepu
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUIShopStateTest, 
    "Game.UI.ShopState", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FGameUIShopStateTest::RunTest(const FString& Parameters)
{
    // Arrange
    UGameUI* UI = NewObject<UGameUI>();
    if (!UI)
    {
        AddError(TEXT("Failed to create GameUI"));
        return false;
    }

    // Test 1: Sklep jest domyślnie wyłączony
    TestFalse(TEXT("Sklep powinien być wyłączony po utworzeniu"), 
        UI->GetShopEnabled());
    
    // Test 2: Włączanie sklepu
    UI->SetShopEnabled(true);
    TestTrue(TEXT("Sklep powinien być włączony po SetShopEnabled(true)"), 
        UI->GetShopEnabled());
    
    // Test 3: Wyłączanie sklepu
    UI->SetShopEnabled(false);
    TestFalse(TEXT("Sklep powinien być wyłączony po SetShopEnabled(false)"), 
        UI->GetShopEnabled());
    
    // Test 4: Przełączanie stanu sklepu wielokrotnie
    UI->SetShopEnabled(true);
    TestTrue(TEXT("Sklep powinien być włączony (1)"), UI->GetShopEnabled());
    
    UI->SetShopEnabled(false);
    TestFalse(TEXT("Sklep powinien być wyłączony (1)"), UI->GetShopEnabled());
    
    UI->SetShopEnabled(true);
    TestTrue(TEXT("Sklep powinien być włączony (2)"), UI->GetShopEnabled());
    
    UI->SetShopEnabled(false);
    TestFalse(TEXT("Sklep powinien być wyłączony (2)"), UI->GetShopEnabled());

    return true;
}

// Test 2: Logika zarządzania złotem i zakupami jednostek
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUIGoldManagementTest, 
    "Game.UI.GoldManagement", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FGameUIGoldManagementTest::RunTest(const FString& Parameters)
{
    // Arrange
    UGameUI* UI = NewObject<UGameUI>();
    if (!UI)
    {
        AddError(TEXT("Failed to create GameUI"));
        return false;
    }

    // Konfiguracja testowych jednostek
    TArray<FUnitShopData> TestUnits;
    
    FUnitShopData Unit1;
    Unit1.UnitName = TEXT("Tank");
    Unit1.UnitCost = 50;
    Unit1.UnitType = 0;
    TestUnits.Add(Unit1);
    
    FUnitShopData Unit2;
    Unit2.UnitName = TEXT("Ninja");
    Unit2.UnitCost = 75;
    Unit2.UnitType = 1;
    TestUnits.Add(Unit2);
    
    FUnitShopData Unit3;
    Unit3.UnitName = TEXT("Sword");
    Unit3.UnitCost = 100;
    Unit3.UnitType = 2;
    TestUnits.Add(Unit3);
    
    FUnitShopData Unit4;
    Unit4.UnitName = TEXT("Armor");
    Unit4.UnitCost = 125;
    Unit4.UnitType = 3;
    TestUnits.Add(Unit4);
    
    UI->SetupUnitShop(TestUnits);

    // Test 1: Gracz z 1000 złota powinien móc kupić wszystkie jednostki
    UI->UpdateGold(1000);
    TestTrue(TEXT("Gracz powinien móc kupić Unit1 (50 złota)"), 
        UI->CanAffordUnit(0));
    TestTrue(TEXT("Gracz powinien móc kupić Unit2 (75 złota)"), 
        UI->CanAffordUnit(1));
    TestTrue(TEXT("Gracz powinien móc kupić Unit3 (100 złota)"), 
        UI->CanAffordUnit(2));
    TestTrue(TEXT("Gracz powinien móc kupić Unit4 (125 złota)"), 
        UI->CanAffordUnit(3));
    
    // Test 2: Gracz z 80 złota powinien móc kupić tylko Unit1 i Unit2
    UI->UpdateGold(80);
    TestTrue(TEXT("Gracz powinien móc kupić Unit1 (50 złota)"), 
        UI->CanAffordUnit(0));
    TestTrue(TEXT("Gracz powinien móc kupić Unit2 (75 złota)"), 
        UI->CanAffordUnit(1));
    TestFalse(TEXT("Gracz NIE powinien móc kupić Unit3 (100 złota)"), 
        UI->CanAffordUnit(2));
    TestFalse(TEXT("Gracz NIE powinien móc kupić Unit4 (125 złota)"), 
        UI->CanAffordUnit(3));
    
    // Test 3: Gracz z 0 złota nie powinien móc kupić żadnej jednostki
    UI->UpdateGold(0);
    TestFalse(TEXT("Gracz bez złota NIE powinien móc kupić Unit1"), 
        UI->CanAffordUnit(0));
    TestFalse(TEXT("Gracz bez złota NIE powinien móc kupić Unit2"), 
        UI->CanAffordUnit(1));
    TestFalse(TEXT("Gracz bez złota NIE powinien móc kupić Unit3"), 
        UI->CanAffordUnit(2));
    TestFalse(TEXT("Gracz bez złota NIE powinien móc kupić Unit4"), 
        UI->CanAffordUnit(3));
    
    // Test 4: Test granicznych przypadków
    UI->UpdateGold(50);  // Dokładnie tyle ile kosztuje Unit1
    TestTrue(TEXT("Gracz z 50 złota powinien móc kupić Unit1 (50 złota)"), 
        UI->CanAffordUnit(0));
    
    UI->UpdateGold(49);  // O 1 złoto za mało
    TestFalse(TEXT("Gracz z 49 złota NIE powinien móc kupić Unit1 (50 złota)"), 
        UI->CanAffordUnit(0));
    
    // Test 5: Nieprawidłowe indeksy
    TestFalse(TEXT("Nieprawidłowy indeks -1 powinien zwrócić false"), 
        UI->CanAffordUnit(-1));
    TestFalse(TEXT("Nieprawidłowy indeks 10 powinien zwrócić false"), 
        UI->CanAffordUnit(10));
    
    // Test 6: Interakcja sklepu z możliwością zakupu
    UI->UpdateGold(100);
    UI->SetShopEnabled(false);
    // Nawet jeśli gracz ma złoto, gdy sklep jest wyłączony, nadal CanAffordUnit 
    // powinno działać (to tylko sprawdza złoto, nie stan sklepu)
    TestTrue(TEXT("CanAffordUnit sprawdza tylko złoto, nie stan sklepu"), 
        UI->CanAffordUnit(0));

    return true;
}

// Test 3: Zarządzanie timerem fazy
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUITimerManagementTest, 
    "Game.UI.TimerManagement", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FGameUITimerManagementTest::RunTest(const FString& Parameters)
{
    // Arrange
    UGameUI* UI = NewObject<UGameUI>();
    if (!UI)
    {
        AddError(TEXT("Failed to create GameUI"));
        return false;
    }

    // Test 1: Rozpoczęcie i zatrzymanie timera
    UI->StartPhaseTimer(60.0f);
    // Timer powinien być aktywny ale nie możemy tego sprawdzić bez gettera
    // Więc testujemy pośrednio poprzez inne metody
    
    UI->StopPhaseTimer();
    // Timer powinien być zatrzymany
    
    // Test 2: Wielokrotne uruchamianie timera
    UI->StartPhaseTimer(30.0f);
    UI->StartPhaseTimer(45.0f);
    UI->StartPhaseTimer(90.0f);
    
    // Test 3: Zatrzymanie i ponowne uruchomienie
    UI->StopPhaseTimer();
    UI->StartPhaseTimer(120.0f);
    UI->StopPhaseTimer();
    
    // Test końcowy - obiekt nie powinien crashować
    TestNotNull(TEXT("UI powinno pozostać prawidłowe po operacjach na timerze"), UI);

    return true;
}

// Test 4: Zarządzanie PlayerID
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUIPlayerIDTest, 
    "Game.UI.PlayerID", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FGameUIPlayerIDTest::RunTest(const FString& Parameters)
{
    // Arrange
    UGameUI* UI = NewObject<UGameUI>();
    if (!UI)
    {
        AddError(TEXT("Failed to create GameUI"));
        return false;
    }
    
    UI->UpdatePlayerID(0);
    UI->UpdatePlayerID(1);
    UI->UpdatePlayerID(5);
    UI->UpdatePlayerID(-1);
    UI->UpdatePlayerID(100);
    
    // Test wielokrotnej aktualizacji
    for (int32 i = 0; i < 10; i++)
    {
        UI->UpdatePlayerID(i);
    }
    
    TestNotNull(TEXT("UI powinno pozostać prawidłowe po aktualizacjach PlayerID"), UI);

    return true;
}

// Test 5: Zakup jednostki i eventy
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUIPurchaseRequestTest, 
    "Game.UI.PurchaseRequest", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FGameUIPurchaseRequestTest::RunTest(const FString& Parameters)
{
    // Arrange
    UGameUI* UI = NewObject<UGameUI>();
    if (!UI)
    {
        AddError(TEXT("Failed to create GameUI"));
        return false;
    }

    // Konfiguracja jednostek
    TArray<FUnitShopData> TestUnits;
    FUnitShopData Unit1;
    Unit1.UnitName = TEXT("TestUnit");
    Unit1.UnitCost = 50;
    Unit1.UnitType = 0;
    TestUnits.Add(Unit1);
    UI->SetupUnitShop(TestUnits);

    // Test że RequestUnitPurchase nie crashuje
    UI->RequestUnitPurchase(0);
    UI->RequestUnitPurchase(1);
    UI->RequestUnitPurchase(2);
    UI->RequestUnitPurchase(3);
    
    // Test z nieprawidłowymi wartościami
    UI->RequestUnitPurchase(-1);
    UI->RequestUnitPurchase(100);
    
    TestNotNull(TEXT("UI powinno pozostać prawidłowe po requestach zakupu"), UI);

    return true;
}