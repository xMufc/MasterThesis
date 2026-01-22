// GridManager.cpp - Zarz¹dzanie siatk¹ gry i strefami spawnu
#include "GridManager.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"  
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/EngineTypes.h"

/// <summary>
/// Konstruktor- Inicjalizacja komponentów siatki
/// </summary>
AGridManager::AGridManager()
{
    PrimaryActorTick.bCanEverTick = false;

    // 1) Utworzenie pustego komponentu sceny jako root
    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // 2) Utworzenie box component jako dziecko root 
    GridCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("GridCollision"));
    GridCollisionBox->SetupAttachment(RootComponent);

    // Ustawienie kolizji na WorldStatic 
    GridCollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
    GridCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GridCollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
}

/// <summary>
/// Inicjalizacja przy starcie gry
/// </summary>
void AGridManager::BeginPlay()
{
    Super::BeginPlay();

    // Pobierz punkt pocz¹tkowy siatki z lokalizacji aktora
    GridOrigin = GetActorLocation();

    // Konfiguracja collision box przed innymi inicjalizacjami
    SetupCollisionBox();

    // Inicjalizacja stref spawnu dla obu graczy
    InitializeSpawnZones();

    // Utworzenie granic mapy jeœli w³¹czone w ustawieniach
    if (bCreateMapBoundaries)
    {
        CreateMapBoundaries();
    }

    // Wygenerowanie wizualizacji siatki jeœli w³¹czone
    if (bShowGrid)
    {
        GenerateGridVisuals();
    }
}

/// <summary>
/// Konfiguruje collision box dla pod³ogi siatki
/// Box jest automatycznie dopasowywany do wymiarów siatki
/// </summary>
void AGridManager::SetupCollisionBox()
{
    if (!GridCollisionBox)
        return;

    // Obliczenie rozmiaru box'a na podstawie wymiarów siatki
    FVector BoxExtent(
        (GridWidth * CellSize) * 0.5f,   // Szerokoœæ siatki / 2
        (GridHeight * CellSize) * 0.5f,  // Wysokoœæ siatki / 2
        FloorThickness * 0.5f            // Gruboœæ pod³ogi / 2
    );
    GridCollisionBox->SetBoxExtent(BoxExtent);

    // Przesuniêcie box'a ¿eby by³ wyœrodkowany pod siatk¹
    FVector BoxCenter(
        (GridWidth * CellSize) * 0.5f,            // Œrodek siatki X
        (GridHeight * CellSize) * 0.5f,           // Œrodek siatki Y
        -FloorDepth - (FloorThickness * 0.5f)     // Pod siatk¹
    );
    GridCollisionBox->SetRelativeLocation(BoxCenter);

    UE_LOG(LogTemp, Warning, TEXT("Collision box skonfigurowany: Rozmiar(%f, %f, %f), Œrodek(%f, %f, %f)"),
        BoxExtent.X, BoxExtent.Y, BoxExtent.Z,
        BoxCenter.X, BoxCenter.Y, BoxCenter.Z);
}

/// <summary>
/// Aktualizacja co klatkê
/// </summary>
/// <param name="DeltaTime">Czas pomiedzy aktualizacjami</param>
void AGridManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

/// <summary>
/// Inicjalizuje strefy spawnu dla obu graczy
/// </summary>
void AGridManager::InitializeSpawnZones()
{
    // Wyczyœæ istniej¹ce strefy
    Player1SpawnZone.Empty();
    Player2SpawnZone.Empty();

    // Zabezpieczenie przed b³êdnymi wartoœciami
    int32 SafePlayer1Size = FMath::Clamp(Player1SpawnSize, 1, GridHeight);
    int32 SafePlayer2Size = FMath::Clamp(Player2SpawnSize, 1, GridHeight);

    // Gracz 1 - dolne rzêdy
    for (int32 x = 0; x < GridWidth; x++)
    {
        for (int32 y = 0; y < SafePlayer1Size; y++)
        {
            Player1SpawnZone.Add(FVector2D(x, y));
        }
    }

    // Gracz 2 - górne rzêdy 
    int32 Player2StartY = FMath::Max(SafePlayer1Size, GridHeight - SafePlayer2Size);
    for (int32 x = 0; x < GridWidth; x++)
    {
        for (int32 y = Player2StartY; y < GridHeight; y++)
        {
            FVector2D Position(x, y);

            // SprawdŸ czy nie nak³ada siê ze stref¹ Gracza 1
            if (!Player1SpawnZone.Contains(Position))
            {
                Player2SpawnZone.Add(Position);
            }
        }
    }

    // Log diagnostyczny
    UE_LOG(LogTemp, Warning, TEXT("Strefy spawnu zainicjalizowane - Gracz1: %d komórek, Gracz2: %d komórek"),
        Player1SpawnZone.Num(), Player2SpawnZone.Num());
}

