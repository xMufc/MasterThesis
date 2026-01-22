// GameUI.cpp - Implementacja interfejsu u¿ytkownika gry
#include "GameUI.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

/// <summary>
/// Konstruktor - inicjalizuje domyœlne wartoœci dla UI
/// </summary>
UGameUI::UGameUI(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    CurrentGold = 1000;              // Pocz¹tkowa iloœæ z³ota dla gracza
    PhaseTimeRemaining = 90.0f;      // Pozosta³y czas fazy w sekundach
    MaxPhaseTime = 90.0f;            // Maksymalny czas trwania fazy
    bTimerActive = false;            // Flaga okreœlaj¹ca czy timer jest aktywny
    bShopEnabled = false;            // Flaga okreœlaj¹ca czy sklep jest dostêpny
    CurrentPlayerID = -1;            // ID aktualnego gracza
}

/// <summary>
/// Inicjalizacja UI po jego skonstruowaniu w Blueprint
/// </summary>
void UGameUI::NativeConstruct()
{
    Super::NativeConstruct();

    // Przypisanie funkcji obs³ugi klikniêæ do przycisków jednostek
    if (Unit1Button)
        Unit1Button->OnClicked.AddDynamic(this, &UGameUI::OnUnit1ButtonClicked);
    if (Unit2Button)
        Unit2Button->OnClicked.AddDynamic(this, &UGameUI::OnUnit2ButtonClicked);
    if (Unit3Button)
        Unit3Button->OnClicked.AddDynamic(this, &UGameUI::OnUnit3ButtonClicked);
    if (Unit4Button)
        Unit4Button->OnClicked.AddDynamic(this, &UGameUI::OnUnit4ButtonClicked);

    // Utworzenie domyœlnej listy jednostek dostêpnych w sklepie
    TArray<FUnitShopData> DefaultUnits;

    // Definicja jednostki typu Tank
    FUnitShopData Infantry;
    Infantry.UnitName = TEXT("Tank");
    Infantry.UnitCost = 50;
    Infantry.UnitType = 0;
    DefaultUnits.Add(Infantry);

    // Definicja jednostki typu Ninja
    FUnitShopData Archer;
    Archer.UnitName = TEXT("Ninja");
    Archer.UnitCost = 75;
    Archer.UnitType = 1;
    DefaultUnits.Add(Archer);

    // Definicja jednostki typu Sword
    FUnitShopData Cavalry;
    Cavalry.UnitName = TEXT("Sword");
    Cavalry.UnitCost = 100;
    Cavalry.UnitType = 2;
    DefaultUnits.Add(Cavalry);

    // Definicja jednostki typu Armor
    FUnitShopData Mage;
    Mage.UnitName = TEXT("Armor");
    Mage.UnitCost = 125;
    Mage.UnitType = 3;
    DefaultUnits.Add(Mage);

    // Konfiguracja sklepu z jednostkami
    SetupUnitShop(DefaultUnits);

    // Inicjalizacja elementów UI z domyœlnymi wartoœciami
    UpdateGold(CurrentGold);
    UpdatePhaseDisplay(TEXT("Oczekiwanie na graczy"));

    // Ukrycie wyniku meczu i wyœwietlenie komunikatu oczekiwania na graczy
    HideMatchResult();
    ShowWaitingForPlayers(0, 2);

    // Wy³¹czenie sklepu do momentu rozpoczêcia fazy zakupów
    SetShopEnabled(false);

    // Inicjalizacja informacji o graczach z pustymi danymi
    if (Player1NameText)
        Player1NameText->SetText(FText::FromString(TEXT("Gracz 1: Oczekiwanie...")));
    if (Player1LivesText)
        Player1LivesText->SetText(FText::FromString(TEXT("Zycia: 3")));
    if (Player2NameText)
        Player2NameText->SetText(FText::FromString(TEXT("Gracz 2: Oczekiwanie...")));
    if (Player2LivesText)
        Player2LivesText->SetText(FText::FromString(TEXT("Zycia: 3")));

    UE_LOG(LogTemp, Warning, TEXT("GameUI: NativeConstruct zakoñczone"));
}

