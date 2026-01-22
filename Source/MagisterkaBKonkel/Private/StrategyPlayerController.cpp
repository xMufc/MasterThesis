// StrategyPlayerController.cpp - Kontroler gracza dla gry strategicznej turowej
#include "StrategyPlayerController.h"
#include "StrategyGameMode.h"
#include "GridManager.h"
#include "UnitManager.h"
#include "GameUI.h"
#include "BaseUnit.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

/// <summary>
/// Konstruktor kontrolera gracza
/// </summary>
AStrategyPlayerController::AStrategyPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;

    // Konfiguracja myszy 
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    // Inicjalizacja zmiennych gracza
    PlayerID = -1; 
    ClickRange = 10000.0f; // Zasiêg raytrace dla wykrywania klikniêæ
    bEnableDoubleClick = false;
    DoubleClickTime = 0.5f;
    bShowDebugInfo = false;
    bDrawDebugLines = false;
    LastClickTime = 0.0f;
    LastClickPosition = FVector2D::ZeroVector;
    bInitialized = false;

    // Wyzerowanie referencji do kluczowych systemów gry
    GridManagerRef = nullptr;
    UnitManagerRef = nullptr;
    GameModeRef = nullptr;
    GameUIRef = nullptr;

    // W³¹czamy replikacjê sieciow¹ dla kontrolera
    bReplicates = true;

    UE_LOG(LogTemp, Warning, TEXT("StrategyPlayerController Constructor"));
}

/// <summary>
/// Wywo³ywana przy rozpoczêciu gry 
/// </summary>
void AStrategyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("PlayerController BeginPlay START"));
    UE_LOG(LogTemp, Warning, TEXT("IsLocalController: %s"), IsLocalController() ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Warning, TEXT("HasAuthority: %s"), HasAuthority() ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Warning, TEXT("PlayerID on start: %d"), PlayerID);

    // OpóŸniona inicjalizacja
    FTimerHandle InitTimer;
    GetWorldTimerManager().SetTimer(InitTimer, [this]()
        {
            InitializeReferences();

            // Tylko lokalny gracz tworzy UI
            if (IsLocalPlayerController())
            {
                UE_LOG(LogTemp, Warning, TEXT("Creating GameUI for local player"));
                CreateGameUI();
            }
        }, 0.5f, false);
}

/// <summary>
/// Inicjalizuje referencje do kluczowych systemów gry
/// </summary>
void AStrategyPlayerController::InitializeReferences()
{
    // ZnajdŸ GridManager w œwiecie gry
    if (!GridManagerRef)
    {
        GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
        if (GridManagerRef)
        {
            UE_LOG(LogTemp, Warning, TEXT("GridManager found"));
        }
    }

    // ZnajdŸ UnitManager w œwiecie gry
    if (!UnitManagerRef)
    {
        UnitManagerRef = Cast<AUnitManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AUnitManager::StaticClass()));
        if (UnitManagerRef)
        {
            UE_LOG(LogTemp, Warning, TEXT("UnitManager found"));
        }
    }

    // GameMode istnieje tylko na serwerze, nigdy na kliencie
    if (HasAuthority())
    {
        if (!GameModeRef)
        {
            GameModeRef = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
            if (GameModeRef)
            {
                UE_LOG(LogTemp, Warning, TEXT("GameMode found (SERVER)"));
            }
        }
    }
    else
    {
        // Klient nie ma dostêpu do GameMode
        GameModeRef = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: GameMode not available (normal)"));
    }

    // Kontroler jest w pe³ni zainicjalizowany gdy ma dostêp do GridManager i UnitManager
    bInitialized = (GridManagerRef && UnitManagerRef);

    if (bInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerController %d fully initialized"), PlayerID);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerController %d failed to initialize"), PlayerID);
    }
}

/// <summary>
/// Konfiguruje przyciski i skróty klawiszowe
/// </summary>
void AStrategyPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (InputComponent)
    {
        // Bindowanie akcji z Project Settings
        InputComponent->BindAction("LeftMouseClick", IE_Pressed, this, &AStrategyPlayerController::OnLeftMouseClick);
        InputComponent->BindAction("RightMouseClick", IE_Pressed, this, &AStrategyPlayerController::OnRightMouseClick);
        InputComponent->BindAction("Escape", IE_Pressed, this, &AStrategyPlayerController::OnEscapePressed);

        // Debugowe skróty klawiszowe
        InputComponent->BindAction("ToggleGrid", IE_Pressed, this, &AStrategyPlayerController::ToggleGridVisibility);
        InputComponent->BindAction("ShowSpawnZones", IE_Pressed, this, &AStrategyPlayerController::ShowPlayerSpawnZones);

        UE_LOG(LogTemp, Warning, TEXT("Input bindings completed for PlayerController %d"), PlayerID);
    }
}

