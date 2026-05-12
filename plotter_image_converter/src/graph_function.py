from function_to_draw_vectors import main


if __name__ == "__main__":
    raise SystemExit(main())
"""
Generate plotter commands (move_y_mm, move_x_mm, move_z_mm, draw_vector) for y = f(x).

Matches conventions used by convert.py: pen Y axis for lift, X and Z as the drawing plane,
angles in degrees for draw_vector().
"""

from __future__ import annotations

import argparse
import math
import os
from pathlib import Path

import numpy as np

# Same as convert.py defaults
PEN_UP_Y_MM = 12
PEN_DOWN_Y_MM = -12
MIN_SEGMENT_MM = 0.01


def _safe_namespace() -> dict:
    """Limited globals for eval(); only math-ish names allowed."""
    ns = {"__builtins__": {}}
    ns["np"] = np
    ns["pi"] = np.pi
    ns["e"] = np.e
    # NumPy ufuncs so sin(x) works on linspace arrays
    for name in (
        "sin",
        "cos",
        "tan",
        "sinh",
        "cosh",
        "tanh",
        "exp",
        "log",
        "log10",
        "sqrt",
        "abs",
        "floor",
        "ceil",
    ):
        ns[name] = getattr(np, name)
    ns["arcsin"] = np.arcsin
    ns["arccos"] = np.arccos
    ns["arctan"] = np.arctan
    ns["arctan2"] = np.arctan2
    ns["round"] = np.round
    ns["minimum"] = np.minimum
    ns["maximum"] = np.maximum
    return ns


def evaluate_expression(expr: str, x: np.ndarray) -> np.ndarray:
    """Evaluate expr with vector x (variable name must be ``x``)."""
    ns = _safe_namespace()
    ns["x"] = np.asarray(x, dtype=np.float64)
    try:
        y = eval(expr, ns, ns)
    except Exception as e:
        raise ValueError(f"Invalid expression {expr!r}: {e}") from e
    y = np.asarray(y, dtype=np.float64)
    if y.shape != ns["x"].shape:
        raise ValueError("Expression must return the same shape as x (element-wise).")
    return y


def map_to_mm(
    x: np.ndarray,
    y: np.ndarray,
    xmin: float,
    xmax: float,
    width_mm: float,
    height_mm: float,
) -> tuple[np.ndarray, np.ndarray]:
    """Map math (x, y) into centered mm coordinates: x -> X mm, y -> Z mm (plot vertical)."""
    if xmax <= xmin:
        raise ValueError("xmax must be greater than xmin.")
    ymin = float(np.nanmin(y))
    ymax = float(np.nanmax(y))
    if not math.isfinite(ymin) or not math.isfinite(ymax):
        raise ValueError("Function produced no finite values in range.")
    if ymax <= ymin:
        ymax = ymin + 1e-9

    nx = (x - xmin) / (xmax - xmin)
    ny = (y - ymin) / (ymax - ymin)
    # Centered plot: x from [-w/2, w/2], z from [-h/2, h/2]; math y grows upward -> positive Z up
    xmm = (nx - 0.5) * width_mm
    zmm = (0.5 - ny) * height_mm
    return xmm, zmm


def emit_polyline(
    xmm: np.ndarray,
    zmm: np.ndarray,
    mirror_x: bool,
) -> list[str]:
    cmds: list[str] = []
    if len(xmm) < 2:
        return cmds

    cx = 0.0
    cz = 0.0

    dx0 = (-(xmm[0] - cx) if mirror_x else (xmm[0] - cx))
    dz0 = zmm[0] - cz
    if abs(dx0) > 1e-9:
        cmds.append(f"move_x_mm({dx0:.4f});")
    if abs(dz0) > 1e-9:
        cmds.append(f"move_z_mm({dz0:.4f});")
    cx = xmm[0]
    cz = zmm[0]

    cmds.append(f"move_y_mm({PEN_DOWN_Y_MM});")

    for i in range(1, len(xmm)):
        dx = (-(xmm[i] - xmm[i - 1]) if mirror_x else (xmm[i] - xmm[i - 1]))
        dz = zmm[i] - zmm[i - 1]
        dist = math.hypot(dx, dz)
        angle_deg = math.degrees(math.atan2(dz, dx))
        if dist > MIN_SEGMENT_MM:
            cmds.append(f"draw_vector({dist:.4f}, {angle_deg:.4f});")

    cmds.append(f"move_y_mm({PEN_UP_Y_MM});")
    return cmds


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Emit draw_vector commands for the graph of y = f(x)."
    )
    parser.add_argument(
        "expr",
        help='Expression in x, e.g. "sin(x)", "x**2", "exp(-x**2)"',
    )
    parser.add_argument("--xmin", type=float, default=-math.pi)
    parser.add_argument("--xmax", type=float, default=math.pi)
    parser.add_argument("--samples", type=int, default=300, help="Number of x samples.")
    parser.add_argument(
        "--width-mm",
        type=float,
        default=40.0,
        help="Plot width in mm (X axis).",
    )
    parser.add_argument(
        "--height-mm",
        type=float,
        default=40.0,
        help="Plot height in mm (Z axis, math y mapped vertically).",
    )
    parser.add_argument(
        "--mirror-x",
        action="store_true",
        help="Mirror horizontally (same idea as MIRROR_X in convert.py).",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=None,
        help="Output .txt path (default: output/graph_<sanitized>.txt).",
    )

    args = parser.parse_args()

    x = np.linspace(args.xmin, args.xmax, max(2, args.samples))
    y = evaluate_expression(args.expr, x)

    mask = np.isfinite(x) & np.isfinite(y)
    x = x[mask]
    y = y[mask]
    if len(x) < 2:
        raise SystemExit("Need at least two finite samples after evaluation.")

    xmm, zmm = map_to_mm(x, y, args.xmin, args.xmax, args.width_mm, args.height_mm)

    commands = emit_polyline(xmm, zmm, mirror_x=args.mirror_x)

    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent
    out_dir = project_dir / "output"
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.output:
        out_path = Path(args.output)
        if not out_path.is_absolute():
            out_path = (project_dir / out_path).resolve()
    else:
        safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in args.expr[:40])
        out_path = out_dir / f"graph_{safe}.txt"

    out_path.write_text("\n".join(commands), encoding="utf-8")

    total_draw = 0.0
    for line in commands:
        if line.startswith("draw_vector("):
            inside = line[len("draw_vector(") :].rstrip(")")
            d, _ = inside.split(",", 1)
            total_draw += float(d.strip())

    rel = os.path.relpath(out_path, Path.cwd())
    print(f"Generated {len(commands)} commands -> {rel}")
    print(f"Estimated total drawn distance: {total_draw:.2f} mm")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
