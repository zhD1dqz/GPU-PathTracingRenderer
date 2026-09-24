"""Evaluate temporal path-tracing sequences exported by PathTracerRenderer.

The renderer writes linear-HDR PFM buffers. This script reports display-space
PSNR/SSIM, HDR relative error, reference-corrected temporal error, geometry-edge
error, disocclusion error, and GPU timing statistics.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path

import numpy as np

try:
    from skimage.metrics import structural_similarity
except ImportError:
    structural_similarity = None


def read_pfm(path: Path) -> np.ndarray:
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"Expected RGB PFM: {path}")
        dimensions = stream.readline().decode("ascii").split()
        width, height = int(dimensions[0]), int(dimensions[1])
        scale = float(stream.readline().decode("ascii"))
        endian = "<" if scale < 0 else ">"
        image = np.fromfile(stream, dtype=endian + "f4")
    return image.reshape(height, width, 3).astype(np.float32)


def display_map(linear: np.ndarray) -> np.ndarray:
    linear = np.maximum(linear, 0.0)
    luminance = np.sum(linear * np.array([0.2126, 0.7152, 0.0722]), axis=2, keepdims=True)
    mapped = linear / (1.0 + luminance)
    return np.clip(mapped, 0.0, 1.0) ** (1.0 / 2.2)


def bilinear_sample(image: np.ndarray, uv: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    height, width = image.shape[:2]
    x = uv[..., 0] * width - 0.5
    y = uv[..., 1] * height - 0.5
    valid = (x >= -0.5) & (x <= width - 0.5) & (y >= -0.5) & (y <= height - 0.5)
    x0 = np.floor(x).astype(np.int32)
    y0 = np.floor(y).astype(np.int32)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    x0 = np.clip(x0, 0, width - 1)
    y0 = np.clip(y0, 0, height - 1)
    x1 = np.clip(x0 + 1, 0, width - 1)
    y1 = np.clip(y0 + 1, 0, height - 1)
    a = image[y0, x0] * (1.0 - fx) + image[y0, x1] * fx
    b = image[y1, x0] * (1.0 - fx) + image[y1, x1] * fx
    return a * (1.0 - fy) + b * fy, valid


def reprojection_uv(motion: np.ndarray) -> np.ndarray:
    height, width = motion.shape[:2]
    x = (np.arange(width, dtype=np.float32) + 0.5) / width
    y = (np.arange(height, dtype=np.float32) + 0.5) / height
    grid_x, grid_y = np.meshgrid(x, y)
    return np.stack((grid_x, grid_y), axis=2) - motion[..., :2]


def masked_rmse(error: np.ndarray, mask: np.ndarray) -> float:
    if not np.any(mask):
        return float("nan")
    squared = np.mean(error * error, axis=2)
    return float(np.sqrt(np.mean(squared[mask])))


def geometry_masks(position: np.ndarray, normal: np.ndarray,
                   previous_position: np.ndarray, previous_normal: np.ndarray,
                   motion: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    uv = reprojection_uv(motion)
    warped_position, valid = bilinear_sample(previous_position, uv)
    warped_normal, _ = bilinear_sample(previous_normal, uv)
    current_hit = np.linalg.norm(normal, axis=2) > 0.25
    previous_hit = np.linalg.norm(warped_normal, axis=2) > 0.25
    position_scale = np.maximum(np.linalg.norm(position, axis=2), 1.0)
    position_delta = np.linalg.norm(position - warped_position, axis=2) / position_scale
    n0 = normal / np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-6)
    n1 = warped_normal / np.maximum(np.linalg.norm(warped_normal, axis=2, keepdims=True), 1e-6)
    normal_dot = np.sum(n0 * n1, axis=2)
    disocclusion = (~valid) | (current_hit != previous_hit)
    disocclusion |= current_hit & ((position_delta > 0.02) | (normal_dot < 0.85))

    dx = np.zeros_like(position_delta)
    dy = np.zeros_like(position_delta)
    dx[:, 1:] = np.linalg.norm(position[:, 1:] - position[:, :-1], axis=2) / position_scale[:, 1:]
    dy[1:, :] = np.linalg.norm(position[1:, :] - position[:-1, :], axis=2) / position_scale[1:, :]
    ndx = np.zeros_like(position_delta)
    ndy = np.zeros_like(position_delta)
    ndx[:, 1:] = 1.0 - np.sum(n0[:, 1:] * n0[:, :-1], axis=2)
    ndy[1:, :] = 1.0 - np.sum(n0[1:, :] * n0[:-1, :], axis=2)
    edge = (dx > 0.02) | (dy > 0.02) | (ndx > 0.15) | (ndy > 0.15)
    return disocclusion, edge


def image_metrics(candidate: np.ndarray, reference: np.ndarray) -> dict[str, float]:
    candidate_display = display_map(candidate)
    reference_display = display_map(reference)
    difference = candidate_display - reference_display
    mse = float(np.mean(difference * difference))
    psnr = float("inf") if mse == 0 else -10.0 * math.log10(mse)
    if structural_similarity is not None:
        ssim = float(structural_similarity(candidate_display, reference_display,
                                           channel_axis=2, data_range=1.0))
    else:
        x, y = candidate_display, reference_display
        ux, uy = np.mean(x), np.mean(y)
        vx, vy = np.var(x), np.var(y)
        covariance = np.mean((x - ux) * (y - uy))
        ssim = float(((2 * ux * uy + 0.01**2) * (2 * covariance + 0.03**2)) /
                     ((ux * ux + uy * uy + 0.01**2) * (vx + vy + 0.03**2)))
    relative = float(np.mean(np.abs(candidate - reference) /
                             (np.abs(reference) + 0.01)))
    return {"psnr": psnr, "ssim": ssim, "hdr_relative_error": relative}


def load_timings(directory: Path) -> dict[str, float]:
    path = directory / "timings.csv"
    if not path.exists():
        return {}
    with path.open(newline="", encoding="utf-8") as stream:
        values = [float(row["total_ms"]) for row in csv.DictReader(stream)]
    # Ignore shader/cache warm-up only when enough frames remain for statistics.
    values = values[10:] if len(values) > 20 else values
    if not values:
        return {}
    return {"gpu_median_ms": float(np.median(values)),
            "gpu_p95_ms": float(np.percentile(values, 95))}


def evaluate(candidate_dir: Path, reference_dir: Path) -> tuple[list[dict], dict]:
    candidate_files = sorted(candidate_dir.glob("frame_*_color.pfm"))
    if not candidate_files:
        raise FileNotFoundError(f"No frame_*_color.pfm in {candidate_dir}")

    rows: list[dict] = []
    previous_candidate = previous_reference = None
    previous_position = previous_normal = None
    for color_path in candidate_files:
        stem = color_path.name.removesuffix("_color.pfm")
        reference_path = reference_dir / f"{stem}_color.pfm"
        if not reference_path.exists():
            raise FileNotFoundError(reference_path)
        candidate = read_pfm(color_path)
        reference = read_pfm(reference_path)
        position = read_pfm(reference_dir / f"{stem}_position.pfm")
        normal = read_pfm(reference_dir / f"{stem}_normal.pfm")
        motion = read_pfm(candidate_dir / f"{stem}_motion.pfm")
        metrics = image_metrics(candidate, reference)
        metrics["frame"] = int(stem.split("_")[-1])

        display_error = display_map(candidate) - display_map(reference)
        if previous_position is not None:
            disocclusion, edge = geometry_masks(position, normal, previous_position,
                                                previous_normal, motion)
            metrics["disocclusion_rmse"] = masked_rmse(display_error, disocclusion)
            metrics["edge_rmse"] = masked_rmse(display_error, edge)
            uv = reprojection_uv(motion)
            warped_candidate, valid = bilinear_sample(display_map(previous_candidate), uv)
            warped_reference, _ = bilinear_sample(display_map(previous_reference), uv)
            temporal_residual = ((display_map(candidate) - warped_candidate) -
                                 (display_map(reference) - warped_reference))
            metrics["temporal_error"] = masked_rmse(temporal_residual, valid)
        else:
            metrics.update(disocclusion_rmse=float("nan"), edge_rmse=float("nan"),
                           temporal_error=float("nan"))
        rows.append(metrics)
        previous_candidate, previous_reference = candidate, reference
        previous_position, previous_normal = position, normal

    keys = ["psnr", "ssim", "hdr_relative_error", "temporal_error",
            "disocclusion_rmse", "edge_rmse"]
    summary = {key: float(np.nanmean([row[key] for row in rows])) for key in keys}
    summary.update(load_timings(candidate_dir))
    summary["frames"] = len(rows)
    return rows, summary


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--output", type=Path, default=Path("metrics"))
    args = parser.parse_args()

    rows, summary = evaluate(args.candidate, args.reference)
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "per_frame.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    if args.baseline:
        _, baseline = evaluate(args.baseline, args.reference)
        def reduction(key: str) -> float:
            return 100.0 * (baseline[key] - summary[key]) / baseline[key]
        summary["vs_baseline"] = {
            "disocclusion_reduction_percent": reduction("disocclusion_rmse"),
            "temporal_reduction_percent": reduction("temporal_error"),
            "psnr_delta_db": summary["psnr"] - baseline["psnr"],
            "ssim_delta": summary["ssim"] - baseline["ssim"],
            "gpu_overhead_percent": 100.0 *
                (summary.get("gpu_median_ms", float("nan")) - baseline.get("gpu_median_ms", float("nan"))) /
                baseline.get("gpu_median_ms", float("nan")),
        }
        comparison = summary["vs_baseline"]
        summary["target_checks"] = {
            "disocclusion_reduction_at_least_15_percent": comparison["disocclusion_reduction_percent"] >= 15.0,
            "temporal_reduction_at_least_10_percent": comparison["temporal_reduction_percent"] >= 10.0,
            "psnr_loss_no_more_than_0_3_db": comparison["psnr_delta_db"] >= -0.3,
            "ssim_loss_no_more_than_0_005": comparison["ssim_delta"] >= -0.005,
            "gpu_overhead_no_more_than_10_percent": comparison["gpu_overhead_percent"] <= 10.0,
        }

    with (args.output / "summary.json").open("w", encoding="utf-8") as stream:
        json.dump(summary, stream, indent=2, allow_nan=True)
    print(json.dumps(summary, indent=2, allow_nan=True))


if __name__ == "__main__":
    main()