/// <summary>
/// Obs³uguje klikniêcie lewym przyciskiem myszy
/// </summary>
void AStrategyPlayerController::OnLeftMouseClick()
{
    // Tylko lokalny gracz mo¿e klikaæ i kontroler musi byæ zainicjalizowany
    if (!IsLocalPlayerController() || !bInitialized)
        return;

    // SprawdŸ czy w obecnej fazie gry mo¿na wchodziæ w interakcjê z jednostkami
    if (!CanInteractWithUnits())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot interact with units in current phase"));
        return;
    }

    FVector2D GridPosition;
    ABaseUnit* UnitUnderCursor = nullptr;

    // SprawdŸ czy klikniêto na jednostkê
    if (GetUnitUnderCursor(UnitUnderCursor))
    {
        UE_LOG(LogTemp, Warning, TEXT("Unit under cursor found - calling ServerHandleUnitClick"));
        ServerHandleUnitClick(UnitUnderCursor, PlayerID);
    }
    // Jeœli nie, sprawdŸ czy klikniêto na siatkê
    else if (GetGridPositionUnderCursor(GridPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("Grid position under cursor: (%f, %f) - calling ServerHandleGridClick"),
            GridPosition.X, GridPosition.Y);
        ServerHandleGridClick(GridPosition, PlayerID);
    }
}

/// <summary>
/// Obs³uguje klikniêcie prawym przyciskiem myszy
/// </summary>
void AStrategyPlayerController::OnRightMouseClick()
{
    // Tylko lokalny gracz mo¿e u¿ywaæ prawego przycisku myszy
    if (!IsLocalPlayerController() || !UnitManagerRef)
        return;

    // Prawy przycisk myszy odznacza aktualnie wybran¹ jednostkê
    ServerDeselectUnit();
}

/// <summary>
/// Obs³uguje naciœniêcie klawisza Escape
/// </summary>
void AStrategyPlayerController::OnEscapePressed()
{
    // Tylko lokalny gracz mo¿e u¿ywaæ klawisza Escape
    if (!IsLocalPlayerController())
        return;

    // Klawisz Escape równie¿ odznacza aktualnie wybran¹ jednostkê
    ServerDeselectUnit();
}

/// <summary>
/// Pobiera pozycjê na siatce pod kursorem myszy
/// </summary>
/// <param name="OutGridPosition">Pozycja na siatce</param>
/// <returns>true jeœli znaleziono prawid³ow¹ pozycjê na siatce, false wpp</returns>
bool AStrategyPlayerController::GetGridPositionUnderCursor(FVector2D& OutGridPosition) const
{
    if (!GridManagerRef)
        return false;

    // Wykonaj raycast z pozycji kursora myszy
    FHitResult HitResult;
    bool bHit = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResult);

    if (bHit)
    {
        // Konwertuj pozycjê œwiatow¹ na pozycjê w siatce
        OutGridPosition = GridManagerRef->GetGridPositionFromWorld(HitResult.Location);
        return GridManagerRef->IsValidGridPosition(OutGridPosition);
    }

    return false;
}

/// <summary>
/// Pobiera jednostkê znajduj¹c¹ siê pod kursorem myszy
/// </summary>
/// <param name="OutUnit">WskaŸnik do jednostki</param>
/// <returns>true jeœli znaleziono jednostkê pod kursorem, false wpp</returns>
bool AStrategyPlayerController::GetUnitUnderCursor(ABaseUnit*& OutUnit) const
{
    if (!UnitManagerRef)
        return false;

    // Wykonaj raycast sprawdzaj¹cy kolizje z Pawn
    FHitResult HitResult;
    bool bHit = GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, HitResult);

    if (bHit)
    {
        OutUnit = Cast<ABaseUnit>(HitResult.GetActor());
        return OutUnit != nullptr;
    }

    return false;
}

