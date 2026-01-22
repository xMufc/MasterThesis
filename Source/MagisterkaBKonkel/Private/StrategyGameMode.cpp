// StrategyGameMode.cpp - Glowny tryb gry strategicznej
#include "StrategyGameMode.h"
#include "StrategyPlayerController.h"
#include "UnitManager.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameUI.h"
#include "Net/UnrealNetwork.h"

/// <summary>
/// Konstruktor klasy AStrategyGameMode.
/// </summary>
AStrategyGameMode::AStrategyGameMode()
{
    PrimaryActorTick.bCanEverTick = true;

    // Inicjalizacja stanu gry
    CurrentPhase = EGamePhase::WaitingForPlayers;
    NextPlayerID = 0;

    // Konfiguracja czasow faz (w sekundach)
    PurchasePhaseTime = 30.0f;    // Czas na zakup jednostek
    BattlePhaseTime = 60.0f;       // Maksymalny czas bitwy
    ResultsPhaseTime = 5.0f;       // Czas wyœwietlania wynikow

    // Zasoby startowe graczy
    StartingGold = 1000;           // Pocz¹tkowa iloœæ zlota
    StartingLives = 3;             // Pocz¹tkowa liczba ¿yæ

    // Limity graczy
    RequiredPlayersCount = 2;      // Minimalna liczba graczy do startu
    MaxPlayersPerGame = 2;         // Maksymalna liczba graczy

    // Inicjalizacja referencji
    UnitManagerRef = nullptr;
    PlayerControllerClass = AStrategyPlayerController::StaticClass();

    // Konfiguracja replikacji sieciowej
    bReplicates = true;
    bAlwaysRelevant = true;
    bFirstUpdatePlayerInfo = true;
}

/// <summary>
/// Wywolywane przy starcie gry.
/// </summary>
void AStrategyGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Logika wykonywana tylko na serwerze
    if (!HasAuthority())
        return;

    InitializeUnitManager();
    InitializeWaitingPhase();
    UE_LOG(LogTemp, Warning, TEXT("StrategyGameMode: BeginPlay zakoñczone"));
}

/// <summary>
/// Wywolywane gdy nowy gracz dol¹cza do gry.
/// </summary>
/// <param name="NewPlayer">Kontroler nowo dol¹czonego gracza</param>
void AStrategyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // Sprawdzenie autorytetu serwera i poprawnoœci kontrolera
    if (!HasAuthority() || !NewPlayer)
        return;

    // Sprawdzenie limitu graczy
    if (NextPlayerID >= MaxPlayersPerGame)
    {
        UE_LOG(LogTemp, Warning, TEXT("Osi¹gnieto limit graczy"));
        NewPlayer->ClientTravel("", ETravelType::TRAVEL_Absolute);
        return;
    }

    // Przypisanie ID gracza i utworzenie nazwy
    int32 AssignedPlayerID = NextPlayerID++;
    FString PlayerName = FString::Printf(TEXT("Gracz %d"), AssignedPlayerID + 1);

    // Utworzenie struktury danych gracza
    FPlayerGameData NewPlayerData;
    NewPlayerData.PlayerID = AssignedPlayerID;
    NewPlayerData.Gold = StartingGold;
    NewPlayerData.Lives = StartingLives;
    NewPlayerData.PlayerName = PlayerName;
    NewPlayerData.PlayerController = NewPlayer;
    NewPlayerData.bReadyForNextPhase = false;
    PlayerDataArray.Add(NewPlayerData);

    // Inicjalizacja kontrolera strategicznego gracza
    if (AStrategyPlayerController* StratPC = Cast<AStrategyPlayerController>(NewPlayer))
    {
        StratPC->SetPlayerID(AssignedPlayerID);
        StratPC->ClientInitializePlayer(AssignedPlayerID, StartingGold);
    }

    UE_LOG(LogTemp, Warning, TEXT("SERWER: Gracz %d dol¹czyl. l¹cznie: %d"), AssignedPlayerID, PlayerDataArray.Num());

    // OpoŸniona synchronizacja danych aby upewniæ sie, ¿e klienci s¹ gotowi
    FTimerHandle SyncTimer;
    GetWorldTimerManager().SetTimer(SyncTimer, [this]()
        {
            UE_LOG(LogTemp, Warning, TEXT("SERWER: Rozsylanie danych graczy do wszystkich klientow"));

            // Synchronizacja danych wszystkich graczy do wszystkich klientow
            for (const FPlayerGameData& PlayerData : PlayerDataArray)
            {
                UE_LOG(LogTemp, Warning, TEXT("SERWER: Synchronizacja danych gracza %d (%s)"),
                    PlayerData.PlayerID, *PlayerData.PlayerName);
                MulticastSyncPlayerData(PlayerData.PlayerID, PlayerData.PlayerName, PlayerData.Gold, PlayerData.Lives);
            }

            // Aktualizacja ekranu oczekiwania
            MulticastShowWaitingForPlayers(PlayerDataArray.Num(), RequiredPlayersCount);

        }, 0.5f, false);

    // Rozgloszenie wiadomoœci o dol¹czeniu gracza
    MulticastPlayerJoined(AssignedPlayerID, PlayerName);

    // Sprawdzenie czy mo¿na rozpocz¹æ gre
    if (HasEnoughPlayers() && CurrentPhase == EGamePhase::WaitingForPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("SERWER: Wystarczaj¹ca liczba graczy - start gry za 2 sekundy"));
        GetWorldTimerManager().SetTimer(DelayedStartTimer,
            FTimerDelegate::CreateUObject(this, &AStrategyGameMode::InitializePurchasePhase), 2.0f, false);
    }
}

/// <summary>
/// Wywolywane gdy gracz opuszcza gre lub traci pol¹czenie.
/// </summary>
/// <param name="Exiting">Kontroler gracza opuszczaj¹cego gre</param>
void AStrategyGameMode::Logout(AController* Exiting)
{
    // Sprawdzenie autorytetu serwera
    if (!HasAuthority())
    {
        Super::Logout(Exiting);
        return;
    }

    // Obsluga wylogowania gracza
    if (APlayerController* PC = Cast<APlayerController>(Exiting))
    {
        int32 PlayerID = GetPlayerIDFromController(PC);
        if (PlayerID >= 0)
        {
            HandlePlayerLogout(PlayerID);
            MulticastPlayerLeft(PlayerID);
        }
    }

    Super::Logout(Exiting);

    // Sprawdzenie czy wystarczaj¹ca liczba graczy pozostala w grze
    if (PlayerDataArray.Num() < RequiredPlayersCount)
    {
        InitializeWaitingPhase();
    }
}

/// <summary>
/// Inicjalizuje faze oczekiwania na graczy.
/// </summary>
void AStrategyGameMode::InitializeWaitingPhase()
{
    // Ustawienie fazy oczekiwania na graczy
    CurrentPhase = EGamePhase::WaitingForPlayers;

    // Wyczyszczenie timerow faz
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().ClearTimer(DelayedStartTimer);

    // Powiadomienie klientow o zmianie fazy
    MulticastPhaseChanged(CurrentPhase, 0.0f);
    MulticastShowWaitingForPlayers(PlayerDataArray.Num(), RequiredPlayersCount);
    UE_LOG(LogTemp, Warning, TEXT("Oczekiwanie na graczy (%d/%d)"), PlayerDataArray.Num(), RequiredPlayersCount);
}