/// <summary>
/// Funkcja wywo³ywana co klatkê - obs³uguje timer fazy
/// </summary>
/// <param name="MyGeometry">Aktualizowany obiekt</param>
/// <param name="InDeltaTime">Aktualny czas</param>
void UGameUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Obs³uga licznika czasu fazy z debugowaniem
    if (bTimerActive && PhaseTimeRemaining > 0.0f)
    {
        // Timer do okresowego logowania stanu
        static float DebugTimer = 0.0f;
        DebugTimer += InDeltaTime;

        // Zmniejszenie pozosta³ego czasu o czas od ostatniej klatki
        PhaseTimeRemaining -= InDeltaTime;
        if (PhaseTimeRemaining <= 0.0f)
        {
            PhaseTimeRemaining = 0.0f;
            bTimerActive = false;
            UE_LOG(LogTemp, Warning, TEXT("GameUI: Timer wygas³ - wywo³anie OnTimerExpired"));
            OnTimerExpired();
        }

        // Aktualizacja wyœwietlania timera
        UpdateTimer(PhaseTimeRemaining, MaxPhaseTime);

        // Logowanie stanu timera co 5 sekund dla celów diagnostycznych
        if (DebugTimer >= 5.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("GameUI: Timer aktywny - Pozosta³o: %.1f/%.1f sekund"),
                PhaseTimeRemaining, MaxPhaseTime);
            DebugTimer = 0.0f;
        }
    }
    else if (bTimerActive && PhaseTimeRemaining <= 0.0f)
    {
        // Zabezpieczenie - zatrzymanie timera jeœli czas ju¿ min¹³
        UE_LOG(LogTemp, Warning, TEXT("GameUI: Timer by³ aktywny ale czas <= 0, zatrzymywanie"));
        bTimerActive = false;
    }
}

/// <summary>
/// Rozpoczêcie odliczania czasu dla aktualnej fazy gry
/// </summary>
/// <param name="TimeInSeconds">Pozostaly czas</param>
void UGameUI::StartPhaseTimer(float TimeInSeconds)
{
    PhaseTimeRemaining = TimeInSeconds;
    MaxPhaseTime = TimeInSeconds;
    bTimerActive = true;

    UE_LOG(LogTemp, Warning, TEXT("GameUI: Timer fazy rozpoczêty na %.1f sekund - bTimerActive=%s"),
        TimeInSeconds, bTimerActive ? TEXT("TRUE") : TEXT("FALSE"));

    // Natychmiastowa aktualizacja wyœwietlania timera
    UpdateTimer(PhaseTimeRemaining, MaxPhaseTime);
}

/// <summary>
/// Zatrzymanie timera fazy i zresetowanie wyœwietlania
/// </summary>
void UGameUI::StopPhaseTimer()
{
    bTimerActive = false;
    PhaseTimeRemaining = 0.0f;

    // Zresetowanie wyœwietlania timera
    if (TimerText)
        TimerText->SetText(FText::FromString(TEXT("00:00")));
    if (TimerProgressBar)
        TimerProgressBar->SetPercent(1.0f);
}

/// <summary>
/// Aktualizacja wyœwietlanej iloœci z³ota gracza
/// </summary>
/// <param name="NewGoldAmount">Nowa wartoœæ z³ota</param>
void UGameUI::UpdateGold(int32 NewGoldAmount)
{
    CurrentGold = NewGoldAmount;

    // Aktualizacja tekstu wyœwietlaj¹cego iloœæ z³ota
    if (GoldAmountText)
    {
        GoldAmountText->SetText(FText::FromString(FString::Printf(TEXT("Zloto: %d"), CurrentGold)));
    }

    // Aktualizacja dostêpnoœci przycisków jednostek na podstawie z³ota
    for (int32 i = 0; i < UnitShopData.Num(); i++)
    {
        SetUnitAvailability(i, CanAffordUnit(i) && bShopEnabled);
    }
}

/// <summary>
/// Aktualizacja wyœwietlania pozosta³ego czasu i paska postêpu
/// </summary>
/// <param name="TimeRemaining">Pozosta³y czas</param>
/// <param name="MaxTime">Calkowity czas</param>
void UGameUI::UpdateTimer(float TimeRemaining, float MaxTime)
{
    // Aktualizacja tekstowego wyœwietlania czasu w formacie MM:SS
    if (TimerText)
    {
        int32 Minutes = FMath::FloorToInt(TimeRemaining / 60.0f);
        int32 Seconds = FMath::FloorToInt(TimeRemaining) % 60;

        FString TimeString = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
        TimerText->SetText(FText::FromString(TimeString));
    }

    // Aktualizacja paska postêpu
    if (TimerProgressBar)
    {
        float Progress = MaxTime > 0 ? (MaxTime - TimeRemaining) / MaxTime : 1.0f;
        TimerProgressBar->SetPercent(Progress);
    }
}

