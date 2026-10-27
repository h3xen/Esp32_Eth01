# Zasady projektu WT32-ETH01

- **Przy każdej zmianie firmware i każdym buildzie podbij wersję** `FW_VERSION` w [src/main.cpp](src/main.cpp)
  (np. 1.2.0 → 1.2.1 przy poprawce, 1.3.0 przy nowej funkcji) i zaktualizuj wartość `FW_VERSION`
  w tabeli ustawień w [README.md](README.md). Dzięki temu na stronie widać, która wersja jest wgrana.
- Nowe ustalenia z użytkownikiem dopisuj do [README.md](README.md).
- Build: `~/.platformio/penv/Scripts/pio.exe run`, wgrywanie: `.\flash.ps1` (COM3) lub OTA ze strony.
