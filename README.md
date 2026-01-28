## Aplikacja

Trzywymiarowa sieciowa gra strategiczna o podgatunku auto battler. Sama gra została zaprojektowana w najpopularniejszym silniku graficznym, czyli Unreal Engine 5. Strategiczna gra typu auto battler charakteryzuje się rywalizacją wielu graczy w podzielonej na fazy rozgrywce. Na początku oczekują na dołączenie wymaganej liczby graczy, następnie w fazie przygotowawczej umieszczają swoją kompozycję jednostek na polu bitwy przypominającym szachownicę. Kolejno występuje faza bitwy, w której bez udziału graczy porozstawiani bohaterowie walczą między sobą. Po zakończeniu aktualnej fazy pojawia się wynik danej rundy, a gracze ponownie przechodzą do fazy przygotowawczej. Wygrywa gracz, który jako ostatni zostanie przy życiu, czyli jego punkty życia nie spadną do zera.

Opis poszczególnych funkcjonalności:

1. ### 'BaseUnit'
	Jednostki w grze posiadają podstawowe statystyki takie jak: maksymalne zdrowie, siła ataku, pancerz, prędkość poruszania się czy też zasięg ataku. Każda z jednostek posiada swoje animacje, również dołączone do paczki z assetami o nazwie „RPGHeroSquad”. Każda z postaci posiada nad sobą, swój prywatny pasek zdrowia. 
2. ### 'GameUI'
  Podczas rozgrywki gracz posiada do dyspozycji interfejs użytkownika, który zawiera takie elementy jak:
    - na samej górze występuje sekcja z nazwą aktualnej fazy (oczekiwania na graczy, zakupów i rozmieszania jednostek, walki, wyników po każdej rundzie orazpodsumowania rozgrywki), a także zegarem, przedstawiającym czas który pozostał do zakończeniu aktualnej fazy. 
    - w prawym górnym rogu, ukazuje się aktualna ilość posiadanego złota przez danego klienta,
    - po lewej stronie znajduje się segment z informacjami o użytkownikach.
    - w lewym dolnym rogu znajduje się chat, na którym gracze mogą komunikować się między sobą.
    - na dole po prawej stronie znajduję się sklep do zakupu jednostek, który jest aktywny tylko w fazie kupowania i rozmieszania jednostek.  
	
3. ### 'GridManager'
	Rozgrywka toczy się na polu bitwy przypominającym szachownicę. Na planszy wyróżnia się trzy podstawowe strefy: strefę spawn pierwszego gracza, strefę neutralną oraz strefę spawnu drugiego gracza. Każda ze stref posiada swoją indywidualną teksturę: pierwsza strefa teksturę w kolorze niebieskim, druga przypomina trawę, a trzecia teksturę w kolorze czerwonym.
4. ### 'HealthBarWidget'
	Pasek zdrowia wyglądem przypomina prostokąt zawierający w swoim centrum informacje o aktualnych oraz maksymalnych punktach zdrowia. Na początku do jego wartości przypisywana jest maksymalna ilość zdrowia ustawionego dla danego typu jednostki. Podczas rozgrywki aktualizowany jest w momencie otrzymywania od innej jednostki. Po za tym sam widżet paska zdrowia podąża zawsze za kamerą. 
5. ### 'SpatialGrid'
	Przechowuje informacje o pozycji jednostek na partycji danych, aby umożliwić optymalizację.
6. ### 'StrategyGameMode'
  Główny komponent odpowiedzialny za obsługiwanie całej logiki występującej w grze. To do niego końcowo trafia wiele zapytań od klientów, obsługuje głównie zadania serwera. Między innymi zajmuje się zarządzaniem systemem faz gry, które występują w grze. Po uruchomieniu oczekuje na połączenie dwóch graczy po czym rozpoczyna rozgrywkę rozsyłając informacje o zmianie fazy do wszystkich klientów. Identycznie to wygląda w sytuacji fazy przygotowawczej i bitwy, w których pilnuje czasu oraz replikuje dla klientów regulatory czasu do zakończenia danej fazy.
7. ### 'StrategyPlayerController'
  Jeden z podstawowych elementów znajdujących się w każdej grze komputerowej, umożliwia przechwytywanie działań użytkownika np. kliknięcie myszką i wywołuje zdarzenia do nich przypisane. W stworzonej strategicznej grze przechwytuje takie zdarzenia jak poruszanie kamerą, przyspieszenie prędkości poruszania się, przybliżanie i oddalanie kamery, zmiana rotacji kamery oraz resetowanie pozycji i rotacji kamery
8. ### 'UnitManager'
  Zaczynając od pierwszej fazy, komponent musi przetwarzać otrzymywane od serwera polecenia dotyczące potrzeby zrespienia jednostki w danym miejscu i przechowywaniu o niej informacji. Kolejnym zadaniem jest obsługa wybrania jednostki przez gracza i wyświetlenie podświetlonych kafelków w obrębie swojego spawn’a.Podczas fazy bitwy załącza dla wszystkich jednostek automatyczną walkę z włączonym replikowaniem bez oczekiwania na potwierdzenie otrzymanych danych. Gdy jednostka zginie podczas pojedynku również zajmuje się zaktualizowaniem listy wciąż żywych bohaterów na areniezmagań.
## Instalacja i uruchomienie

Najlepszym sposobem do przetestowania gry jest pobranie silnika Unreal Engine 5 w wersji
5.5.4 oraz Visual Studio 2022. Visual Studio musi zostać zainstalowane z komponentami:
  * Desktop development with C++,
  * MSVC v143 - VS 2022 C++ x64/x86 build tools,
  * Windows 10/11 SDK,
  * C++ standard library modules
  * C++ CMake tools for Windows.

Następnie należy:
  1. Uruchomić launcher Unreal Engine.
  2. W menu początkowym należy wybrać w prawym dolnym rogu opcję wyszukiwania projektów, a następnie odnaleźć MagisterkaBKonkel oraz uruchomić grę przyciskiem znajdującym się w tym samym miejscu.
  3. Po załadowaniu projektu należy upewnić się czy w opcjach wieloosobowych liczba graczy wynosi dwa. Opcje te dostępne są w górnej części edytora, po rozwinięciu menu ukrytego pod trzema kropkami obok przycisku uruchamiania gry.
  4. Po sprawdzeniu i ewentualnej zmianie liczby graczy, należy wcisnąć zieloną strzałkę w górnej części okienka uruchamiająca rozgrywkę.
  5. Po zbudowaniu i uruchomieniu dwóch okienek można przetestować grę.