/// <summary>
/// Inicjalizuje faze zakupow jednostek.
/// </summary>
void AStrategyGameMode::InitializePurchasePhase()
{
    // Sprawdzenie autorytetu i liczby graczy
    if (!HasAuthority() || !HasEnoughPlayers())
    {
        InitializeWaitingPhase();
        return;
    }

    // Pierwsza aktualizacja informacji o graczach w UI
    if (bFirstUpdatePlayerInfo) {
        UpdateAllPlayersUI();
        bFirstUpdatePlayerInfo = false;
    }

    // Ustawienie fazy zakupow
    CurrentPhase = EGamePhase::Purchase;
    ResetPlayerPhaseData();
    ClearAllUnitsFromBattlefield();
    MulticastPhaseChanged(CurrentPhase, PurchasePhaseTime);

    // Ustawienie timera zakoñczenia fazy zakupow
    GetWorldTimerManager().SetTimer(PhaseTimer,
        FTimerDelegate::CreateUObject(this, &AStrategyGameMode::OnPhaseTimerExpired), PurchasePhaseTime, false);

    UE_LOG(LogTemp, Warning, TEXT("Rozpoczeto faze zakupow"));
}

/// <summary>
/// Inicjalizuje faze walki miedzy jednostkami graczy.
/// </summary>
void AStrategyGameMode::InitializeBattlePhase()
{
    if (!HasAuthority())
        return;

    // Ustawienie fazy walki
    CurrentPhase = EGamePhase::Battle;
    MulticastPhaseChanged(CurrentPhase, BattlePhaseTime);

    // Rozpoczecie fazy walki w UnitManager
    if (UnitManagerRef)
    {
        UnitManagerRef->DeselectUnit();
        UnitManagerRef->StartCombatPhase();
    }

    // Ustawienie timera sprawdzania koñca bitwy
    GetWorldTimerManager().SetTimer(BattleEndCheckTimer,
        FTimerDelegate::CreateUObject(this, &AStrategyGameMode::CheckForBattleEnd), 2.0f, true);

    // Ustawienie timera maksymalnego czasu bitwy
    GetWorldTimerManager().SetTimer(PhaseTimer,
        FTimerDelegate::CreateUObject(this, &AStrategyGameMode::OnPhaseTimerExpired), BattlePhaseTime, false);

    UE_LOG(LogTemp, Warning, TEXT("Rozpoczeto faze walki - system walki aktywowany, sprawdzanie koñca co 2s"));
}

/// <summary>
/// Inicjalizuje faze wynikow bitwy.
/// </summary>
void AStrategyGameMode::InitializeResultsPhase()
{
    if (!HasAuthority())
        return;

    // Ustawienie fazy wynikow
    CurrentPhase = EGamePhase::Results;

    // Zatrzymanie fazy walki przed usunieciem jednostek
    if (UnitManagerRef)
    {
        UnitManagerRef->StopCombatPhase();
    }

    // Zatrzymanie timera sprawdzania koñca bitwy
    GetWorldTimerManager().ClearTimer(BattleEndCheckTimer);

    // Przetworzenie wynikow bitwy
    ProcessBattleResults();

    // Powiadomienie klientow o zmianie fazy
    MulticastPhaseChanged(CurrentPhase, ResultsPhaseTime);

    // Ustawienie timera zakoñczenia fazy wynikow
    GetWorldTimerManager().SetTimer(PhaseTimer,
        FTimerDelegate::CreateUObject(this, &AStrategyGameMode::OnPhaseTimerExpired), ResultsPhaseTime, false);

    UE_LOG(LogTemp, Warning, TEXT("Rozpoczeto faze wynikow - system walki zatrzymany, wszystkie jednostki usuniete"));
}