/// <summary>
/// Aktualizacja wyœwietlania nazwy aktualnej fazy gry
/// </summary>
/// <param name="PhaseName">Nazwa aktualnej fazy</param>
void UGameUI::UpdatePhaseDisplay(const FString& PhaseName)
{
    if (PhaseNameText)
    {
        PhaseNameText->SetText(FText::FromString(PhaseName));

        // Przypisanie koloru w zale¿noœci od typu fazy
        FLinearColor PhaseColor = FLinearColor::White;
        if (PhaseName.Contains(TEXT("kupowania")))
        {
            PhaseColor = FLinearColor::Green;      // Zielony dla fazy zakupów
        }
        else if (PhaseName.Contains(TEXT("walki")))
        {
            PhaseColor = FLinearColor::Red;        // Czerwony dla fazy walki
        }
        else if (PhaseName.Contains(TEXT("Wyniki")))
        {
            PhaseColor = FLinearColor::Yellow;     // ¯ó³ty dla wyœwietlania wyników
        }
        else if (PhaseName.Contains(TEXT("Oczekiwanie")))
        {
            PhaseColor = FLinearColor::Gray;       // Szary dla oczekiwania
        }

        PhaseNameText->SetColorAndOpacity(FSlateColor(PhaseColor));
    }

    UE_LOG(LogTemp, Warning, TEXT("GameUI: Wyœwietlacz fazy zaktualizowany do: %s"), *PhaseName);
}

/// <summary>
/// Aktualizacja informacji o graczu
/// </summary>
/// <param name="PlayerID">ID gracz</param>
/// <param name="PlayerName">Nazwa gracza</param>
/// <param name="Lives">Liczba zyc</param>
void UGameUI::UpdatePlayerInfo(int32 PlayerID, const FString& PlayerName, int32 Lives)
{
    // Utworzenie nazwy wyœwietlanej - jeœli pusta, u¿yj domyœlnej
    FString DisplayName = PlayerName.IsEmpty() ? FString::Printf(TEXT("Gracz %d"), PlayerID + 1) : PlayerName;

    UE_LOG(LogTemp, Warning, TEXT("GameUI: Aktualizacja informacji gracza - ID: %d, Nazwa: %s, Zycia: %d"),
        PlayerID, *DisplayName, Lives);

    // Aktualizacja dla gracza 1
    if (PlayerID == 0)
    {
        if (Player1NameText)
        {
            Player1NameText->SetText(FText::FromString(DisplayName));
            UE_LOG(LogTemp, Warning, TEXT("GameUI: Zaktualizowano nazwê Gracza1 na: %s"), *DisplayName);
        }
        if (Player1LivesText)
        {
            Player1LivesText->SetText(FText::FromString(FString::Printf(TEXT("Zycia: %d"), Lives)));

            // Zmiana koloru w zale¿noœci od iloœci ¿yæ
            FLinearColor LifeColor = FLinearColor::Green;
            if (Lives <= 1)
                LifeColor = FLinearColor::Red;        // Czerwony przy 1 ¿yciu
            else if (Lives == 2)
                LifeColor = FLinearColor::Yellow;     // ¯ó³ty przy 2 ¿yciach

            Player1LivesText->SetColorAndOpacity(FSlateColor(LifeColor));
            UE_LOG(LogTemp, Warning, TEXT("GameUI: Zaktualizowano ¿ycia Gracza1 na: %d"), Lives);
        }
    }
    // Aktualizacja dla gracza 2
    else if (PlayerID == 1)
    {
        if (Player2NameText)
        {
            Player2NameText->SetText(FText::FromString(DisplayName));
            UE_LOG(LogTemp, Warning, TEXT("GameUI: Zaktualizowano nazwê Gracza2 na: %s"), *DisplayName);
        }
        if (Player2LivesText)
        {
            Player2LivesText->SetText(FText::FromString(FString::Printf(TEXT("Zycia: %d"), Lives)));

            // Zmiana koloru w zale¿noœci od iloœci ¿yæ
            FLinearColor LifeColor = FLinearColor::Green;
            if (Lives <= 1)
                LifeColor = FLinearColor::Red;
            else if (Lives == 2)
                LifeColor = FLinearColor::Yellow;

            Player2LivesText->SetColorAndOpacity(FSlateColor(LifeColor));
            UE_LOG(LogTemp, Warning, TEXT("GameUI: Zaktualizowano ¿ycia Gracza2 na: %d"), Lives);
        }
    }
}