/// <summary>
/// Sprawdza czy gracz mo¿e w tej chwili wchodziæ w interakcjê z jednostkami
/// </summary>
/// <returns>true jeœli interakcja jest mo¿liwa, false wpp</returns>
bool AStrategyPlayerController::CanInteractWithUnits() const
{
    // Klient nie ma dostêpu do GameMode, wiêc sprawdzamy fazê gry inaczej

    if (HasAuthority())
    {
        // Na serwerze sprawdzamy bezpoœrednio w GameMode
        if (!GameModeRef)
            return false;
        EGamePhase CurrentPhase = GameModeRef->GetCurrentPhase();
        // Interakcja z jednostkami jest mo¿liwa tylko w fazie zakupów/rozmieszczania
        return CurrentPhase == EGamePhase::Purchase;
    }
    else
    {
        // Sprawdzamy czy sklep jest aktywny w UI (to oznacza fazê Purchase)
        if (GameUIRef)
        {
            // Jeœli shop jest w³¹czony, to jesteœmy w fazie Purchase
            bool bShopEnabled = GameUIRef->GetShopEnabled();
            UE_LOG(LogTemp, Warning, TEXT("CLIENT: CanInteractWithUnits - ShopEnabled: %s"),
                bShopEnabled ? TEXT("YES") : TEXT("NO"));
            return bShopEnabled;
        }

        // Jeœli nie ma UI, pozwól na interakcjê
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: CanInteractWithUnits - No UI, allowing interaction"));
        return true;
    }
}

/// <summary>
/// Sprawdza czy gra jest w fazie rozmieszczania jednostek
/// </summary>
/// <returns>true jeœli jest faza rozmieszczania, false wpp</returns>
bool AStrategyPlayerController::IsDeploymentPhase() const
{
    if (!GameModeRef)
        return false;

    // Faza rozmieszczania jest czêœci¹ fazy zakupów 
    return GameModeRef->GetCurrentPhase() == EGamePhase::Purchase;
}

/// <summary>
/// Tworzy i inicjalizuje interfejs u¿ytkownika
/// </summary>
void AStrategyPlayerController::CreateGameUI()
{
    UE_LOG(LogTemp, Warning, TEXT("CreateGameUI START"));

    // Tylko lokalny gracz tworzy UI i tylko jeœli jeszcze nie istnieje
    if (!IsLocalPlayerController() || GameUIRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateGameUI - Early return"));
        return;
    }

    // GameUIClass musi byæ ustawiona w Blueprint dziedziczy¹cym z tego kontrolera
    if (GameUIClass)
    {
        GameUIRef = CreateWidget<UGameUI>(this, GameUIClass);
        if (GameUIRef)
        {
            // Dodaj widget UI do ekranu
            GameUIRef->AddToViewport();

            // Pod³¹cz event delegata dla zakupu jednostek
            if (!GameUIRef->OnUnitPurchaseRequested.IsBound())
            {
                GameUIRef->OnUnitPurchaseRequested.AddDynamic(this, &AStrategyPlayerController::OnUnitPurchaseRequested);
                UE_LOG(LogTemp, Warning, TEXT("UI delegate bound successfully"));
            }

            UE_LOG(LogTemp, Warning, TEXT("UI created for Player %d"), PlayerID);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("FAILED to create GameUI widget!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GameUIClass is NULL! Set it in Blueprint!"));
    }
}
/// <summary>
///  Wyœwietla podœwietlenie wybranej jednostki po stronie klienta
/// </summary>
/// <param name="UnitGridPosition">Pozycja jednostki na siatce</param>
/// <param name="RequestingPlayerID">ID gracza ¿¹daj¹cego wyœwietlenia</param>
void AStrategyPlayerController::ClientShowUnitSelectionByPosition_Implementation(FVector2D UnitGridPosition, int32 RequestingPlayerID)
{
    UE_LOG(LogTemp, Warning, TEXT("CLIENT RPC: ClientShowUnitSelectionByPosition received"));

    // SprawdŸ czy ¿¹danie dotyczy tego gracza
    if (!UnitManagerRef || RequestingPlayerID != this->PlayerID)
        return;

    // ZnajdŸ jednostkê na podanej pozycji
    ABaseUnit* ClientUnit = UnitManagerRef->GetUnitAtPosition(UnitGridPosition);
    if (!ClientUnit)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot find unit at position (%f,%f)!"),
            UnitGridPosition.X, UnitGridPosition.Y);
        return;
    }

    // Ukryj poprzednie podœwietlenia i poka¿ nowe dla wybranej jednostki
    UnitManagerRef->HideAllHighlights();
    UnitManagerRef->ShowValidMovePositions(ClientUnit, RequestingPlayerID);
    UnitManagerRef->ShowSelectionHighlight(ClientUnit);
}
/// <summary>
/// Ukrywa wszystkie podœwietlenia jednostek po stronie klienta
/// </summary>
void AStrategyPlayerController::ClientHideUnitSelection_Implementation()
{
    // Ukryj wszystkie podœwietlenia na siatce
    if (UnitManagerRef)
    {
        UnitManagerRef->HideAllHighlights();
    }
}

