# NTRIP Helper - Node.js Tool

Prosty skrypt Node.js do testowania połączeń z serwerami NTRIP Caster i logowania wiadomości RTCM.

## Funkcje

- Nawiązuje połączenie z wybranym NTRIP Casterem
- Wysyła ramki GGA (symulowane współrzędne GPS)
- Parsuje i loguje wiadomości RTCM3
- Wyświetla statystyki w czasie rzeczywistym
- Zapisuje logi do pliku z timestamp

## Instalacja

```bash
cd tools
npm install  # (opcjonalne, skrypt używa tylko wbudowanych modułów Node.js)
```

## Użycie

### Podstawowe użycie:

```bash
node ntrip_helper.js [host] [port] [mountpoint] [username] [password]
```

### Przykłady:

1. **Połączenie z RTK2GO (publiczny caster):**

```bash
node ntrip_helper.js rtk2go.com 2101 WROC
```

2. **Połączenie z uwierzytelnianiem:**

```bash
node ntrip_helper.js your-caster.com 2101 STATION user password
```

3. **Test z domyślnymi parametrami:**

```bash
npm run test
```

### Parametry:

- `host` - adres serwera NTRIP (domyślnie: rtk2go.com)
- `port` - port serwera (domyślnie: 2101)
- `mountpoint` - nazwa mountpoint
- `username` - nazwa użytkownika (opcjonalne)
- `password` - hasło (opcjonalne)

## Wyjście

Skrypt wyświetla:

- Status połączenia
- Wysyłane ramki GGA
- Informacje o otrzymanych wiadomościach RTCM
- Statystyki co minutę

### Przykład wyjścia:

```
NTRIP Helper - Node.js RTCM Logger
Connecting to: rtk2go.com:2101/WROC

Connected to NTRIP Caster: rtk2go.com:2101
Successfully authenticated with NTRIP Caster
Sending GGA: $GPGGA,123045,5213.0000,N,02100.0000,E,1,08,1.2,100.0,M,0.0,M,,*6E

2025-10-14T12:30:45.123Z - RTCM Type: 1005, Length: 19 bytes
2025-10-14T12:30:45.200Z - RTCM Type: 1077, Length: 156 bytes
2025-10-14T12:30:46.150Z - RTCM Type: 1087, Length: 138 bytes
```

## Pliki logów

Logi są zapisywane do pliku `ntrip_log_YYYY-MM-DDTHH-MM-SS.txt` w katalogu tools.

## Statystyki

Co minutę wyświetlane są statystyki:

- Czas połączenia
- Łączna liczba wiadomości
- Wiadomości na sekundę
- Rozkład typów wiadomości RTCM

## Zamykanie

Naciśnij `Ctrl+C` aby bezpiecznie zamknąć połączenie i wyświetlić końcowe statystyki.

## Popularne NTRIP Castery

- **RTK2GO** (publiczny): rtk2go.com:2101
- **EUREF**: www.euref-ip.net:2101
- **IGS**: products.igs-ip.net:2101

## Troubleshooting

1. **Connection refused**: Sprawdź adres i port serwera
2. **Authentication failed**: Sprawdź nazwę użytkownika i hasło
3. **Mountpoint not found**: Sprawdź dostępne mountpointy na serwerze
4. **No RTCM data**: Niektóre mountpointy wymagają poprawnych współrzędnych GGA