/// <summary>
/// W³¹czenie/wy³¹czenie dostêpnoœci sklepu
/// </summary>
/// <param name="bEnabled">Dostêpnoœæ sklepu</param>
void UGameUI::SetShopEnabled(bool bEnabled)
{
    bShopEnabled = bEnabled;

    // Aktualizacja dostêpnoœci wszystkich przycisków jednostek
    for (int32 i = 0; i < 4; i++)
    {
        SetUnitAvailability(i, CanAffordUnit(i) && bEnabled);
    }

    UE_LOG(LogTemp, Warning, TEXT("GameUI: Sklep ustawiony na: %s"), bEnabled ? TEXT("w³¹czony") : TEXT("wy³¹czony"));
}

/// <summary>
/// Sprawdzenie czy sklep jest w³¹czony
/// </summary>
/// <returns>true - gdy jest wlaczony, false - wpp</returns>
bool UGameUI::GetShopEnabled() const
{
    return bShopEnabled;
}

/// <summary>
/// Wyœwietlenie komunikatu oczekiwania na graczy
/// </summary>
/// <param name="CurrentPlayers">Aktualna ilosc graczy</param>
/// <param name="RequiredPlayers">Wymagana ilosc graczy</param>
void UGameUI::ShowWaitingForPlayers(int32 CurrentPlayers, int32 RequiredPlayers)
{
    if (WaitingPlayersText)
    {
        FString WaitingText = FString::Printf(TEXT("Oczekiwanie na graczy: %d/%d"), CurrentPlayers, RequiredPlayers);
        WaitingPlayersText->SetText(FText::FromString(WaitingText));
    }

    if (WaitingPlayersBorder)
    {
        WaitingPlayersBorder->SetVisibility(ESlateVisibility::Visible);
    }

    UE_LOG(LogTemp, Warning, TEXT("GameUI: Wyœwietlanie oczekiwania na graczy: %d/%d"), CurrentPlayers, RequiredPlayers);
}

/// <summary>
/// Ukrycie komunikatu oczekiwania na graczy
/// </summary>
void UGameUI::HideWaitingForPlayers()
{
    if (WaitingPlayersBorder)
    {
        WaitingPlayersBorder->SetVisibility(ESlateVisibility::Hidden);
    }

    UE_LOG(LogTemp, Warning, TEXT("GameUI: Ukrywanie oczekiwania na graczy"));
}

/// <summary>
/// Wyœwietlenie wyniku meczu 
/// </summary>
/// <param name="ResultText">Wynik meczu</param>
/// <param name="DisplayDuration">Dlugosc wyswietlania</param>
void UGameUI::ShowMatchResult(const FString& ResultText, float DisplayDuration)
{
    if (MatchResultText)
    {
        MatchResultText->SetText(FText::FromString(ResultText));

        // Wybór koloru w zale¿noœci od treœci wyniku
        FLinearColor ResultColor = FLinearColor::White;
        if (ResultText.Contains(TEXT("wygr")))
        {
            ResultColor = FLinearColor::Green;      // Zielony dla zwyciêstwa
        }
        else if (ResultText.Contains(TEXT("Remis")))
        {
            ResultColor = FLinearColor::Yellow;     // ¯ó³ty dla remisu
        }

        MatchResultText->SetColorAndOpacity(FSlateColor(ResultColor));
    }

    if (MatchResultBorder)
    {
        MatchResultBorder->SetVisibility(ESlateVisibility::Visible);
    }

    // Ustawienie timera do automatycznego ukrycia wyniku po okreœlonym czasie
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(MatchResultTimerHandle,
            this, &UGameUI::OnMatchResultTimerExpired, DisplayDuration, false);
    }

    UE_LOG(LogTemp, Warning, TEXT("GameUI: Wyœwietlanie wyniku meczu: %s"), *ResultText);
}

/// <summary>
/// Ukrycie wyœwietlanego wyniku meczu
/// </summary>
void UGameUI::HideMatchResult()
{
    if (MatchResultBorder)
    {
        MatchResultBorder->SetVisibility(ESlateVisibility::Hidden);
    }

    // Wyczyszczenie timera jeœli jest aktywny
    if (GetWorld() && MatchResultTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(MatchResultTimerHandle);
    }
}