/// <summary>
/// Aktualizuje wyœwietlan¹ iloœæ z³ota w interfejsie u¿ytkownika
/// </summary>
/// <param name="NewGold">Nowa wartoœæ z³ota do wyœwietlenia</param>
void AStrategyPlayerController::UpdateUIGold(int32 NewGold)
{
    // Aktualizuj wyœwietlanie z³ota w UI
    if (GameUIRef && IsLocalPlayerController())
    {
        GameUIRef->UpdateGold(NewGold);
    }
}

/// <summary>
/// Inicjalizuje gracza z przypisanym ID i startowym z³otem
/// </summary>
/// <param name="AssignedPlayerID">ID przypisane graczowi przez serwer</param>
/// <param name="StartingGold">Pocz¹tkowa iloœæ z³ota gracza</param>
void AStrategyPlayerController::ClientInitializePlayer_Implementation(int32 AssignedPlayerID, int32 StartingGold)
{
    // Serwer przypisa³ temu kontrolerowi ID gracza
    PlayerID = AssignedPlayerID;

    UE_LOG(LogTemp, Warning, TEXT("Player initialized as Player %d with %d gold"), PlayerID, StartingGold);

    // Zaktualizuj UI z nowymi danymi gracza
    if (GameUIRef)
    {
        GameUIRef->UpdateGold(StartingGold);
        GameUIRef->UpdatePlayerID(AssignedPlayerID);
    }
}

/// <summary>
/// Obs³uguje ¿¹danie zakupu jednostki z interfejsu u¿ytkownika
/// </summary>
/// <param name="UnitType">Typ jednostki do zakupu</param>
void AStrategyPlayerController::OnUnitPurchaseRequested(int32 UnitType)
{
    UE_LOG(LogTemp, Warning, TEXT("OnUnitPurchaseRequested: UnitType=%d, PlayerID=%d"), UnitType, PlayerID);
    UE_LOG(LogTemp, Warning, TEXT("IsLocalPlayerController: %s"), IsLocalPlayerController() ? TEXT("YES") : TEXT("NO"));

    // Tylko lokalny gracz z prawid³owym ID mo¿e kupowaæ jednostki
    if (!IsLocalPlayerController() || PlayerID == -1)
    {
        UE_LOG(LogTemp, Error, TEXT("REJECTED: Not local controller or invalid PlayerID"));
        return;
    }

    // WA¯NE: Nie sprawdzamy fazy tutaj
    UE_LOG(LogTemp, Warning, TEXT("Sending purchase request to server"));
    ServerRequestUnitPurchase(UnitType, PlayerID);
}

/// <summary>
/// Obs³uguje klikniêcie na siatkê po stronie serwera
/// </summary>
/// <param name="GridPosition">Pozycja na siatce która zosta³a klikniêta</param>
/// <param name="RequestingPlayerID">ID gracza wykonuj¹cego akcjê</param>
void AStrategyPlayerController::ServerHandleGridClick_Implementation(FVector2D GridPosition, int32 RequestingPlayerID)
{
    // Tylko serwer przetwarza logikê gry
    if (!HasAuthority() || !UnitManagerRef)
        return;

    // Przeka¿ klikniêcie do UnitManager, który obs³uguje logikê ruchu jednostek
    UnitManagerRef->HandleGridClick(GridPosition, RequestingPlayerID);
}