/// <summary>
/// Inicjalizuje faze koñca gry.
/// </summary>
void AStrategyGameMode::InitializeGameOverPhase()
{
    // Ustawienie fazy koñca gry
    CurrentPhase = EGamePhase::GameOver;
    MulticastPhaseChanged(CurrentPhase, 0.0f);

    // Okreœlenie zwyciezcy (gracz ktory ma jeszcze ¿ycia)
    for (const FPlayerGameData& PlayerData : PlayerDataArray)
    {
        if (PlayerData.Lives > 0)
        {
            EMatchResult WinResult = PlayerData.PlayerID == 0 ? EMatchResult::Player1Wins : EMatchResult::Player2Wins;
            MulticastShowMatchResult(WinResult);
            break;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Koniec gry"));
}

/// <summary>
/// Wywolywana gdy timer fazy wygasa.
/// </summary>
void AStrategyGameMode::OnPhaseTimerExpired()
{
    // Obsluga wygaœniecia timera fazy
    switch (CurrentPhase)
    {
    case EGamePhase::Purchase:
        // Sprawdzenie czy bitwa ju¿ sie zakoñczyla
        CheckForBattleEnd();
        if (CurrentPhase != EGamePhase::Results)
        {
            InitializeBattlePhase();
        }
        break;

    case EGamePhase::Battle:
        InitializeResultsPhase();
        break;

    case EGamePhase::Results:
        StartNextPhase();
        break;
    }
}

/// <summary>
/// Rozpoczyna nastepn¹ faze gry.
/// </summary>
void AStrategyGameMode::StartNextPhase()
{
    // Sprawdzenie czy gra sie zakoñczyla (ktoœ stracil wszystkie ¿ycia)
    bool bGameOver = false;
    for (const FPlayerGameData& PlayerData : PlayerDataArray)
    {
        if (PlayerData.Lives <= 0)
        {
            bGameOver = true;
            break;
        }
    }

    // Aktualizacja informacji o graczach jeœli oczekujemy na graczy
    if (CurrentPhase == EGamePhase::WaitingForPlayers) {
        MulticastUpdatePlayersInfo();
    }

    // Rozpoczecie odpowiedniej fazy
    if (bGameOver)
    {
        InitializeGameOverPhase();
    }
    else
    {
        InitializePurchasePhase();
    }
}

/// <summary>
/// Sprawdza czy bitwa powinna sie zakoñczyæ.
/// </summary>
void AStrategyGameMode::CheckForBattleEnd()
{
    if (!HasAuthority() || !UnitManagerRef)
        return;

    // Policzenie jednostek ka¿dego gracza
    int32 Player1Units = CountPlayerUnits(0);
    int32 Player2Units = CountPlayerUnits(1);

    UE_LOG(LogTemp, Warning, TEXT("=== SPRAWDZANIE BITWY: Jednostki Gracza 1: %d, Jednostki Gracza 2: %d ==="),
        Player1Units, Player2Units);

    // Sprawdzenie czy ktoryœ gracz nie ma jednostek lub obaj nie maj¹ jednostek
    if ((Player1Units == 0 && Player2Units == 0) || Player1Units == 0 || Player2Units == 0)
    {
        if (CurrentPhase == EGamePhase::Battle)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== KONIEC BITWY: Jeden lub obaj gracze nie maj¹ jednostek - zakoñczenie bitwy ==="));
            GetWorldTimerManager().ClearTimer(PhaseTimer);
            InitializeResultsPhase();
        }
    }
}

/// <summary>
/// Przetwarza wyniki bitwy i stosuje konsekwencje.
/// </summary>
void AStrategyGameMode::ProcessBattleResults()
{
    if (!HasAuthority())
        return;

    // Okreœlenie zwyciezcy bitwy
    EMatchResult Result = DetermineBattleWinner();

    // Zastosowanie konsekwencji wyniku
    switch (Result)
    {
    case EMatchResult::Player1Wins:
        RemovePlayerLife(1);  // Gracz 2 traci ¿ycie
        break;
    case EMatchResult::Player2Wins:
        RemovePlayerLife(0);  // Gracz 1 traci ¿ycie
        break;
    case EMatchResult::Draw:
        // Remis - nikt nie traci ¿ycia
        break;
    }

    // Wyczyszczenie pola bitwy i wyœwietlenie wyniku
    ClearAllUnitsFromBattlefield();
    ShowMatchResult(Result);
}

/// <summary>
/// Okreœla zwyciezce bitwy na podstawie liczby ¿ywych jednostek.
/// </summary>
/// <returns>Wynik bitwy (Player1Wins, Player2Wins lub Draw)</returns>
EMatchResult AStrategyGameMode::DetermineBattleWinner()
{
    if (!UnitManagerRef)
        return EMatchResult::Draw;

    // Policzenie ¿ywych jednostek ka¿dego gracza
    int32 Player1Units = CountPlayerUnits(0);
    int32 Player2Units = CountPlayerUnits(1);

    // Okreœlenie zwyciezcy na podstawie liczby jednostek
    if (Player1Units > Player2Units)
        return EMatchResult::Player1Wins;
    else if (Player2Units > Player1Units)
        return EMatchResult::Player2Wins;
    else
        return EMatchResult::Draw;
}

/// <summary>
/// Zlicza ¿ywe jednostki gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>Liczba ¿ywych jednostek gracza</returns>
int32 AStrategyGameMode::CountPlayerUnits(int32 PlayerID) const
{
    if (!UnitManagerRef)
        return 0;

    // Pobranie liczby ¿ywych jednostek gracza z UnitManagera
    int32 PlayerUnitsAlive = UnitManagerRef->GetAliveUnitCount(PlayerID);
    return PlayerUnitsAlive;
}

/// <summary>
/// Wyœwietla wynik bitwy na wszystkich klientach.
/// </summary>
/// <param name="Result">Wynik bitwy do wyœwietlenia</param>
void AStrategyGameMode::ShowMatchResult(EMatchResult Result)
{
    // Rozgloszenie wyniku do wszystkich klientow
    MulticastShowMatchResult(Result);
}

/// <summary>
/// Usuwa ¿ycie graczowi i aktualizuje UI na wszystkich klientach.
/// </summary>
/// <param name="PlayerID">ID gracza ktory traci ¿ycie</param>
void AStrategyGameMode::RemovePlayerLife(int32 PlayerID)
{
    // Pobranie danych gracza i odjecie ¿ycia
    FPlayerGameData* PlayerData = GetPlayerData(PlayerID);
    if (PlayerData && PlayerData->Lives > 0)
    {
        PlayerData->Lives--;
        MulticastUpdatePlayerLives(PlayerData->PlayerID, PlayerData->Lives);
        UE_LOG(LogTemp, Warning, TEXT("Gracz %d stracil ¿ycie. Pozostalo: %d"), PlayerID, PlayerData->Lives);
    }
}

/// <summary>
/// Wydaje zloto gracza na zakup.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="Amount">Iloœæ zlota do wydania</param>
/// <returns>True jeœli transakcja sie powiodla, false jeœli gracz nie ma wystarczaj¹co zlota</returns>
bool AStrategyGameMode::SpendPlayerGold(int32 PlayerID, int32 Amount)
{
    if (!HasAuthority())
        return false;

    // Sprawdzenie czy gracz ma wystarczaj¹co zlota
    FPlayerGameData* PlayerData = GetPlayerData(PlayerID);
    if (!PlayerData || PlayerData->Gold < Amount)
        return false;

    // Odjecie zlota i aktualizacja UI
    PlayerData->Gold -= Amount;
    MulticastUpdatePlayerGold(PlayerID, PlayerData->Gold);
    return true;
}

/// <summary>
/// Dodaje zloto graczowi i aktualizuje UI.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="Amount">Iloœæ zlota do dodania</param>
void AStrategyGameMode::AddPlayerGold(int32 PlayerID, int32 Amount)
{
    if (!HasAuthority())
        return;

    // Dodanie zlota graczowi i aktualizacja UI
    FPlayerGameData* PlayerData = GetPlayerData(PlayerID);
    if (PlayerData)
    {
        PlayerData->Gold += Amount;
        MulticastUpdatePlayerGold(PlayerID, PlayerData->Gold);
    }
}
/// <summary>
/// Obsluguje zakup jednostki przez gracza.
/// </summary>
/// <param name="UnitType">Typ jednostki do zakupu</param>
/// <param name="PlayerID">ID gracza dokonuj¹cego zakupu</param>
void AStrategyGameMode::OnUnitPurchased(int32 UnitType, int32 PlayerID)
{
    if (!HasAuthority())
        return;

    UE_LOG(LogTemp, Warning, TEXT("Zakup jednostki: Typ=%d, GraczID=%d, AktualnaFaza=%d"),
        UnitType, PlayerID, (int32)CurrentPhase);

    // Sprawdzenie czy jesteœmy w fazie zakupow
    if (CurrentPhase != EGamePhase::Purchase)
    {
        UE_LOG(LogTemp, Error, TEXT("ODRZUCONO: Nie mo¿na kupowaæ jednostek poza faz¹ zakupow. Aktualna: %d"),
            (int32)CurrentPhase);
        return;
    }

    // Pobranie kosztu jednostki i sprawdzenie czy gracz ma wystarczaj¹co zlota
    int32 UnitCost = GetUnitCost(UnitType);
    if (!SpendPlayerGold(PlayerID, UnitCost))
    {
        UE_LOG(LogTemp, Error, TEXT("ODRZUCONO: Gracz %d nie ma wystarczaj¹co zlota (%d) na jednostke typu %d"),
            PlayerID, UnitCost, UnitType);
        return;
    }

    // Zarejestrowanie zakupu
    RegisterUnitPurchase(PlayerID, UnitType);

    // Spawnowanie jednostki
    if (UnitManagerRef)
    {
        EBaseUnitType UnitTypeEnum = static_cast<EBaseUnitType>(UnitType);
        ABaseUnit* SpawnedUnit = UnitManagerRef->SpawnUnitForPlayer(PlayerID, UnitTypeEnum);

        if (SpawnedUnit)
        {
            UE_LOG(LogTemp, Warning, TEXT("SUKCES: Zespawnowano jednostke typu %d dla Gracza %d"),
                UnitType, PlayerID);

            MulticastUnitSpawned(UnitType, PlayerID, SpawnedUnit->GridPosition);
        }
        else
        {
            // Zwrot zlota w przypadku niepowodzenia spawnu
            UE_LOG(LogTemp, Error, TEXT("NIEPOWODZENIE: Nie mo¿na zespawnowaæ jednostki typu %d dla Gracza %d"),
                UnitType, PlayerID);

            AddPlayerGold(PlayerID, UnitCost);
        }
    }
    else
    {
        // Zwrot zlota jeœli brak UnitManagera
        UE_LOG(LogTemp, Error, TEXT("NIEPOWODZENIE: Nie znaleziono UnitManagera!"));
        AddPlayerGold(PlayerID, UnitCost);
    }
}

/// <summary>
/// Zwraca koszt jednostki w zale¿noœci od typu.
/// </summary>
/// <param name="UnitType">Typ jednostki</param>
/// <returns>Koszt jednostki w zlocie</returns>

int32 AStrategyGameMode::GetUnitCost(int32 UnitType) const
{
    // Zwrocenie kosztu jednostki w zale¿nosci od typu
    switch (UnitType)
    {
    case 0: return 50;   // Piechota
    case 1: return 75;   // lucznik
    case 2: return 100;  // Kawaler
    case 3: return 125;  // Mag
    default: return 50;
    }
}

/// <summary>
/// Rejestruje zakupion¹ jednostkê w liœcie zakupów gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="UnitType">Typ zakupionej jednostki</param>
void AStrategyGameMode::RegisterUnitPurchase(int32 PlayerID, int32 UnitType)
{
    // Dodanie zakupionej jednostki do listy zakupów gracza
    FPlayerGameData* PlayerData = GetPlayerData(PlayerID);
    if (PlayerData)
    {
        PlayerData->PurchasedUnits.Add(UnitType);
    }
}


/// <summary>
/// Znajduje dane gracza po ID
/// </summary>
/// <param name="PlayerID">ID gracza do znalezienia</param>
/// <returns>WskaŸnik do danych gracza lub nullptr jeœli nie znaleziono</returns>
FPlayerGameData* AStrategyGameMode::GetPlayerData(int32 PlayerID)
{
    // Wyszukanie danych gracza po ID
    for (FPlayerGameData& Data : PlayerDataArray)
    {
        if (Data.PlayerID == PlayerID)
        {
            return &Data;
        }
    }
    return nullptr;
}

/// <summary>
/// Znajduje dane gracza po ID
/// </summary>
/// <param name="PlayerID">ID gracza do znalezienia</param>
/// <returns>Sta³y wskaŸnik do danych gracza lub nullptr jeœli nie znaleziono</returns>
const FPlayerGameData* AStrategyGameMode::FindPlayerDataConst(int32 PlayerID) const
{
    // Wyszukanie sta³ych danych gracza po ID
    for (const FPlayerGameData& Data : PlayerDataArray)
    {
        if (Data.PlayerID == PlayerID)
        {
            return &Data;
        }
    }
    return nullptr;
}

/// <summary>
/// Znajduje ID gracza na podstawie kontrolera.
/// </summary>
/// <param name="Controller">Kontroler gracza</param>
/// <returns>ID gracza lub -1 jeœli nie znaleziono</returns>
int32 AStrategyGameMode::GetPlayerIDFromController(APlayerController* Controller) const
{
    // Znalezienie ID gracza na podstawie kontrolera
    for (const FPlayerGameData& Data : PlayerDataArray)
    {
        if (Data.PlayerController == Controller)
        {
            return Data.PlayerID;
        }
    }
    return -1;
}

/// <summary>
/// Obs³uguje wylogowanie gracza z gry.
/// </summary>
/// <param name="PlayerID">ID gracza opuszczaj¹cego grê</param>
void AStrategyGameMode::HandlePlayerLogout(int32 PlayerID)
{
    // Usuniêcie wszystkich jednostek gracza przy wylogowaniu
    if (UnitManagerRef)
    {
        TArray<ABaseUnit*> PlayerUnits = UnitManagerRef->GetPlayerUnits(PlayerID);
        for (ABaseUnit* Unit : PlayerUnits)
        {
            UnitManagerRef->RemoveUnit(Unit);
        }
    }

    // Usuniêcie gracza z tablicy
    RemovePlayerFromArray(PlayerID);
}

/// <summary>
/// Usuwa gracza z tablicy danych graczy.
/// </summary>
/// <param name="PlayerID">ID gracza do usuniêcia</param>
void AStrategyGameMode::RemovePlayerFromArray(int32 PlayerID)
{
    // Usuniêcie gracza z tablicy danych graczy
    for (int32 i = PlayerDataArray.Num() - 1; i >= 0; i--)
    {
        if (PlayerDataArray[i].PlayerID == PlayerID)
        {
            PlayerDataArray.RemoveAt(i);
            break;
        }
    }
}

/// <summary>
/// Resetuje dane fazowe dla wszystkich graczy.
/// </summary>
void AStrategyGameMode::ResetPlayerPhaseData()
{
    // Zresetowanie danych fazowych dla wszystkich graczy
    for (FPlayerGameData& Data : PlayerDataArray)
    {
        Data.bReadyForNextPhase = false;
        Data.PurchasedUnits.Empty();
    }
}


/// <summary>
/// Inicjalizuje referencjê do UnitManagera jeœli jeszcze nie istnieje.
/// </summary>
void AStrategyGameMode::InitializeUnitManager()
{
    // Inicjalizacja referencji do UnitManagera jeœli jeszcze nie istnieje
    if (UnitManagerRef)
        return;

    UnitManagerRef = Cast<AUnitManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AUnitManager::StaticClass()));

    if (UnitManagerRef)
    {
        BindUnitManagerEvents();
        UE_LOG(LogTemp, Warning, TEXT("UnitManager zainicjalizowany"));
    }
}

