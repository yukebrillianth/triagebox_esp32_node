
## 2026-07-28T20:38:23Z
- main/CMakeLists.txt SRCS now includes ui_types.c + ui_status.c for ESP-IDF link parity with sim.
- idf.py build exit 0 after adding both sources.

## F3 final-qa 2026-07-29T00:31Z
- SIM_AUTO_WALK=1 from repo root; UI_MEASURE_MS=2000; mock RFID 3021
- SDL 1-4 = bar 0-3; p/c cycle priority; exit 0 + ALL_SCREENS_OK required
- Edge host selftest complements AUTO_WALK (abort/back/empty/rapid)