/// <summary>
/// Rêczna regeneracja stref spawnu
/// </summary>
void AGridManager::RegenerateSpawnZones()
{
    // Ponowna inicjalizacja stref
    InitializeSpawnZones();

    // Regeneracja collision box jeœli zmieni³y siê wymiary
    SetupCollisionBox();

    // Regeneracja granic mapy jeœli w³¹czone
    if (bCreateMapBoundaries)
    {
        ClearMapBoundaries();
        CreateMapBoundaries();
    }

    // Regeneracja wizualizacji siatki jeœli ju¿ istnieje
    if (bGridGenerated && bShowGrid)
    {
        GenerateGridVisuals();
    }

    UE_LOG(LogTemp, Warning, TEXT("Strefy spawnu zregenerowane rêcznie"));
}

#if WITH_EDITOR

/// <summary>
/// Wywo³ywane gdy w³aœciwoœæ zostanie zmieniona w edytorze
/// Automatycznie regeneruje strefy spawnu i kolizje
/// </summary>
/// <param name="PropertyChangedEvent">Zmieniona wlasciwosc</param>
void AGridManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != NULL) ?
        PropertyChangedEvent.Property->GetFName() : NAME_None;

    // SprawdŸ czy zmieniono wymiary siatki, rozmiary stref spawnu lub ustawienia kolizji
    if (PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, GridWidth) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, GridHeight) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, Player1SpawnSize) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, Player2SpawnSize) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, CellSize) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, FloorThickness) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, FloorDepth))
    {
        // Automatyczna regeneracja stref spawnu i kolizji przy zmianie tych w³aœciwoœci
        InitializeSpawnZones();
        SetupCollisionBox();

        // Regeneracja granic mapy jeœli potrzeba
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, bCreateMapBoundaries) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, GridWidth) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, GridHeight) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, CellSize) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, BoundaryWallHeight) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, BoundaryWallThickness) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AGridManager, BoundaryOffset))
        {
            ClearMapBoundaries();
            if (bCreateMapBoundaries)
            {
                CreateMapBoundaries();
            }
        }

        UE_LOG(LogTemp, Log, TEXT("Auto-regeneracja stref spawnu i kolizji z powodu zmiany w³aœciwoœci: %s"),
            *PropertyName.ToString());
    }
}
#endif

/// <summary>
/// Konwertuje pozycjê siatki 2D na pozycjê w œwiecie 3D
/// </summary>
/// <param name="GridPosition">pozycja na siatce (x, y)</param>
/// <returns>pozycja w przestrzeni œwiata 3D</returns>
FVector AGridManager::GetWorldLocationFromGrid(FVector2D GridPosition) const
{
    return GridOrigin + FVector(
        GridPosition.X * CellSize,
        GridPosition.Y * CellSize,
        0.0f
    );
}

/// <summary>
/// Konwertuje pozycjê œwiata 3D na pozycjê siatki 2D
/// </summary>
/// <param name="WorldLocation">pozycja w przestrzeni œwiata</param>
/// <returns>zaokr¹glona pozycja na siatce (x, y)</returns>
FVector2D AGridManager::GetGridPositionFromWorld(FVector WorldLocation) const
{
    FVector RelativeLocation = WorldLocation - GridOrigin;
    return FVector2D(
        FMath::RoundToInt(RelativeLocation.X / CellSize),
        FMath::RoundToInt(RelativeLocation.Y / CellSize)
    );
}

/// <summary>
/// Sprawdza czy pozycja siatki jest prawid³owa (w granicach)
/// </summary>
/// <param name="GridPosition">pozycja do sprawdzenia</param>
/// <returns>true jeœli pozycja jest w granicach siatki, false - wpp</returns>
bool AGridManager::IsValidGridPosition(FVector2D GridPosition) const
{
    return GridPosition.X >= 0 && GridPosition.X < GridWidth &&
        GridPosition.Y >= 0 && GridPosition.Y < GridHeight;
}