/// <summary>
/// Podpina zdarzenia z UnitManagera do callbacków GameMode.
/// </summary>
void AStrategyGameMode::BindUnitManagerEvents()
{
    // Podpiêcie eventów z UnitManagera
    if (UnitManagerRef)
    {
        UnitManagerRef->OnUnitSpawned.AddDynamic(this, &AStrategyGameMode::OnUnitSpawned);
        UnitManagerRef->OnUnitMoved.AddDynamic(this, &AStrategyGameMode::OnUnitMoved);
    }
}

/// <summary>
/// Callback wywo³ywany gdy jednostka zostanie zespawnowana.
/// </summary>
/// <param name="SpawnedUnit">Zespawnowana jednostka</param>
/// <param name="PlayerID">ID gracza w³aœciciela</param>
/// <param name="GridPosition">Pozycja na siatce</param>
void AStrategyGameMode::OnUnitSpawned(ABaseUnit* SpawnedUnit, int32 PlayerID, FVector2D GridPosition)
{
    // Callback wywo³ywany gdy jednostka zostanie zespawnowana
    UE_LOG(LogTemp, Log, TEXT("Jednostka zespawnowana dla Gracza %d"), PlayerID);
}

/// <summary>
/// Callback wywo³ywany gdy jednostka zostanie przemieszczona.
/// </summary>
/// <param name="MovedUnit">Przemieszczona jednostka</param>
/// <param name="OldPosition">Poprzednia pozycja</param>
/// <param name="NewPosition">Nowa pozycja</param>
/// <param name="PlayerID">ID gracza w³aœciciela</param>
void AStrategyGameMode::OnUnitMoved(ABaseUnit* MovedUnit, FVector2D OldPosition, FVector2D NewPosition, int32 PlayerID)
{
    // Callback wywo³ywany gdy jednostka zostanie przemieszczona
    UE_LOG(LogTemp, Log, TEXT("Jednostka przemieszczona dla Gracza %d"), PlayerID);
}

