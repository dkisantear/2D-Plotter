import argparse
import math
import os
from pathlib import Path

import cv2
import numpy as np


# ----------------------------
# CONFIG (matches your firmware assumptions)
# ----------------------------
SCALE = 0.1  # base pixels -> mm (used when AUTO_FIT_TO_MAX_OFFSET_MM is False)
PEN_UP_Y_MM = 12
PEN_DOWN_Y_MM = -12

SMOOTHING_WINDOW = 5

BED_MAX_MM = 200.0
MAX_OFFSET_MM = 25.0
AUTO_FIT_TO_MAX_OFFSET_MM = True
MIRROR_X = True

# Preprocessing: Canny traces both sides of a thick stroke → double/concentric lines.
# "ink_blob" thresholds ink as regions → RETR_EXTERNAL gives one outer outline per stroke.
PREPROCESS_MODE = "ink_blob"  # or "canny_edges" for thin edge-only artwork

MIN_CONTOUR_AREA = 200


def smooth_contour(contour: np.ndarray, window: int = 5) -> np.ndarray:
    pts = contour[:, 0, :].astype(np.float32)
    if len(pts) < window:
        return contour

    half = window // 2
    smoothed = []

    for i in range(len(pts)):
        x_sum = 0.0
        y_sum = 0.0
        count = 0

        for j in range(i - half, i + half + 1):
            idx = max(0, min(len(pts) - 1, j))
            x_sum += float(pts[idx][0])
            y_sum += float(pts[idx][1])
            count += 1

        smoothed.append([x_sum / count, y_sum / count])

    return np.array(smoothed, dtype=np.int32).reshape(-1, 1, 2)


def preprocess_binary_ink(img: np.ndarray) -> np.ndarray:
    """Dark ink on light background → one outer contour per disconnected stroke.

    Uses thresholded ink regions instead of Canny (which traces both sides of a thick line).
    Avoid morphological *closing* across the whole image — it can bridge eyes/mouth to the
    head into one blob; light OPEN only removes speckle noise.
    """
    blurred = cv2.GaussianBlur(img, (3, 3), 0)
    _, bw = cv2.threshold(blurred, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)
    bw = cv2.morphologyEx(bw, cv2.MORPH_OPEN, np.ones((2, 2), np.uint8), iterations=1)
    return bw


def preprocess_canny_edges(img: np.ndarray) -> np.ndarray:
    blurred = cv2.GaussianBlur(img, (3, 3), 0)
    edges = cv2.Canny(blurred, 40, 120)
    return cv2.dilate(edges, np.ones((2, 2), np.uint8), iterations=1)


