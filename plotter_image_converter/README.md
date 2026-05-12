# Plotter Command Tools

This folder has two helper scripts for the 2D plotter:

- `src/image_to_draw_vectors.py` turns an image into plotter commands
- `src/function_to_draw_vectors.py` samples a math function and turns it into plotter commands

The generated `.txt` files use `move_x_mm()`, `move_z_mm()`, `move_y_mm()`, and `draw_vector(distance, angle_in_degrees)`.

## Folder structure

```
plotter_image_converter/
├── input/          # place source images here
├── output/         # generated command .txt files go here
└── src/
    ├── image_to_draw_vectors.py
    ├── function_to_draw_vectors.py
    ├── convert.py          # compatibility wrapper for the old name
    └── graph_function.py   # compatibility wrapper for the old name
```

## Install

From the `plotter_image_converter/` folder:

```bash
python -m pip install -r requirements.txt
```

## Run

### Convert a specific image

```bash
python src/image_to_draw_vectors.py input/snoopy.png
```

This writes `output/snoopy.txt`.

### Pick an image interactively

```bash
python src/image_to_draw_vectors.py
```

If you run without arguments, the script will list files in `input/` and prompt you to choose one.

### Plot a math function

```bash
python src/function_to_draw_vectors.py "sin(x)" --xmin -3.14159 --xmax 3.14159 --samples 200 --width-mm 40 --height-mm 40
```

This writes a file like `output/graph_sin_x_.txt`.

## Notes

- Coordinate system assumption:
  - X and Z are the horizontal drawing axes (the converter outputs vectors in the image plane).
  - Y is pen lift: `move_y_mm(-12)` is pen down, `move_y_mm(12)` is pen up.
- `SCALE = 0.1` converts pixels → mm.
- Bed size check: warns if the scaled contour bounding box exceeds ~200mm × 200mm.
- The image tool can auto-fit artwork to about `+/-25 mm` in X and Z.