/// <summary>
/// Funkcja wywo³ywana po wygaœniêciu timera wyniku meczu
/// </summary>
void UGameUI::OnMatchResultTimerExpired()
{
    HideMatchResult();
}


/// <summary>
/// Dodanie wiadomoœci do okna czatu
/// </summary>
/// <param name="Message">Wiadomosc</param>
/// <param name="PlayerName">Nazwa gracza</param>
void UGameUI::AddChatMessage(const FString& Message, const FString& PlayerName)
{
    if (!ChatBox)
        return;

    // Utworzenie nowego elementu tekstowego dla wiadomoœci
    UTextBlock* MessageText = NewObject<UTextBlock>(this);
    if (MessageText)
    {
        FString FullMessage;
        // Formatowanie wiadomoœci z nazw¹ gracza jeœli dostêpna
        if (!PlayerName.IsEmpty())
        {
            FullMessage = FString::Printf(TEXT("[%s]: %s"), *PlayerName, *Message);
        }
        else
        {
            FullMessage = Message;
        }

        MessageText->SetText(FText::FromString(FullMessage));
        MessageText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        ChatBox->AddChild(MessageText);

        // Ograniczenie liczby wiadomoœci do 10
        if (ChatBox->GetChildrenCount() > 10)
        {
            ChatBox->RemoveChildAt(0);
        }
    }
}

/// <summary>
/// Konfiguracja sklepu z jednostkami
/// </summary>
/// <param name="Units">Tablica z dostepnymi jednostkami</param>
void UGameUI::SetupUnitShop(const TArray<FUnitShopData>& Units)
{
    UnitShopData = Units;

    // Uzupe³nienie brakuj¹cych slotów pustymi jednostkami
    while (UnitShopData.Num() < 4)
    {
        FUnitShopData EmptyUnit;
        EmptyUnit.UnitName = TEXT("Empty");
        EmptyUnit.UnitCost = 0;
        UnitShopData.Add(EmptyUnit);
    }

    // Tablice wskaŸników do elementów UI poszczególnych jednostek
    TArray<UTextBlock*> NameTexts = { Unit1Name, Unit2Name, Unit3Name, Unit4Name };
    TArray<UTextBlock*> CostTexts = { Unit1Cost, Unit2Cost, Unit3Cost, Unit4Cost };
    TArray<UImage*> IconImages = { Unit1Icon, Unit2Icon, Unit3Icon, Unit4Icon };

    // Aktualizacja UI dla ka¿dej jednostki
    for (int32 i = 0; i < 4 && i < UnitShopData.Num(); i++)
    {
        if (NameTexts[i])
            NameTexts[i]->SetText(FText::FromString(UnitShopData[i].UnitName));

        if (CostTexts[i])
            CostTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("%d Zloto"), UnitShopData[i].UnitCost)));

        if (IconImages[i] && UnitShopData[i].UnitIcon)
            IconImages[i]->SetBrushFromTexture(UnitShopData[i].UnitIcon);
    }

    // Ustawienie pocz¹tkowej dostêpnoœci przycisków
    for (int32 i = 0; i < 4; i++)
    {
        SetUnitAvailability(i, CanAffordUnit(i) && bShopEnabled);
    }
}

/// <summary>
/// Ustawienie dostêpnoœci konkretnej jednostki w sklepie
/// </summary>
/// <param name="UnitIndex">ID jednostki</param>
/// <param name="bAvailable">Dostêpnoœæ</param>
void UGameUI::SetUnitAvailability(int32 UnitIndex, bool bAvailable)
{
    TArray<UButton*> UnitButtons = { Unit1Button, Unit2Button, Unit3Button, Unit4Button };
    TArray<UTextBlock*> CostTexts = { Unit1Cost, Unit2Cost, Unit3Cost, Unit4Cost };

    if (UnitIndex < 0 || UnitIndex >= 4)
        return;

    // W³¹czenie/wy³¹czenie przycisku i ustawienie przezroczystoœci
    if (UnitButtons[UnitIndex])
    {
        UnitButtons[UnitIndex]->SetIsEnabled(bAvailable && bShopEnabled);
        float Alpha = (bAvailable && bShopEnabled) ? 1.0f : 0.5f;
        UnitButtons[UnitIndex]->SetRenderOpacity(Alpha);
    }

    // Zmiana koloru tekstu kosztu (zielony = dostêpny, czerwony = niedostêpny)
    if (CostTexts[UnitIndex])
    {
        FLinearColor TextColor = (bAvailable && bShopEnabled) ? FLinearColor::Green : FLinearColor::Red;
        CostTexts[UnitIndex]->SetColorAndOpacity(FSlateColor(TextColor));
    }
}