/// <summary>
/// Sprawdza czy pozycja znajduje siê w strefie spawnu Gracza 1
/// </summary>
/// <param name="GridPosition">pozycja do sprawdzenia</param>
/// <returns>true jeœli pozycja jest w strefie Gracza 1, false - wpp</returns>
bool AGridManager::IsPositionInPlayer1SpawnZone(FVector2D GridPosition) const
{
    return Player1SpawnZone.Contains(GridPosition);
}

/// <summary>
/// Sprawdza czy pozycja znajduje siê w strefie spawnu Gracza 2
/// </summary>
/// <param name="GridPosition">pozycja do sprawdzenia</param>
/// <returns>true jeœli pozycja jest w strefie Gracza 2, false - wpp</returns>
bool AGridManager::IsPositionInPlayer2SpawnZone(FVector2D GridPosition) const
{
    return Player2SpawnZone.Contains(GridPosition);
}

/// <summary>
/// Pobiera wszystkie dostêpne pozycje spawnu dla gracza
/// </summary>
/// <param name="PlayerID">ID gracza</param>
/// <returns>tablica pozycji siatki dostêpnych do spawnu</returns>
TArray<FVector2D> AGridManager::GetValidSpawnPositions(int32 PlayerID) const
{
    if (PlayerID == 0)
        return Player1SpawnZone;
    else if (PlayerID == 1)
        return Player2SpawnZone;
    return TArray<FVector2D>();
}


/// <summary>
/// Generuje wizualizacjê siatki w grze
/// </summary>
void AGridManager::GenerateGridVisuals()
{
    // Jeœli siatka ju¿ istnieje, wyczyœæ j¹ najpierw
    if (bGridGenerated)
    {
        ClearGridVisuals();
    }

    // Utwórz linie siatki
    CreateGridLines();
    bGridGenerated = true;
}

/// <summary>
/// Czyœci wszystkie wizualizacje siatki
/// </summary>
void AGridManager::ClearGridVisuals()
{
    // Zniszcz wszystkie komponenty linii
    for (UStaticMeshComponent* Component : GridLineComponents)
    {
        if (Component)
        {
            Component->DestroyComponent();
        }
    }
    GridLineComponents.Empty();
    bGridGenerated = false;
}

/// <summary>
/// Pokazuje wizualizacjê siatki
/// </summary>
void AGridManager::ShowGrid()
{
    for (UStaticMeshComponent* Component : GridLineComponents)
    {
        if (Component)
        {
            Component->SetVisibility(true);
        }
    }
}

/// <summary>
/// Ukrywa wizualizacjê siatki
/// </summary>
void AGridManager::HideGrid()
{
    for (UStaticMeshComponent* Component : GridLineComponents)
    {
        if (Component)
        {
            Component->SetVisibility(false);
        }
    }
}

/// <summary>
/// Dodaje komponent linii do listy zarz¹dzanych komponentów
/// </summary>
/// <param name="Component">komponent do dodania</param>
void AGridManager::AddGridLineComponent(UStaticMeshComponent* Component)
{
    if (Component)
    {
        GridLineComponents.Add(Component);
    }
}

/// <summary>
/// Tworzy linie siatki w Blueprint
/// </summary>
void AGridManager::CreateGridLines()
{
}

/// <summary>
/// Podœwietla okreœlon¹ komórkê siatki w Blueprint
/// </summary>
/// <param name="GridPosition">pozycja komórki do podœwietlenia</param>
/// <param name="HighlightColor">kolor podœwietlenia</param>
void AGridManager::HighlightCell(FVector2D GridPosition, FLinearColor HighlightColor)
{
}

