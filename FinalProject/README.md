# Final Project Firmware Source

This folder only keeps the firmware source files we actually want on GitHub.

The full STM32CubeIDE project in the local workspace has an `.ioc` setup that gets rewritten a lot, so for the repo we are only keeping the final source files:

- `src/main.c`
- `src/i2c_lcd.c`
- `src/i2c_lcd.h`

This is the code used for the STM32-based 2D plotter with:

- stepper drivers
- Ender 3 frame
- limit switches
- LCD menu
- rotary encoder
- hardcoded drawing functions
