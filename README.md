# 🎰 Małe Las Vegas - Jednoręki Bandyta na STM32

Projekt gry hazardowej typu "Slot Machine" zrealizowany na mikrokontrolerze **STM32L476RG**. Gra wykorzystuje wyświetlacz OLED, sterowanie przyciskiem oraz interfejs UART do zarządzania menu.

## 🚀 Funkcjonalności

* **Animacja Bębnów:** Płynna, losowa animacja 3 bębnów z symbolami (777, Owoce, Bar lub Diament).
* **System Kredytów:** Obliczanie wygranych na podstawie tabeli wypłat (Jackpot, Pary).
* **Menu Systemowe:** Nawigacja po ekranach (Gra, Autorzy, Opis, Wyniki).
* **Zdalne Sterowanie:** Możliwość sterowania grą i menu poprzez terminal UART.
* **Prawdziwa Losowość:** Generator RNG inicjowany czasem reakcji gracza.

## 🛠️ Sprzęt (Hardware)

* **Mikrokontroler:** STM32L476RG (Nucleo-64)
* **Wyświetlacz:** OLED 2.42" SSD1309 (Interfejs **SPI**)
* **Ekspander GPIO:** SX1509 (Interfejs I2C)
* **Sterowanie:**
    * Przycisk User Button (B1) - Start gry
    * UART (USB) - Nawigacja w menu

## 🔌 Podłączenie (Pinout)

### Wyświetlacz SSD1306 (SPI1)
| Pin OLED | Pin STM32 | Funkcja |
| :--- | :--- | :--- |
| **GND** | GND | Masa |
| **VCC** | 3.3V | Zasilanie |
| **D0 (SCK)** | PA5 | SPI1 Clock |
| **D1 (SDA)** | PA7 | SPI1 SDA |
| **RES** | PA9 | Reset |
| **DC** | PC7 | Data/Command |
| **CS** | PB6 | Chip Select |

### Pozostałe
* **Przycisk B1:** PC13 (Przerwanie EXTI)
* **UART Console:** PA2 (TX), PA3 (RX) - Baudrate 115200

## 🎮 Sterowanie (UART Terminal)

Podłącz płytkę do komputera i otwórz terminal (np. Putty/RealTerm) na porcie COM ST-Linka (115200 bps).

* `a` - Góra (Menu)
* `d` - Dół (Menu)
* `e` - Wybierz / Zatwierdź
* `q` - Powrót do menu głównego

## 🏗️ Struktura Projektu

* `Core/Src/main.c` - Główna logika gry (Maszyna Stanów).
* `Core/Src/ssd1306.c` - Biblioteka obsługi wyświetlacza OLED.
* `Core/Src/symbols.c` - Bitmapy symboli (Wisienki, Dzwonki itp.).

## 👥 Autorzy

* **Kuba** - Logika gry, silnik graficzny, sterowanie.
* **Mateusz** - Obsługa pamięci, moduł komunikacji.
