# Function To Draw Vectors

This tool samples a math function and turns it into plotter commands.

## Files

- `src/function_to_draw_vectors.py` - main script
- `output/` - sample generated graph command files
- `requirements.txt` - Python dependencies

## Install

```bash
python -m pip install -r requirements.txt
```

## Run

```bash
python src/function_to_draw_vectors.py "sin(x)" --xmin -3.14159 --xmax 3.14159 --samples 200 --width-mm 40 --height-mm 40
```

Included sample outputs:

- `graph_sin_x_.txt`
- `graph_cos_x_.txt`
- `graph_x__3.txt`