/// <summary>
/// Waliduje parametry RPC ServerHandleGridClick
/// </summary>
/// <param name="GridPosition">Pozycja na siatce do zwalidowania</param>
/// <param name="RequestingPlayerID">ID gracza do zwalidowania</param>
/// <returns>true jeœli parametry s¹ prawid³owe, false wpp</returns>
bool AStrategyPlayerController::ServerHandleGridClick_Validate(FVector2D GridPosition, int32 RequestingPlayerID)
{
    // Walidacja danych przed wykonaniem RPC
    return GridManagerRef && GridManagerRef->IsValidGridPosition(GridPosition) && RequestingPlayerID >= 0;
}

/// <summary>
/// Obs³uguje klikniêcie na jednostkê po stronie serwera
/// </summary>
/// <param name="ClickedUnit">WskaŸnik do klikniêtej jednostki</param>
/// <param name="RequestingPlayerID">ID gracza wykonuj¹cego akcjê</param>
void AStrategyPlayerController::ServerHandleUnitClick_Implementation(ABaseUnit* ClickedUnit, int32 RequestingPlayerID)
{
    // Tylko serwer przetwarza logikê gry
    if (!HasAuthority() || !UnitManagerRef || !ClickedUnit)
        return;

    // Przeka¿ klikniêcie jednostki do UnitManager
    UnitManagerRef->HandleUnitClick(ClickedUnit, RequestingPlayerID);
}

/// <summary>
/// Waliduje parametry RPC ServerHandleUnitClick
/// </summary>
/// <param name="ClickedUnit">WskaŸnik do jednostki do zwalidowania</param>
/// <param name="RequestingPlayerID">ID gracza do zwalidowania</param>
/// <returns>true jeœli parametry s¹ prawid³owe, false wpp</returns>
bool AStrategyPlayerController::ServerHandleUnitClick_Validate(ABaseUnit* ClickedUnit, int32 RequestingPlayerID)
{
    // Walidacja danych przed wykonaniem RPC
    return ClickedUnit != nullptr && RequestingPlayerID >= 0;
}

/// <summary>
/// Odznacza aktualnie wybran¹ jednostkê po stronie serwera
/// </summary>
void AStrategyPlayerController::ServerDeselectUnit_Implementation()
{
    // Tylko serwer przetwarza logikê gry
    if (!HasAuthority() || !UnitManagerRef)
        return;

    // Zapamiêtaj ID gracza przed odznaczeniem
    int32 PreviousPlayerID = UnitManagerRef->GetSelectedUnitPlayerID();
    UnitManagerRef->DeselectUnit();

    // Wyœlij do odpowiedniego klienta informacjê o ukryciu podœwietleñ
    if (PreviousPlayerID >= 0)
    {
        for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
        {
            if (AStrategyPlayerController* PC = Cast<AStrategyPlayerController>(Iterator->Get()))
            {
                if (PC->GetPlayerID() == PreviousPlayerID)
                {
                    PC->ClientHideUnitSelection();
                    break;
                }
            }
        }
    }
}

/// <summary>
/// Przetwarza ¿¹danie zakupu jednostki po stronie serwera
/// </summary>
/// <param name="UnitType">Typ jednostki do zakupu</param>
/// <param name="RequestingPlayerID">ID gracza ¿¹daj¹cego zakupu</param>
void AStrategyPlayerController::ServerRequestUnitPurchase_Implementation(int32 UnitType, int32 RequestingPlayerID)
{
    // Tylko serwer przetwarza zakupy
    if (!HasAuthority())
        return;

    // Upewnij siê ¿e mamy referencjê do GameMode
    if (!GameModeRef)
    {
        GameModeRef = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
        if (!GameModeRef)
        {
            UE_LOG(LogTemp, Error, TEXT("Cannot find GameMode!"));
            return;
        }
    }

    // Przeka¿ ¿¹danie zakupu do GameMode, który sprawdzi fazê gry i iloœæ z³ota
    UE_LOG(LogTemp, Warning, TEXT("Server: Processing unit purchase request"));
    GameModeRef->OnUnitPurchased(UnitType, RequestingPlayerID);
}