/// <summary>
/// Konwertuje enum fazy gry na tekst do wyœwietlenia.
/// </summary>
/// <param name="Phase">Faza gry</param>
/// <returns>Tekstowa reprezentacja fazy</returns>
FString AStrategyGameMode::GetPhaseText(EGamePhase Phase) const
{
    // Konwersja typu fazy na tekst
    switch (Phase)
    {
    case EGamePhase::WaitingForPlayers: return TEXT("Oczekiwanie na graczy");
    case EGamePhase::Purchase: return TEXT("Faza kupowania");
    case EGamePhase::Battle: return TEXT("Faza walki");
    case EGamePhase::Results: return TEXT("Wyniki");
    case EGamePhase::GameOver: return TEXT("Koniec gry");
    default: return TEXT("Nieznana");
    }
}

/// <summary>
/// Czyœci wszystkie jednostki z pola bitwy z synchronizacj¹ klient-serwer.
/// </summary>
void AStrategyGameMode::ClearAllUnitsFromBattlefield()
{
    if (!HasAuthority() || !UnitManagerRef)
        return;

    // Zabezpieczenie przed wielokrotnym wykonaniem
    static bool bClearingInProgress = false;
    if (bClearingInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("Czyszczenie jednostek ju¿ w toku - pomijanie"));
        return;
    }

    bClearingInProgress = true;

    UE_LOG(LogTemp, Warning, TEXT("SERWER: Czyszczenie wszystkich jednostek z pola bitwy"));

    // Zebranie wszystkich jednostek ze wszystkich graczy
    TArray<ABaseUnit*> AllUnits;
    TArray<FVector2D> UnitPositions;

    for (int32 PlayerID = 0; PlayerID < MaxPlayersPerGame; PlayerID++)
    {
        TArray<ABaseUnit*> PlayerUnits = UnitManagerRef->GetPlayerUnits(PlayerID);

        UE_LOG(LogTemp, Warning, TEXT("SERWER: Gracz %d ma %d jednostek"), PlayerID, PlayerUnits.Num());

        for (ABaseUnit* Unit : PlayerUnits)
        {
            if (Unit && IsValid(Unit))
            {
                AllUnits.Add(Unit);
                UnitPositions.Add(Unit->GridPosition);
                UE_LOG(LogTemp, Warning, TEXT("SERWER: Jednostka na (%f,%f) oznaczona do usuniêcia"),
                    Unit->GridPosition.X, Unit->GridPosition.Y);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("SERWER: £¹czna liczba jednostek do usuniêcia: %d"), AllUnits.Num());

    if (AllUnits.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SERWER: Brak jednostek do wyczyszczenia"));
        bClearingInProgress = false;
        return;
    }

    // Wys³anie RPC do klientów z pozycjami do wyczyszczenia
    UE_LOG(LogTemp, Warning, TEXT("SERWER: Wysylanie pozycji do wyczyszczenia do klientow"));
    MulticastClearSpecificUnits(UnitPositions);

    // Oczekiwanie na przetworzenie przez klientów, nastêpnie zniszczenie na serwerze
    FTimerHandle ClearTimer;
    GetWorldTimerManager().SetTimer(ClearTimer, [this, AllUnits]()
        {
            UE_LOG(LogTemp, Warning, TEXT("SERWER: Rozpoczêcie niszczenia jednostek po stronie serwera"));

            // Zniszczenie jednostek na serwerze
            for (ABaseUnit* Unit : AllUnits)
            {
                if (Unit && IsValid(Unit))
                {
                    UE_LOG(LogTemp, Warning, TEXT("SERWER: Niszczenie jednostki na (%f,%f)"),
                        Unit->GridPosition.X, Unit->GridPosition.Y);
                    Unit->Destroy();
                }
            }

            // Wyczyszczenie danych po stronie serwera
            if (UnitManagerRef)
            {
                UnitManagerRef->MulticastClearAllUnits();
                UnitManagerRef->ClearAllUnits();
            }

            // Wys³anie koñcowego potwierdzenia
            MulticastAllUnitsCleared();

            bClearingInProgress = false;
            UE_LOG(LogTemp, Warning, TEXT("SERWER: Czyszczenie jednostek zakoñczone"));

        }, 0.3f, false); // Zwiêkszone opóŸnienie do 300ms dla lepszej niezawodnoœci sieci
}


/// <summary>
/// Multicast RPC potwierdzaj¹cy zakoñczenie czyszczenia wszystkich jednostek.
/// </summary>
void AStrategyGameMode::MulticastAllUnitsCleared_Implementation()
{
    // Powiadomienie o zakoñczeniu czyszczenia jednostek
    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Otrzymano powiadomienie o wyczyszczeniu wszystkich jednostek"));

    if (!HasAuthority() && UnitManagerRef)
    {
        // Finalne czyszczenie po stronie klientów
        UnitManagerRef->FinalizeUnitClearing();
        UE_LOG(LogTemp, Warning, TEXT("KLIENT: Sfinalizowano czyszczenie jednostek"));
    }
}

/// <summary>
/// Multicast RPC przygotowuj¹cy klientów do czyszczenia jednostek.
/// </summary>
void AStrategyGameMode::MulticastPrepareUnitClearing_Implementation()
{
    // Przygotowanie do czyszczenia jednostek
    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Otrzymano przygotowanie czyszczenia jednostek"));

    if (!HasAuthority() && UnitManagerRef)
    {
        // Po stronie klientów - przygotowanie do czyszczenia
        UnitManagerRef->PrepareForUnitClearing();
        UE_LOG(LogTemp, Warning, TEXT("KLIENT: Przygotowano do czyszczenia jednostek"));
    }
}

/// <summary>
/// Multicast RPC czyszcz¹cy konkretne jednostki na podstawie pozycji.
/// Serwer niszczy swoje jednostki osobno z opóŸnieniem.
/// </summary>
/// <param name="UnitPositions">Tablica pozycji jednostek do zniszczenia</param>
void AStrategyGameMode::MulticastClearSpecificUnits_Implementation(const TArray<FVector2D>& UnitPositions)
{
    // Czyszczenie konkretnych jednostek po pozycjach
    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Otrzymano czyszczenie konkretnych jednostek - %d pozycji"), UnitPositions.Num());

    // Wykonuj tylko po stronie klientów
    if (!HasAuthority() && UnitManagerRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("KLIENT: Przetwarzanie czyszczenia jednostek na %d pozycjach"), UnitPositions.Num());

        // Wywo³anie UnitManagera do obs³ugi czyszczenia
        UnitManagerRef->DestroyUnitsAtPositions(UnitPositions);

        UE_LOG(LogTemp, Warning, TEXT("KLIENT: ¯¹danie czyszczenia jednostek wys³ane do UnitManagera"));
    }
    else if (HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("SERWER: Otrzymano potwierdzenie multicast"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("KLIENT: Brak dostêpnego UnitManagerRef do czyszczenia!"));
    }
}

/// <summary>
/// Definiuje które w³aœciwoœci s¹ synchronizowane przez sieæ.
/// </summary>
/// <param name="OutLifetimeProps">Tablica w³aœciwoœci do replikacji</param>
void AStrategyGameMode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // Replikacja aktualnej fazy gry do wszystkich klientów
    DOREPLIFETIME(AStrategyGameMode, CurrentPhase);
}

/// <summary>
/// Server RPC obs³uguj¹cy ¿¹danie zakupu jednostki od klienta.
/// </summary>
/// <param name="UnitType">Typ jednostki do zakupu</param>
/// <param name="PlayerID">ID gracza dokonuj¹cego zakupu</param>
void AStrategyGameMode::ServerRequestUnitPurchase_Implementation(int32 UnitType, int32 PlayerID)
{
    // Przetworzenie ¿¹dania zakupu jednostki
    OnUnitPurchased(UnitType, PlayerID);
}