/// <summary>
/// Sprawdzenie czy gracz ma wystarczaj¹co z³ota na zakup jednostki
/// </summary>
/// <param name="UnitIndex">ID jednostki</param>
/// <returns>true - gdy mo¿e kupiæ, false - wpp</returns>
bool UGameUI::CanAffordUnit(int32 UnitIndex) const
{
    if (UnitIndex < 0 || UnitIndex >= UnitShopData.Num())
        return false;

    return CurrentGold >= UnitShopData[UnitIndex].UnitCost;
}

// Funkcje obs³ugi klikniêæ przycisków poszczególnych jednostek
void UGameUI::OnUnit1ButtonClicked() { HandleUnitPurchase(0); }
void UGameUI::OnUnit2ButtonClicked() { HandleUnitPurchase(1); }
void UGameUI::OnUnit3ButtonClicked() { HandleUnitPurchase(2); }
void UGameUI::OnUnit4ButtonClicked() { HandleUnitPurchase(3); }

// Obs³uga zakupu jednostki - walidacja i rozg³oszenie eventu
void UGameUI::HandleUnitPurchase(int32 UnitIndex)
{
    // Sprawdzenie czy sklep jest w³¹czony i indeks jest poprawny
    if (!bShopEnabled || UnitIndex < 0 || UnitIndex >= UnitShopData.Num())
        return;

    // Sprawdzenie czy gracz ma wystarczaj¹co z³ota
    if (!CanAffordUnit(UnitIndex))
        return;

    int32 UnitType = UnitShopData[UnitIndex].UnitType;

    // Rozg³oszenie requestu zakupu do zewnêtrznych systemów
    if (OnUnitPurchaseRequested.IsBound())
    {
        OnUnitPurchaseRequested.Broadcast(UnitType);
        UE_LOG(LogTemp, Warning, TEXT("GameUI: Za¿¹dano zakupu jednostki: Typ %d"), UnitType);
    }

    // Pobranie ID gracza i rozg³oszenie eventu zakupu
    int32 PlayerID = GetOwningPlayer() ? GetPlayerIDFromController(GetOwningPlayer()) : CurrentPlayerID;
    if (OnUnitPurchased.IsBound())
    {
        OnUnitPurchased.Broadcast(UnitType, PlayerID);
    }
}


/// <summary>
/// Funkcja wywo³ywana po wygaœniêciu timera fazy
/// </summary>
void UGameUI::OnTimerExpired()
{
    bTimerActive = false;
    SetShopEnabled(false);

    // Zresetowanie wyœwietlania timera
    if (TimerText)
        TimerText->SetText(FText::FromString(TEXT("00:00")));
    if (TimerProgressBar)
        TimerProgressBar->SetPercent(1.0f);

    // Rozg³oszenie eventu zakoñczenia fazy
    OnPhaseCompleted.Broadcast();
    UE_LOG(LogTemp, Warning, TEXT("GameUI: Timer fazy wygas³"));
}

/// <summary>
/// Pobranie ID gracza z kontrolera
/// </summary>
/// <param name="Controller">WskaŸnik do kontrolera</param>
/// <returns>ID gracza</returns>
int32 UGameUI::GetPlayerIDFromController(APlayerController* Controller) const
{
    if (Controller)
    {
        return Controller->GetLocalPlayer() ? Controller->GetLocalPlayer()->GetControllerId() : CurrentPlayerID;
    }
    return CurrentPlayerID;
}

/// <summary>
/// Aktualizacja przechowywanego ID gracza
/// </summary>
/// <param name="NewPlayerID">ID nowego gracza</param>
void UGameUI::UpdatePlayerID(int32 NewPlayerID)
{
    CurrentPlayerID = NewPlayerID;
    UE_LOG(LogTemp, Warning, TEXT("GameUI: Zaktualizowano PlayerID na %d"), CurrentPlayerID);
}

/// <summary>
/// ¯¹danie zakupu jednostki z zewn¹trz
/// </summary>
/// <param name="UnitType"> ID jednostki</param>
void UGameUI::RequestUnitPurchase(int32 UnitType)
{
    if (OnUnitPurchaseRequested.IsBound())
    {
        OnUnitPurchaseRequested.Broadcast(UnitType);
    }
}