/// <summary>
/// Waliduje parametry RPC ServerRequestUnitPurchase
/// </summary>
/// <param name="UnitType">Typ jednostki do zwalidowania</param>
/// <param name="RequestingPlayerID">ID gracza do zwalidowania</param>
/// <returns>true jeœli parametry s¹ prawid³owe, false wpp</returns>
bool AStrategyPlayerController::ServerRequestUnitPurchase_Validate(int32 UnitType, int32 RequestingPlayerID)
{
    // Walidacja danych - typ jednostki musi byæ w zakresie 0-3, ID gracza >= 0
    return UnitType >= 0 && UnitType < 4 && RequestingPlayerID >= 0;
}


/// <summary>
/// Prze³¹cza widocznoœæ siatki gry
/// </summary>
void AStrategyPlayerController::ToggleGridVisibility()
{
    // Tylko lokalny gracz mo¿e prze³¹czaæ widocznoœæ siatki
    if (!IsLocalPlayerController() || !GridManagerRef)
        return;

    if (GridManagerRef->bShowGrid)
    {
        GridManagerRef->HideGrid();
        GridManagerRef->bShowGrid = false;
    }
    else
    {
        GridManagerRef->ShowGrid();
        GridManagerRef->bShowGrid = true;
    }

    UE_LOG(LogTemp, Log, TEXT("Grid visibility toggled: %s"),
        GridManagerRef->bShowGrid ? TEXT("ON") : TEXT("OFF"));
}

/// <summary>
/// Podœwietla wszystkie prawid³owe pozycje spawnu gracza
/// </summary>
void AStrategyPlayerController::ShowPlayerSpawnZones()
{
    // Tylko lokalny gracz mo¿e podejrzeæ swoje strefy spawnu
    if (!IsLocalPlayerController() || !GridManagerRef || !UnitManagerRef)
        return;

    // Pobierz wszystkie prawid³owe pozycje spawnu dla tego gracza
    TArray<FVector2D> SpawnPositions = GridManagerRef->GetValidSpawnPositions(PlayerID);

    // Wyczyœæ poprzednie podœwietlenia
    UnitManagerRef->HideAllHighlights();

    // Podœwietl wszystkie pozycje spawnu
    for (const FVector2D& Position : SpawnPositions)
    {
        FVector WorldLocation = GridManagerRef->GetWorldLocationFromGrid(Position);
        UnitManagerRef->HighlightGridPosition(Position, UnitManagerRef->ValidMoveHighlightMaterial);

        // Opcjonalnie narysuj debug box w edytorze
        if (bDrawDebugLines)
        {
            DrawDebugBox(GetWorld(), WorldLocation, FVector(50, 50, 10), FColor::Green, false, 5.0f);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Player %d spawn zone has %d positions"), PlayerID, SpawnPositions.Num());

    // Automatycznie ukryj podœwietlenia po 5 sekundach
    FTimerHandle HighlightTimer;
    GetWorld()->GetTimerManager().SetTimer(HighlightTimer, [this]()
        {
            if (UnitManagerRef)
            {
                UnitManagerRef->HideAllHighlights();
            }
        }, 5.0f, false);
}

/// <summary>
/// Konfiguruje w³aœciwoœci replikowane przez sieæ
/// </summary>
/// <param name="OutLifetimeProps">Tablica w³aœciwoœci do replikacji</param>
void AStrategyPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // PlayerID jest replikowane do wszystkich klientów
    DOREPLIFETIME(AStrategyPlayerController, PlayerID);
}


/// <summary>
/// Konfiguruje linie
/// </summary>
bool AStrategyPlayerController::PerformLineTrace(FHitResult& OutHit) const
{
    FVector MouseWorldLocation, MouseWorldDirection;
    if (DeprojectMousePositionToWorld(MouseWorldLocation, MouseWorldDirection))
    {
        FVector StartLocation = MouseWorldLocation;
        FVector EndLocation = StartLocation + (MouseWorldDirection * ClickRange);

        return GetWorld()->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility);
    }

    return false;
}

/// <summary>
/// Wykonuje raycast z pozycji myszy w œwiat gry
/// </summary>
/// <returns>true jeœli raycast trafi³ w jakiœ obiekt, false wpp</returns>
FVector AStrategyPlayerController::GetMouseWorldPosition() const
{
    // Zwróæ pozycjê 3D w œwiecie gry odpowiadaj¹c¹ pozycji kursora myszy
    FHitResult HitResult;
    if (PerformLineTrace(HitResult))
    {
        return HitResult.Location;
    }

    return FVector::ZeroVector;
}