/// <summary>
/// Walidacja parametrów Server RPC zakupu jednostki.
/// </summary>
/// <param name="UnitType">Typ jednostki (0-3)</param>
/// <param name="PlayerID">ID gracza (>=0)</param>
/// <returns>True jeœli parametry s¹ poprawne</returns>
bool AStrategyGameMode::ServerRequestUnitPurchase_Validate(int32 UnitType, int32 PlayerID)
{
    // Walidacja parametrów ¿¹dania zakupu
    return UnitType >= 0 && UnitType < 4 && PlayerID >= 0;
}

/// <summary>
/// Multicast RPC powiadamiaj¹cy o do³¹czeniu gracza.
/// </summary>
/// <param name="PlayerID">ID gracza który do³¹czy³</param>
/// <param name="PlayerName">Nazwa gracza</param>
void AStrategyGameMode::MulticastPlayerJoined_Implementation(int32 PlayerID, const FString& PlayerName)
{
    // Powiadomienie o do³¹czeniu gracza
    UE_LOG(LogTemp, Log, TEXT("Gracz %d (%s) do³¹czy³"), PlayerID, *PlayerName);
}

/// <summary>
/// Multicast RPC powiadamiaj¹cy o opuszczeniu gry przez gracza.
/// </summary>
/// <param name="PlayerID">ID gracza który opuœci³ grê</param>
void AStrategyGameMode::MulticastPlayerLeft_Implementation(int32 PlayerID)
{
    // Powiadomienie o opuszczeniu gry przez gracza
    UE_LOG(LogTemp, Log, TEXT("Gracz %d opusci³ gre"), PlayerID);
}

/// <summary>
/// Multicast RPC aktualizuj¹cy z³oto gracza w UI.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="NewGold">Nowa wartoœæ z³ota</param>
void AStrategyGameMode::MulticastUpdatePlayerGold_Implementation(int32 PlayerID, int32 NewGold)
{
    // Aktualizacja z³ota gracza w UI
    UpdatePlayerGoldUI(PlayerID, NewGold);
}

/// <summary>
/// Multicast RPC aktualizuj¹cy liczbê ¿yæ gracza w UI.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="NewLives">Nowa liczba ¿yæ</param>
void AStrategyGameMode::MulticastUpdatePlayerLives_Implementation(int32 PlayerID, int32 NewLives)
{
    // Aktualizacja liczby ¿yæ gracza w UI
    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGameUI::StaticClass());

    for (UUserWidget* Widget : FoundWidgets)
    {
        if (UGameUI* GameUI = Cast<UGameUI>(Widget))
        {
            FPlayerGameData* PlayerData = GetPlayerData(PlayerID);
            if (PlayerData)
            {
                GameUI->UpdatePlayerInfo(PlayerData->PlayerID, PlayerData->PlayerName, NewLives);
            }
        }
    }
}

/// <summary>
/// Aktualizuje UI z³ota tylko dla lokalnego gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="NewGold">Nowa wartoœæ z³ota</param>
void AStrategyGameMode::UpdatePlayerGoldUI(int32 PlayerID, int32 NewGold)
{
    // Aktualizacja UI z³ota tylko dla lokalnego gracza
    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGameUI::StaticClass());

    for (UUserWidget* Widget : FoundWidgets)
    {
        if (UGameUI* GameUI = Cast<UGameUI>(Widget))
        {
            AStrategyPlayerController* LocalPC = Cast<AStrategyPlayerController>(GameUI->GetOwningPlayer());
            if (LocalPC && LocalPC->GetPlayerID() == PlayerID)
            {
                GameUI->UpdateGold(NewGold);
            }
        }
    }
}

/// <summary>
/// Multicast RPC powiadamiaj¹cy o zespawnowaniu jednostki.
/// </summary>
/// <param name="UnitType">Typ jednostki</param>
/// <param name="PlayerID">ID gracza w³aœciciela</param>
/// <param name="GridPosition">Pozycja na siatce</param>
void AStrategyGameMode::MulticastUnitSpawned_Implementation(int32 UnitType, int32 PlayerID, FVector2D GridPosition)
{
    // Powiadomienie o zespawnowaniu jednostki
    UE_LOG(LogTemp, Log, TEXT("Jednostka zespawnowana: Typ %d, Gracz %d"), UnitType, PlayerID);
}

/// <summary>
/// Multicast RPC zmieniaj¹cy fazê gry na wszystkich klientach.
/// </summary>
/// <param name="NewPhase">Nowa faza gry</param>
/// <param name="PhaseTime">Czas trwania fazy (0 = bez limitu)</param>
void AStrategyGameMode::MulticastPhaseChanged_Implementation(EGamePhase NewPhase, float PhaseTime)
{
    // Zmiana fazy gry
    CurrentPhase = NewPhase;
    FString PhaseText = GetPhaseText(NewPhase);

    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Zmiana fazy na: %s (Czas: %.1f)"), *PhaseText, PhaseTime);

    // Aktualizacja wszystkich instancji UI
    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGameUI::StaticClass());

    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Znaleziono %d widgetów GameUI"), FoundWidgets.Num());

    for (UUserWidget* Widget : FoundWidgets)
    {
        if (UGameUI* GameUI = Cast<UGameUI>(Widget))
        {
            UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Aktualizacja widgetu GameUI"));

            // Aktualizacja wyœwietlania fazy
            GameUI->UpdatePhaseDisplay(PhaseText);

            // W³¹czenie/wy³¹czenie sklepu w zale¿noœci od fazy
            bool bShopEnabled = (NewPhase == EGamePhase::Purchase);
            GameUI->SetShopEnabled(bShopEnabled);

            // Start timera jeœli faza ma limit czasu
            if (PhaseTime > 0.0f)
            {
                UE_LOG(LogTemp, Error, TEXT("MULTICAST: Uruchomienie timera na %.1f sekund"), PhaseTime);
                GameUI->StartPhaseTimer(PhaseTime);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("MULTICAST: Zatrzymanie timera (CzasFazy = %.1f)"), PhaseTime);
                GameUI->StopPhaseTimer();
            }

            // Ukrycie komunikatu oczekiwania jeœli nie jesteœmy w fazie oczekiwania
            if (NewPhase != EGamePhase::WaitingForPlayers)
            {
                GameUI->HideWaitingForPlayers();
            }
        }
    }
}

/// <summary>
/// Multicast RPC wyœwietlaj¹cy wynik bitwy na wszystkich klientach.
/// </summary>
/// <param name="Result">Wynik bitwy (Player1Wins, Player2Wins lub Draw)</param>
void AStrategyGameMode::MulticastShowMatchResult_Implementation(EMatchResult Result)
{
    // Wyœwietlenie wyniku meczu
    FString ResultText;

    switch (Result)
    {
    case EMatchResult::Player1Wins:
    {
        FPlayerGameData* Player1Data = GetPlayerData(0);
        ResultText = FString::Printf(TEXT("%s wygrywa!"),
            Player1Data ? *Player1Data->PlayerName : TEXT("Gracz 1"));
    }
    break;
    case EMatchResult::Player2Wins:
    {
        FPlayerGameData* Player2Data = GetPlayerData(1);
        ResultText = FString::Printf(TEXT("%s wygrywa!"),
            Player2Data ? *Player2Data->PlayerName : TEXT("Gracz 2"));
    }
    break;
    case EMatchResult::Draw:
        ResultText = TEXT("Remis!");
        break;
    default:
        ResultText = TEXT("Nieznany wynik");
        break;
    }

    // Aktualizacja wszystkich widgetów UI z wynikiem
    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGameUI::StaticClass());

    for (UUserWidget* Widget : FoundWidgets)
    {
        if (UGameUI* GameUI = Cast<UGameUI>(Widget))
        {
            GameUI->ShowMatchResult(ResultText, ResultsPhaseTime);
        }
    }
}