def contours_from_ink_mask_by_components(bw: np.ndarray) -> list[np.ndarray]:
    """One outer contour per connected ink region.

    Calling findContours on the full mask merges nested boundaries (e.g. ring-shaped head =
    outer + inner edge), which plots as concentric circles. Per-component contours avoid that.
    """
    mask = (bw > 127).astype(np.uint8)
    n, labels, stats, _ = cv2.connectedComponentsWithStats(mask, connectivity=8)
    out: list[np.ndarray] = []
    for i in range(1, n):
        if stats[i, cv2.CC_STAT_AREA] < MIN_CONTOUR_AREA:
            continue
        comp = np.uint8((labels == i) * 255)
        cs, _ = cv2.findContours(comp, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        for c in cs:
            if cv2.contourArea(c) >= MIN_CONTOUR_AREA:
                out.append(c)
    return out


def process_contour(contour: np.ndarray) -> np.ndarray:
    epsilon = 0.008 * cv2.arcLength(contour, True)
    contour = cv2.approxPolyDP(contour, epsilon, True)
    contour = smooth_contour(contour, SMOOTHING_WINDOW)
    return contour


def contour_start_end_mm(
    contour: np.ndarray, center_x_px: float, center_y_px: float, scale_used: float
) -> tuple[float, float, float, float]:
    x0, y0 = contour[0][0]
    x1, y1 = contour[-1][0]
    sx0 = -(x0 - center_x_px) if MIRROR_X else (x0 - center_x_px)
    sx1 = -(x1 - center_x_px) if MIRROR_X else (x1 - center_x_px)
    return (
        sx0 * scale_used,
        (y0 - center_y_px) * scale_used,
        sx1 * scale_used,
        (y1 - center_y_px) * scale_used,
    )


def contour_to_commands(contour: np.ndarray, scale_used: float) -> list[str]:
    cmds: list[str] = []
    if len(contour) < 2:
        return cmds

    contour = process_contour(contour)

    # "move" to start: we assume your STM32 code handles absolute positioning separately,
    # so we only output draw vectors for the stroke itself.
    x_prev, y_prev = contour[0][0]

    # pen down (you start up already; end of each contour lifts)
    cmds.append(f"move_y_mm({PEN_DOWN_Y_MM})")

    # draw stroke
    for i in range(1, len(contour)):
        x, y = contour[i][0]

        dx = (-(x - x_prev) if MIRROR_X else (x - x_prev)) * scale_used
        dy = (y - y_prev) * scale_used

        dist = math.hypot(dx, dy)
        angle = math.atan2(dy, dx)

        if dist > 0.01:
            angle_deg = math.degrees(angle)
            cmds.append(f"draw_vector({dist:.4f}, {angle_deg:.4f})")

        x_prev, y_prev = x, y

    # pen up
    cmds.append(f"move_y_mm({PEN_UP_Y_MM})")
    return cmds


def load_and_trace(
    image_path: Path,
) -> tuple[list[np.ndarray], tuple[float, float], tuple[float, float], tuple[float, float]]:
    img = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise ValueError(f"Could not load image: {image_path}")

    if PREPROCESS_MODE == "canny_edges":
        bw = preprocess_canny_edges(img)
    else:
        bw = preprocess_binary_ink(img)

    if PREPROCESS_MODE == "canny_edges":
        contours, _ = cv2.findContours(bw, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        contours = [c for c in contours if cv2.contourArea(c) >= MIN_CONTOUR_AREA]
    else:
        contours = contours_from_ink_mask_by_components(bw)

    contours = sorted(contours, key=lambda c: cv2.boundingRect(c)[1])

    if not contours:
        return [], (0.0, 0.0), (0.0, 0.0), (0.0, 0.0)

    # overall bounding box in pixels
    xs = []
    ys = []
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        xs.extend([x, x + w])
        ys.extend([y, y + h])

    min_x = float(min(xs))
    max_x = float(max(xs))
    min_y = float(min(ys))
    max_y = float(max(ys))

    width_px = float(max_x - min_x)
    height_px = float(max_y - min_y)

    center_x_px = (min_x + max_x) / 2.0
    center_y_px = (min_y + max_y) / 2.0

    half_w_mm = (width_px * SCALE) / 2.0
    half_h_mm = (height_px * SCALE) / 2.0

    return (
        contours,
        (width_px * SCALE, height_px * SCALE),
        (center_x_px, center_y_px),
        (half_w_mm, half_h_mm),
    )


def list_input_images(input_dir: Path) -> list[Path]:
    exts = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff", ".webp"}
    files = [p for p in input_dir.iterdir() if p.is_file() and p.suffix.lower() in exts]
    return sorted(files, key=lambda p: p.name.lower())


def prompt_user_to_choose(input_dir: Path) -> Path:
    files = list_input_images(input_dir)
    if not files:
        raise ValueError(f"No images found in {input_dir}")

    print("Choose an image from input/:")
    for i, p in enumerate(files, start=1):
        print(f"  {i}. {p.name}")

    while True:
        raw = input("Enter number: ").strip()
        try:
            idx = int(raw)
            if 1 <= idx <= len(files):
                return files[idx - 1]
        except ValueError:
            pass
        print(f"Invalid choice. Enter 1..{len(files)}.")


def resolve_paths(cli_path: str | None) -> tuple[Path, Path]:
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent
    input_dir = project_dir / "input"
    output_dir = project_dir / "output"

    output_dir.mkdir(parents=True, exist_ok=True)

    if cli_path:
        in_path = Path(cli_path)
        if not in_path.is_absolute():
            in_path = (project_dir / in_path).resolve()
    else:
        in_path = prompt_user_to_choose(input_dir).resolve()

    if not in_path.exists():
        raise ValueError(f"Input image does not exist: {in_path}")

    out_path = (output_dir / f"{in_path.stem}.txt").resolve()
    return in_path, out_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert an image to plotter commands.")
    parser.add_argument(
        "image",
        nargs="?",
        help="Path to input image (e.g. input/dog.png). If omitted, prompts from input/.",
    )
    args = parser.parse_args()

    in_path, out_path = resolve_paths(args.image)

    contours, (w_mm_base, h_mm_base), (center_x_px, center_y_px), (half_w_mm_base, half_h_mm_base) = load_and_trace(
        in_path
    )
    if not contours:
        print(f"No contours found in {in_path.name}; no output generated.")
        return 0

    scale_used = SCALE
    if AUTO_FIT_TO_MAX_OFFSET_MM and half_w_mm_base > 0 and half_h_mm_base > 0:
        # half_w_mm_base / SCALE = half-width in pixels. We want: half-width in mm == MAX_OFFSET_MM.
        fit_scale_w = (MAX_OFFSET_MM * SCALE) / half_w_mm_base
        fit_scale_h = (MAX_OFFSET_MM * SCALE) / half_h_mm_base
        scale_used = min(fit_scale_w, fit_scale_h)

    w_mm = (w_mm_base / SCALE) * scale_used if SCALE > 0 else 0.0
    h_mm = (h_mm_base / SCALE) * scale_used if SCALE > 0 else 0.0
    half_w_mm = w_mm / 2.0
    half_h_mm = h_mm / 2.0

    if w_mm > BED_MAX_MM or h_mm > BED_MAX_MM:
        print(
            f"WARNING: Scaled contour bounds are ~{w_mm:.1f}mm x {h_mm:.1f}mm "
            f"(bed ~{BED_MAX_MM:.0f}mm x {BED_MAX_MM:.0f}mm). Consider resizing the image."
        )

    if half_w_mm > MAX_OFFSET_MM or half_h_mm > MAX_OFFSET_MM:
        print(
            f"WARNING: Centered drawing would extend to ~±{half_w_mm:.1f}mm (X) and ±{half_h_mm:.1f}mm (Z). "
            f"Target is ±{MAX_OFFSET_MM:.0f}mm. Consider resizing the image or reducing SCALE."
        )

    # Pen lift logic (confirmed):
    # - Each contour starts with move_y_mm(-12) inside contour_to_commands()
    # - Each contour ends with move_y_mm(12) inside contour_to_commands()
    commands: list[str] = []
    current_x_mm = 0.0
    current_z_mm = 0.0

    for c in contours:
        c_proc = process_contour(c)
        if len(c_proc) < 2:
            continue

        start_x_mm, start_z_mm, end_x_mm, end_z_mm = contour_start_end_mm(
            c_proc, center_x_px, center_y_px, scale_used
        )

        dx_mm = start_x_mm - current_x_mm
        dz_mm = start_z_mm - current_z_mm

        # Travel (pen should already be up at program start; and up after each contour)
        if abs(dx_mm) > 1e-6:
            commands.append(f"move_x_mm({dx_mm:.4f})")
        if abs(dz_mm) > 1e-6:
            commands.append(f"move_z_mm({dz_mm:.4f})")

        commands.extend(contour_to_commands(c_proc, scale_used))

        current_x_mm = end_x_mm
        current_z_mm = end_z_mm

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(commands), encoding="utf-8")

    total_draw_mm = 0.0
    for line in commands:
        if not line.startswith("draw_vector("):
            continue
        inside = line[len("draw_vector(") :].rstrip(")")
        dist_str, _angle_str = inside.split(",", 1)
        total_draw_mm += float(dist_str.strip())

    # keep this print simple so it's easy to parse/log
    rel_out = os.path.relpath(out_path, Path.cwd())
    print(f"Generated {len(commands)} commands -> {rel_out}")
    print(f"Estimated total drawn distance: {total_draw_mm:.2f} mm")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
