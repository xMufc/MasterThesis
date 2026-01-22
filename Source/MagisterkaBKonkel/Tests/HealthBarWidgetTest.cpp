#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "HealthBarWidget.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

// Klasa testowa dziedzicząca z HealthBarWidget aby uzyskać dostęp do protected metod
class UHealthBarWidgetTest : public UHealthBarWidget
{
public:
    FLinearColor TestGetHealthColor(float HealthPercentage) const
    {
        return GetHealthColor(HealthPercentage);
    }
};

// Test klasy GetHealthColor
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealthBarWidgetColorTest, "HealthBarWidget.GetHealthColor", 
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FHealthBarWidgetColorTest::RunTest(const FString& Parameters)
{
    // Tworzenie instancji testowej klasy dziedziczącej z HealthBarWidget
    UHealthBarWidgetTest* HealthBarWidget = NewObject<UHealthBarWidgetTest>();
    
    if (!HealthBarWidget)
    {
        AddError(TEXT("Nie można utworzyć instancji UHealthBarWidgetTest"));
        return false;
    }

    // Test 1: Sprawdzenie koloru dla pełnego zdrowia (> 0.6f)
    {
        float HealthPercentage = 1.0f; // 100%
        FLinearColor ResultColor = HealthBarWidget->TestGetHealthColor(HealthPercentage);
        FLinearColor ExpectedColor = FLinearColor::Green;
        
        if (!ResultColor.Equals(ExpectedColor, 0.001f))
        {
            AddError(FString::Printf(TEXT("Test 1 FAILED: Dla %.1f%% oczekiwano zielonego koloru, otrzymano R:%.3f G:%.3f B:%.3f A:%.3f"), 
                HealthPercentage * 100, ResultColor.R, ResultColor.G, ResultColor.B, ResultColor.A));
            return false;
        }
        else
        {
            AddInfo(FString::Printf(TEXT("Test 1 PASSED: 100%% zdrowia = zielony kolor")));
        }
    }

    // Test 2: Sprawdzenie koloru dla średniego zdrowia (0.3f < x <= 0.6f)
    {
        float HealthPercentage = 0.5f; // 50%
        FLinearColor ResultColor = HealthBarWidget->TestGetHealthColor(HealthPercentage);
        FLinearColor ExpectedColor = FLinearColor::Yellow;
        
        if (!ResultColor.Equals(ExpectedColor, 0.001f))
        {
            AddError(FString::Printf(TEXT("Test 2 FAILED: Dla %.1f%% oczekiwano żółtego koloru, otrzymano R:%.3f G:%.3f B:%.3f A:%.3f"), 
                HealthPercentage * 100, ResultColor.R, ResultColor.G, ResultColor.B, ResultColor.A));
            return false;
        }
        else
        {
            AddInfo(FString::Printf(TEXT("Test 2 PASSED: 50%% zdrowia = żółty kolor")));
        }
    }

    // Test 3: Sprawdzenie koloru dla niskiego zdrowia (<= 0.3f)
    {
        float HealthPercentage = 0.2f; // 20%
        FLinearColor ResultColor = HealthBarWidget->TestGetHealthColor(HealthPercentage);
        FLinearColor ExpectedColor = FLinearColor::Red;
        
        if (!ResultColor.Equals(ExpectedColor, 0.001f))
        {
            AddError(FString::Printf(TEXT("Test 3 FAILED: Dla %.1f%% oczekiwano czerwonego koloru, otrzymano R:%.3f G:%.3f B:%.3f A:%.3f"), 
                HealthPercentage * 100, ResultColor.R, ResultColor.G, ResultColor.B, ResultColor.A));
            return false;
        }
        else
        {
            AddInfo(FString::Printf(TEXT("Test 3 PASSED: 20%% zdrowia = czerwony kolor")));
        }
    }

    // Test 4: Sprawdzenie wartości granicznych - dokładnie 0.6f
    {
        float HealthPercentage = 0.6f; // 60%
        FLinearColor ResultColor = HealthBarWidget->TestGetHealthColor(HealthPercentage);
        FLinearColor ExpectedColor = FLinearColor::Yellow; // Powinien być żółty (warunek > 0.6f)
        
        if (!ResultColor.Equals(ExpectedColor, 0.001f))
        {
            AddError(FString::Printf(TEXT("Test 4 FAILED: Dla %.1f%% oczekiwano żółtego koloru (wartość graniczna), otrzymano R:%.3f G:%.3f B:%.3f A:%.3f"), 
                HealthPercentage * 100, ResultColor.R, ResultColor.G, ResultColor.B, ResultColor.A));
            return false;
        }
        else
        {
            AddInfo(FString::Printf(TEXT("Test 4 PASSED: 60%% zdrowia = żółty kolor")));
        }
    }

    // Test 5: Sprawdzenie wartości granicznych - dokładnie 0.3f
    {
        float HealthPercentage = 0.3f; // 30%
        FLinearColor ResultColor = HealthBarWidget->TestGetHealthColor(HealthPercentage);
        FLinearColor ExpectedColor = FLinearColor::Red; // Powinien być czerwony (warunek > 0.3f)
        
        if (!ResultColor.Equals(ExpectedColor, 0.001f))
        {
            AddError(FString::Printf(TEXT("Test 5 FAILED: Dla %.1f%% oczekiwano czerwonego koloru (wartość graniczna), otrzymano R:%.3f G:%.3f B:%.3f A:%.3f"), 
                HealthPercentage * 100, ResultColor.R, ResultColor.G, ResultColor.B, ResultColor.A));
            return false;
        }
        else
        {
            AddInfo(FString::Printf(TEXT("Test 5 PASSED: 30%% zdrowia = czerwony kolor")));
        }
    }

    // Test 6: Sprawdzenie wartości skrajnych - 0%
    {
        float HealthPercentage = 0.0f; // 0%
        FLinearColor ResultColor = HealthBarWidget->TestGetHealthColor(HealthPercentage);
        FLinearColor ExpectedColor = FLinearColor::Red;
        
        if (!ResultColor.Equals(ExpectedColor, 0.001f))
        {
            AddError(FString::Printf(TEXT("Test 6 FAILED: Dla %.1f%% oczekiwano czerwonego koloru, otrzymano R:%.3f G:%.3f B:%.3f A:%.3f"), 
                HealthPercentage * 100, ResultColor.R, ResultColor.G, ResultColor.B, ResultColor.A));
            return false;
        }
        else
        {
            AddInfo(FString::Printf(TEXT("Test 6 PASSED: 0%% zdrowia = czerwony kolor")));
        }
    }

    AddInfo(TEXT("Wszystkie testy GetHealthColor przeszły pomyślnie!"));
    return true;
}