/// <summary>
/// Multicast RPC aktualizuj¹cy informacje o wszystkich graczach w UI.
/// </summary>
void AStrategyGameMode::MulticastUpdatePlayersInfo_Implementation()
{
    // Aktualizacja informacji o wszystkich graczach
    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Wywo³ano UpdatePlayersInfo"));

    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGameUI::StaticClass());

    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Znaleziono %d widgetów GameUI do aktualizacji informacji o graczach"), FoundWidgets.Num());

    for (UUserWidget* Widget : FoundWidgets)
    {
        if (UGameUI* GameUI = Cast<UGameUI>(Widget))
        {
            UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Aktualizacja informacji o graczach w widgecie GameUI"));

            // Aktualizacja informacji dla wszystkich graczy
            for (const FPlayerGameData& PlayerData : PlayerDataArray)
            {
                UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Aktualizacja info gracza: ID=%d, Nazwa=%s, ¯ycia=%d"),
                    PlayerData.PlayerID, *PlayerData.PlayerName, PlayerData.Lives);

                GameUI->UpdatePlayerInfo(PlayerData.PlayerID, PlayerData.PlayerName, PlayerData.Lives);
            }

            // Wype³nienie brakuj¹cych slotów graczy domyœlnymi informacjami
            if (PlayerDataArray.Num() < 2)
            {
                for (int32 i = PlayerDataArray.Num(); i < 2; i++)
                {
                    FString DefaultName = FString::Printf(TEXT("Gracz %d: Oczekiwanie..."), i + 1);
                    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Dodawanie domyœlnej informacji gracza dla slotu %d"), i);
                    GameUI->UpdatePlayerInfo(i, DefaultName, 3);
                }
            }
        }
    }
}

/// <summary>
/// Multicast RPC wyœwietlaj¹cy ekran oczekiwania na graczy.
/// </summary>
/// <param name="CurrentCount">Aktualna liczba graczy</param>
/// <param name="RequiredCount">Wymagana liczba graczy</param>
void AStrategyGameMode::MulticastShowWaitingForPlayers_Implementation(int32 CurrentCount, int32 RequiredCount)
{
    // Wyœwietlenie ekranu oczekiwania na graczy
    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Wyœwietlanie oczekiwania na graczy: %d/%d"), CurrentCount, RequiredCount);

    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGameUI::StaticClass());

    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Znaleziono %d widgetów do aktualizacji oczekiwania"), FoundWidgets.Num());

    for (UUserWidget* Widget : FoundWidgets)
    {
        if (UGameUI* GameUI = Cast<UGameUI>(Widget))
        {
            UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Aktualizacja wyœwietlania oczekiwania w GameUI"));
            GameUI->ShowWaitingForPlayers(CurrentCount, RequiredCount);
        }
    }
}

/// <summary>
/// Multicast RPC synchronizuj¹cy dane pojedynczego gracza do wszystkich klientów.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="PlayerName">Nazwa gracza</param>
/// <param name="Gold">Z³oto gracza</param>
/// <param name="Lives">¯ycia gracza</param>
void AStrategyGameMode::MulticastSyncPlayerData_Implementation(int32 PlayerID, const FString& PlayerName, int32 Gold, int32 Lives)
{
    // Multicast - synchronizacja danych gracza
    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Synchronizacja danych gracza - ID=%d, Nazwa=%s, Z³oto=%d, ¯ycia=%d"),
        PlayerID, *PlayerName, Gold, Lives);
    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: HasAuthority=%s"), HasAuthority() ? TEXT("SERWER") : TEXT("KLIENT"));

    // Aktualizacja lokalnych danych tylko po stronie klientów, serwer ju¿ je posiada
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("KLIENT: Przetwarzanie synchronizacji danych gracza"));

        // Znalezienie lub utworzenie danych gracza lokalnie na kliencie
        bool bFoundPlayer = false;
        for (FPlayerGameData& Data : PlayerDataArray)
        {
            if (Data.PlayerID == PlayerID)
            {
                UE_LOG(LogTemp, Warning, TEXT("KLIENT: Aktualizacja istniej¹cych danych gracza %d"), PlayerID);
                Data.PlayerName = PlayerName;
                Data.Gold = Gold;
                Data.Lives = Lives;
                bFoundPlayer = true;
                break;
            }
        }

        // Jeœli gracz nie zosta³ znaleziony, utwórz nowy wpis
        if (!bFoundPlayer)
        {
            UE_LOG(LogTemp, Warning, TEXT("KLIENT: Tworzenie nowego wpisu danych dla Gracza %d"), PlayerID);
            FPlayerGameData NewData;
            NewData.PlayerID = PlayerID;
            NewData.PlayerName = PlayerName;
            NewData.Gold = Gold;
            NewData.Lives = Lives;
            NewData.PlayerController = nullptr; // Klienci nie potrzebuj¹ tego
            NewData.bReadyForNextPhase = false;
            PlayerDataArray.Add(NewData);
        }
    }

    // Aktualizacja UI na WSZYSTKICH maszynach 
    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGameUI::StaticClass());

    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Znaleziono %d widgetów UI do aktualizacji"), FoundWidgets.Num());

    for (UUserWidget* Widget : FoundWidgets)
    {
        if (UGameUI* GameUI = Cast<UGameUI>(Widget))
        {
            UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Aktualizacja widgetu UI informacj¹ o graczu %d"), PlayerID);
            GameUI->UpdatePlayerInfo(PlayerID, PlayerName, Lives);

            // Aktualizacja z³ota tylko dla lokalnego gracza
            AStrategyPlayerController* LocalPC = Cast<AStrategyPlayerController>(GameUI->GetOwningPlayer());
            if (LocalPC && LocalPC->GetPlayerID() == PlayerID)
            {
                UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Aktualizacja z³ota dla lokalnego gracza %d: %d"), PlayerID, Gold);
                GameUI->UpdateGold(Gold);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("MULTICAST: Synchronizacja danych gracza zakoñczona dla Gracza %d"), PlayerID);
}

/// <summary>
/// Aktualizuje UI dla wszystkich graczy przez wywo³anie multicast.
/// </summary>
void AStrategyGameMode::UpdateAllPlayersUI()
{
    // Aktualizacja UI dla wszystkich graczy
    UE_LOG(LogTemp, Warning, TEXT("UpdateAllPlayersUI wywo³ane - rozmiar PlayerDataArray: %d"), PlayerDataArray.Num());

    // Wywo³anie multicast do aktualizacji wszystkich klientów
    MulticastUpdatePlayersInfo();
}

/// <summary>
/// Rozg³asza wiadomoœæ czatu do wszystkich graczy.
/// </summary>
/// <param name="Message">Treœæ wiadomoœci</param>
/// <param name="SenderName">Nazwa nadawcy</param>
void AStrategyGameMode::BroadcastToAllPlayers(const FString& Message, const FString& SenderName)
{
    // Rozg³oszenie wiadomoœci czatu do wszystkich graczy
    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGameUI::StaticClass());

    for (UUserWidget* Widget : FoundWidgets)
    {
        if (UGameUI* GameUI = Cast<UGameUI>(Widget))
        {
            GameUI->AddChatMessage(Message, SenderName);
        }
    }
}

