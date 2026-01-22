// HealthBarWidget.cpp - Implementacja widgetu paska zdrowia
#include "HealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"



/// <summary>
/// Konstruktor widgetu paska zdrowia
/// Inicjalizuje domyœlne wartoœci dla wyœwietlania tekstu i kolorów stanu zdrowia
/// </summary>
UHealthBarWidget::UHealthBarWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Domyœlnie w³¹czone wyœwietlanie wartoœci liczbowych zdrowia
    bShowHealthText = true;

    // Definicja palety kolorów dla ró¿nych stanów zdrowia
    HealthyColor = FLinearColor::Green;      // Zdrowy stan (>60%)
    DamagedColor = FLinearColor::Yellow;     // Stan uszkodzony (30-60%)
    CriticalColor = FLinearColor::Red;       // Stan krytyczny (<30%)
}



/// <summary>
/// Metoda wywo³ywana podczas konstruowania widgetu w czasie rzeczywistym
/// Odpowiedzialna za pierwsz¹ inicjalizacjê komponentów UI
/// </summary>
void UHealthBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Inicjalizacja paska postêpu zdrowia
    if (HealthProgressBar)
    {
        // Ustawienie pocz¹tkowej wartoœci na 100%
        HealthProgressBar->SetPercent(1.0f);

        // Ustawienie koloru wype³nienia na kolor zdrowego stanu
        HealthProgressBar->SetFillColorAndOpacity(HealthyColor);
    }

    // Inicjalizacja tekstu wyœwietlaj¹cego wartoœci liczbowe zdrowia
    if (HealthText)
    {
        // Ustawienie widocznoœci tekstu na podstawie flagi bShowHealthText
        HealthText->SetVisibility(bShowHealthText ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

/// <summary>
/// G³ówna metoda aktualizuj¹ca wyœwietlanie paska zdrowia
/// </summary>
/// <param name="HealthPercentage">procent zdrowia w zakresie 0.0-1.0</param>
/// <param name="CurrentHealth">aktualna wartoœæ punktów zdrowia</param>
/// <param name="MaxHealth">maksymalna wartoœæ punktów zdrowia</param>
void UHealthBarWidget::UpdateHealthBar(float HealthPercentage, int32 CurrentHealth, int32 MaxHealth)
{
    // Logowanie informacji debugowych o aktualizacji paska zdrowia
    UE_LOG(LogTemp, Warning, TEXT("UpdateHealthBar called: Percentage=%.2f, Current=%d, Max=%d"),
        HealthPercentage, CurrentHealth, MaxHealth);

    // Aktualizacja komponentu paska postêpu
    if (HealthProgressBar)
    {
        // Logowanie próby ustawienia wartoœci procentowej
        UE_LOG(LogTemp, Warning, TEXT("Setting progress bar percent to: %.2f"), HealthPercentage);

        // Ustawienie wartoœci wype³nienia paska (0.0-1.0)
        HealthProgressBar->SetPercent(HealthPercentage);

        // Dynamiczna zmiana koloru paska na podstawie poziomu zdrowia
        HealthProgressBar->SetFillColorAndOpacity(GetHealthColor(HealthPercentage));

        // Weryfikacja poprawnoœci ustawionej wartoœci (diagnostyka)
        float CurrentPercent = HealthProgressBar->GetPercent();
        UE_LOG(LogTemp, Warning, TEXT("Progress bar percent after setting: %.2f"), CurrentPercent);
    }
    else
    {
        // Logowanie b³êdu w przypadku braku referencji do paska postêpu
        UE_LOG(LogTemp, Error, TEXT("HealthProgressBar is NULL!"));
    }

    // Aktualizacja tekstowej reprezentacji zdrowia
    if (HealthText && bShowHealthText)
    {
        // Formatowanie tekstu w formacie "aktualne / maksymalne"
        FString HealthString = FString::Printf(TEXT("%d / %d"), CurrentHealth, MaxHealth);
        HealthText->SetText(FText::FromString(HealthString));
    }
}

/// <summary>
/// Metoda umo¿liwiaj¹ca rêczne ustawienie koloru paska zdrowia
/// </summary>
/// <param name="NewColor">nowy kolor wype³nienia paska</param>
void UHealthBarWidget::SetHealthBarColor(FLinearColor NewColor)
{
    if (HealthProgressBar)
    {
        HealthProgressBar->SetFillColorAndOpacity(NewColor);
    }
}

// Metoda prze³¹czaj¹ca widocznoœæ tekstowej reprezentacji zdrowia
// Parametry:
//   bShow - flaga okreœlaj¹ca czy tekst ma byæ widoczny
void UHealthBarWidget::SetShowHealthText(bool bShow)
{
    bShowHealthText = bShow;

    // Aktualizacja widocznoœci komponentu tekstowego
    if (HealthText)
    {
        HealthText->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

/// <summary>
/// Metoda pomocnicza okreœlaj¹ca odpowiedni kolor na podstawie poziomu zdrowia
/// </summary>
/// <param name="HealthPercentage">procent zdrowia w zakresie 0.0-1.0</param>
/// <returns>kolor odpowiadaj¹cy stanowi zdrowia</returns>
FLinearColor UHealthBarWidget::GetHealthColor(float HealthPercentage) const
{
    // Zdrowy stan - powy¿ej 60% zdrowia
    if (HealthPercentage > 0.6f)
    {
        return HealthyColor;
    }
    // Stan uszkodzony - miêdzy 30% a 60% zdrowia
    else if (HealthPercentage > 0.3f)
    {
        return DamagedColor;
    }
    // Stan krytyczny - poni¿ej 30% zdrowia
    else
    {
        return CriticalColor;
    }
}