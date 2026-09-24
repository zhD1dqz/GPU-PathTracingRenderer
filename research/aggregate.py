"""Aggregate per-seed evaluation summaries into mean and sample standard deviation."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

import numpy as np


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    groups: dict[str, list[dict]] = defaultdict(list)
    for path in sorted(args.root.glob("metrics_*_s*/summary.json")):
        directory_name = path.parent.name
        method = directory_name.removeprefix("metrics_").rsplit("_s", 1)[0]
        with path.open(encoding="utf-8") as stream:
            groups[method].append(json.load(stream))
    if not groups:
        raise FileNotFoundError(f"No metrics_*_s*/summary.json under {args.root}")

    keys = ("psnr", "ssim", "hdr_relative_error", "temporal_error",
            "disocclusion_rmse", "edge_rmse", "gpu_median_ms", "gpu_p95_ms")
    result = {}
    for method, summaries in groups.items():
        method_result = {"seeds": len(summaries)}
        for key in keys:
            values = np.asarray([item[key] for item in summaries if key in item], dtype=float)
            if values.size:
                method_result[key] = {
                    "mean": float(np.nanmean(values)),
                    "std": float(np.nanstd(values, ddof=1)) if values.size > 1 else 0.0,
                }
        result[method] = method_result

    output = args.output or (args.root / "aggregate_summary.json")
    with output.open("w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2, allow_nan=True)
    print(json.dumps(result, indent=2, allow_nan=True))


if __name__ == "__main__":
    main()
