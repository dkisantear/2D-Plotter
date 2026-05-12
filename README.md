# STM32 2D Plotter Final Project

This repo is our final project for a small 2D plotter built around an STM32 board, stepper drivers, an Ender 3 frame, limit switches, an I2C LCD, a rotary encoder, and a set of hardcoded drawing functions.

## Repo layout

- `FinalProject/`
  - `Core/Src/main.c` - main firmware for the plotter
  - `Core/Src/i2c_lcd.c` - LCD helper source
  - `Core/Inc/i2c_lcd.h` - LCD helper header
  - CubeIDE project files and generated STM32 setup
- `plotter_image_converter/`
  - `src/image_to_draw_vectors.py` - image to plotter command converter
  - `src/function_to_draw_vectors.py` - math function to plotter command converter
  - `input/` - test images
  - `output/` - generated command text files
- `docs/project_photo.png` - current photo of the plotter setup

## Hardware summary

- STM32F303K8 based controller
- Stepper drivers for X, Y, and Z
- Ender 3 frame used as the plotter base
- Limit switches for homing
- I2C LCD for the menu
- Rotary encoder and push button for input
- Pen lift on the Y axis

## Firmware notes

The firmware keeps the same motion behavior that was used on the machine:

- pin mappings stay the same
- stepper movement logic stays the same
- LCD menu flow stays the same
- encoder behavior stays the same
- homing and centering behavior stay the same

The cleanup in this version was focused on comments, section labels, and general readability for submission.

## Python helper tools

From `plotter_image_converter/`:

```bash
python src/image_to_draw_vectors.py input/snoopy.png
python src/function_to_draw_vectors.py "sin(x)" --xmin -3.14159 --xmax 3.14159 --samples 200 --width-mm 40 --height-mm 40
```

The old script names `convert.py` and `graph_function.py` are still there as compatibility wrappers.

## Build / open in STM32CubeIDE

Open the `FinalProject/` folder as the STM32CubeIDE project, then build and flash from there.

## Submission files

Included now:

- cleaned `main.c`
- `i2c_lcd.c` and `i2c_lcd.h`
- repo README
- project photo
- Python helper scripts for image and graph conversion

Still to add if needed before final submission:

- wiring diagram or a clearer wiring photo
- short demo video or GIF
- proposal PDF/doc
- final slides