/// <summary>
/// Tworzy niewidzialne œciany wokó³ siatki
/// </summary>
void AGridManager::CreateMapBoundaries()
{
    // Wyczyœæ istniej¹ce granice przed stworzeniem nowych
    ClearMapBoundaries();

    // Oblicz wymiary siatki w jednostkach œwiata
    float GridWorldWidth = GridWidth * CellSize;
    float GridWorldHeight = GridHeight * CellSize;

    // Oblicz pozycje œcian
    float WallHalfThickness = BoundaryWallThickness * 0.5f;
    float WallHalfHeight = BoundaryWallHeight * 0.5f;

    // Pozycje œrodków œcian
    float LeftWallX = -BoundaryOffset - WallHalfThickness;
    float RightWallX = GridWorldWidth + BoundaryOffset + WallHalfThickness;
    float BottomWallY = -BoundaryOffset - WallHalfThickness;
    float TopWallY = GridWorldHeight + BoundaryOffset + WallHalfThickness;

    float CenterX = GridWorldWidth * 0.5f;
    float CenterY = GridWorldHeight * 0.5f;

    // Lewa œciana
    CreateBoundaryWall(
        FVector(LeftWallX, CenterY, WallHalfHeight - 100.f),
        FVector(WallHalfThickness, (GridWorldHeight + 2 * BoundaryOffset) * 0.5f, WallHalfHeight),
        TEXT("LeftWall")
    );

    // Prawa œciana
    CreateBoundaryWall(
        FVector(RightWallX, CenterY, WallHalfHeight - 100.f),
        FVector(WallHalfThickness, (GridWorldHeight + 2 * BoundaryOffset) * 0.5f, WallHalfHeight),
        TEXT("RightWall")
    );

    // Dolna œciana
    CreateBoundaryWall(
        FVector(CenterX, BottomWallY, WallHalfHeight - 100.f),
        FVector((GridWorldWidth + 2 * BoundaryOffset + 2 * BoundaryWallThickness) * 0.5f, WallHalfThickness, WallHalfHeight),
        TEXT("BottomWall")
    );

    // Górna œciana
    CreateBoundaryWall(
        FVector(CenterX, TopWallY, WallHalfHeight - 100.f),
        FVector((GridWorldWidth + 2 * BoundaryOffset + 2 * BoundaryWallThickness) * 0.5f, WallHalfThickness, WallHalfHeight),
        TEXT("TopWall")
    );

    UE_LOG(LogTemp, Warning, TEXT("Utworzono %d œcian granicznych"), BoundaryWalls.Num());
}

/// <summary>
/// Tworzy pojedyncz¹ œcianê graniczn¹
/// </summary>
/// <param name="Location">pozycja œrodka œciany</param>
/// <param name="Extent">wymiary box component</param>
/// <param name="Name">nazwa œciany</param>
void AGridManager::CreateBoundaryWall(FVector Location, FVector Extent, FString Name)
{
    // Utworzenie nowego box component dla œciany
    UBoxComponent* WallComponent = NewObject<UBoxComponent>(this, UBoxComponent::StaticClass(), *Name);
    if (!WallComponent)
        return;

    // Do³¹czenie do root component
    WallComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
    WallComponent->RegisterComponent();
    WallComponent->SetRelativeLocation(Location);
    WallComponent->SetBoxExtent(Extent);

    // Konfiguracja kolizji
    WallComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    WallComponent->SetCollisionObjectType(ECC_GameTraceChannel2); // MapBoundaries

    // Najpierw ignoruj wszystkie kana³y
    WallComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

    // Ustaw konkretne odpowiedzi kolizji
    WallComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);      // Niewidzialne dla raycastów kamery
    WallComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);           // Blokuje kamerê
    WallComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore); // Ignoruj custom channel 1
    WallComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);      // Blokuje statyczne obiekty
    WallComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);     // Blokuje dynamiczne obiekty
    WallComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);             // Blokuje pawny
    WallComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);      // Blokuje fizyczne obiekty
    WallComponent->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);          // Blokuje pojazdy
    WallComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);     // Blokuje zniszczalne
    WallComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); // Blokuje inne granice

    // Dodaj do listy zarz¹dzanych œcian
    BoundaryWalls.Add(WallComponent);

    UE_LOG(LogTemp, Log, TEXT("Utworzono œcianê graniczn¹: %s na pozycji (%f, %f, %f) z rozmiarem (%f, %f, %f)"),
        *Name, Location.X, Location.Y, Location.Z, Extent.X, Extent.Y, Extent.Z);
}

/// <summary>
/// Usuwa wszystkie œciany graniczne
/// </summary>
void AGridManager::ClearMapBoundaries()
{
    for (UBoxComponent* Wall : BoundaryWalls)
    {
        if (Wall)
        {
            Wall->DestroyComponent();
        }
    }
    BoundaryWalls.Empty();
}

/// <summary>
/// Regeneruje œciany graniczne mapy
/// </summary>
void AGridManager::RegenerateMapBoundaries()
{
    ClearMapBoundaries();
    if (bCreateMapBoundaries)
    {
        CreateMapBoundaries();
    }
    UE_LOG(LogTemp, Warning, TEXT("Granice mapy zregenerowane rêcznie"));
}