/// <summary>
/// Aktualizuje dane gracza w tablicy.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="Gold">Nowe z³oto</param>
/// <param name="bReady">Status gotowoœci</param>
void AStrategyGameMode::UpdatePlayerDataArray(int32 PlayerID, int32 Gold, bool bReady)
{
    // Aktualizacja danych gracza w tablicy
    for (FPlayerGameData& PlayerData : PlayerDataArray)
    {
        if (PlayerData.PlayerID == PlayerID)
        {
            PlayerData.Gold = Gold;
            PlayerData.bReadyForNextPhase = bReady;
            break;
        }
    }
}

/// <summary>
/// Synchronizuje dane graczy do klientów przez multicast.
/// </summary>
void AStrategyGameMode::SyncPlayerDataToClients()
{
    // Synchronizacja danych graczy do klientów
    MulticastUpdatePlayersInfo();
}

/// <summary>
/// Placeholder dla tworzenia UI gracza.
/// </summary>
/// <param name="PlayerController">Kontroler gracza</param>
/// <param name="PlayerID">ID gracza</param>
void AStrategyGameMode::CreateUIForPlayer(APlayerController* PlayerController, int32 PlayerID)
{
    // Utworzenie UI dla gracza 
    UE_LOG(LogTemp, Warning, TEXT("Tworzenie UI obs³ugiwane przez PlayerController"));
}

/// <summary>
/// Sprawdza liczbê graczy i rozpoczyna fazê zakupów jeœli jest wystarczaj¹ca.
/// </summary>
void AStrategyGameMode::DelayedGameStart()
{
    // OpóŸnione rozpoczêcie gry
    if (HasEnoughPlayers())
    {
        InitializePurchasePhase();
    }
}

/// <summary>
/// Pobiera iloœæ z³ota gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>Z³oto gracza lub 0 jeœli nie znaleziono</returns>
int32 AStrategyGameMode::GetPlayerGold(int32 PlayerID) const
{
    // Pobranie iloœci z³ota gracza
    const FPlayerGameData* PlayerData = FindPlayerDataConst(PlayerID);
    return PlayerData ? PlayerData->Gold : 0;
}

/// <summary>
/// Sprawdza czy gracz zosta³ wyeliminowany
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>True jeœli gracz nie ma ¿yæ</returns>
bool AStrategyGameMode::IsPlayerEliminated(int32 PlayerID) const
{
    // Sprawdzenie czy gracz zosta³ wyeliminowany
    const FPlayerGameData* PlayerData = FindPlayerDataConst(PlayerID);
    return PlayerData ? (PlayerData->Lives <= 0) : true;
}

/// <summary>
/// Pobiera liczbê ¿yæ gracza.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>Liczba ¿yæ lub 0 jeœli nie znaleziono</returns>
int32 AStrategyGameMode::GetPlayerLives(int32 PlayerID) const
{
    // Pobranie liczby ¿yæ gracza
    const FPlayerGameData* PlayerData = FindPlayerDataConst(PlayerID);
    return PlayerData ? PlayerData->Lives : 0;
}

/// <summary>
/// Resetuje liczbê ¿yæ dla wszystkich graczy do wartoœci startowej.
/// </summary>
void AStrategyGameMode::ResetPlayerLives()
{
    // Zresetowanie liczby ¿yæ dla wszystkich graczy
    for (FPlayerGameData& PlayerData : PlayerDataArray)
    {
        PlayerData.Lives = StartingLives;
    }
}

/// <summary>
/// Ustawia status gotowoœci gracza do nastêpnej fazy.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <param name="bReady">Status gotowoœci</param>
void AStrategyGameMode::SetPlayerReadyForNextPhase(int32 PlayerID, bool bReady)
{
    // Ustawienie statusu gotowoœci gracza do nastêpnej fazy
    FPlayerGameData* PlayerData = GetPlayerData(PlayerID);
    if (PlayerData)
    {
        PlayerData->bReadyForNextPhase = bReady;
        if (bReady)
        {
            CheckAllPlayersReady();
        }
    }
}

/// <summary>
/// Sprawdza czy wszyscy gracze s¹ gotowi do nastêpnej fazy.
/// </summary>
void AStrategyGameMode::CheckAllPlayersReady()
{
    // Sprawdzenie czy wszyscy gracze s¹ gotowi
    if (PlayerDataArray.Num() == 0)
        return;

    bool bAllReady = AreAllPlayersReady();
    if (bAllReady)
    {
        ResetPlayerReadyStatus();
        SchedulePhaseTransition();
    }
}

/// <summary>
/// Sprawdza czy wszyscy gracze s¹ gotowi.
/// </summary>
/// <returns>True jeœli wszyscy gracze maj¹ flagê bReadyForNextPhase</returns>
bool AStrategyGameMode::AreAllPlayersReady() const
{
    // Sprawdzenie czy wszyscy gracze s¹ gotowi do nastêpnej fazy
    for (const FPlayerGameData& Data : PlayerDataArray)
    {
        if (!Data.bReadyForNextPhase)
        {
            return false;
        }
    }
    return true;
}

/// <summary>
/// Resetuje status gotowoœci wszystkich graczy.
/// Wywo³ywane po rozpoczêciu nowej fazy.
/// </summary>
void AStrategyGameMode::ResetPlayerReadyStatus()
{
    // Zresetowanie statusu gotowoœci wszystkich graczy
    for (FPlayerGameData& Data : PlayerDataArray)
    {
        Data.bReadyForNextPhase = false;
    }
}

/// <summary>
/// Planuje przejœcie do nastêpnej fazy z 1-sekundowym opóŸnieniem.
/// </summary>
void AStrategyGameMode::SchedulePhaseTransition()
{
    // Zaplanowanie przejœcia do nastêpnej fazy
    GetWorldTimerManager().SetTimer(DelayedStartTimer,
        FTimerDelegate::CreateUObject(this, &AStrategyGameMode::StartNextPhase), 1.0f, false);
}

/// <summary>
/// Pobiera listê zakupionych jednostek gracza w bie¿¹cej fazie.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>Tablica typów zakupionych jednostek</returns>
TArray<int32> AStrategyGameMode::GetPlayerPurchasedUnits(int32 PlayerID) const
{
    // Pobranie listy zakupionych jednostek gracza
    const FPlayerGameData* PlayerData = FindPlayerDataConst(PlayerID);
    return PlayerData ? PlayerData->PurchasedUnits : TArray<int32>();
}

/// <summary>
/// Aktualizuje UI gracza z aktualnymi danymi.
/// </summary>
/// <param name="PlayerID">ID gracza</param>
void AStrategyGameMode::UpdatePlayerUI(int32 PlayerID)
{
    // Aktualizacja UI gracza
    FPlayerGameData* PlayerData = GetPlayerData(PlayerID);
    if (!PlayerData || !PlayerData->PlayerController)
        return;

    if (AStrategyPlayerController* StratPC = Cast<AStrategyPlayerController>(PlayerData->PlayerController))
    {
        StratPC->UpdateUIGold(PlayerData->Gold);
    }
}