# Image To Draw Vectors

This tool converts a source image into plotter commands for the STM32 2D plotter.

## Files

- `src/image_to_draw_vectors.py` - main script
- `input/` - sample input images
- `output/` - generated sample command files
- `requirements.txt` - Python dependencies

## Install

```bash
python -m pip install -r requirements.txt
```

## Run

```bash
python src/image_to_draw_vectors.py input/snoopy.png
```

Or run it with no argument and pick a file from `input/`.
