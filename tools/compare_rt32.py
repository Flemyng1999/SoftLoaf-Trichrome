#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path


def read_float_tiff(path):
    import cv2
    import numpy as np

    image = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise ValueError(f"could not read {path}")
    if image.ndim == 2:
        image = image[:, :, None]
    if image.shape[2] >= 3:
        image = image[:, :, :3]
    image = image.astype(np.float32, copy=False)
    return image


def common_center_crop(a, b):
    h = min(a.shape[0], b.shape[0])
    w = min(a.shape[1], b.shape[1])
    ay = (a.shape[0] - h) // 2
    ax = (a.shape[1] - w) // 2
    by = (b.shape[0] - h) // 2
    bx = (b.shape[1] - w) // 2
    return a[ay:ay + h, ax:ax + w], b[by:by + h, bx:bx + w], (ax, ay, bx, by)


def lowpass(image):
    import cv2

    sigma = max(1.0, min(image.shape[0], image.shape[1]) / 512.0)
    return cv2.GaussianBlur(image, (0, 0), sigmaX=sigma, sigmaY=sigma)


def compare(oracle, project):
    import numpy as np

    o = read_float_tiff(oracle)
    p = read_float_tiff(project)
    o, p, offsets = common_center_crop(o, p)
    ol = lowpass(o)
    pl = lowpass(p)
    low_mean_abs = float(np.mean(np.abs(ol - pl)))

    omean = np.mean(ol, axis=(0, 1), dtype=np.float64)
    pmean = np.mean(pl, axis=(0, 1), dtype=np.float64)
    gain = np.divide(omean, pmean, out=np.ones_like(omean), where=np.abs(pmean) > 1e-12)
    corrected = pl * gain.reshape((1, 1, -1))
    gain_mean_abs = float(np.mean(np.abs(ol - corrected)))
    ratios = np.divide(pmean, omean, out=np.zeros_like(pmean), where=np.abs(omean) > 1e-12)
    return {
        "oracle_width": o.shape[1],
        "oracle_height": o.shape[0],
        "project_width": p.shape[1],
        "project_height": p.shape[0],
        "crop_offset": f"oracle_x={offsets[0]};oracle_y={offsets[1]};project_x={offsets[2]};project_y={offsets[3]}",
        "low_mean_abs": f"{low_mean_abs:.9g}",
        "gain_mean_abs": f"{gain_mean_abs:.9g}",
        "channel_ratios": ";".join(f"{x:.9g}" for x in ratios[:3]),
    }


def main():
    parser = argparse.ArgumentParser(description="Compare RT32 float oracle TIFFs to project float TIFFs.")
    parser.add_argument("--oracle-dir", required=True)
    parser.add_argument("--project-dir", required=True)
    parser.add_argument("--out-csv", required=True)
    args = parser.parse_args()

    oracle_dir = Path(args.oracle_dir)
    project_dir = Path(args.project_dir)
    rows = []
    for oracle in sorted(oracle_dir.glob("*.rt32.tif*")):
        stem = oracle.name.split(".rt32.")[0]
        candidates = list(project_dir.glob(f"{stem}.project.tif*"))
        row = {
            "sample": stem,
            "oracle": str(oracle),
            "project": str(candidates[0]) if candidates else "",
            "status": "ok" if candidates else "missing_project",
            "failure_reason": "",
        }
        if candidates:
            try:
                row.update(compare(oracle, candidates[0]))
            except Exception as exc:
                row["status"] = "failed"
                row["failure_reason"] = str(exc)
        rows.append(row)

    fields = [
        "sample", "oracle", "project", "status", "oracle_width", "oracle_height",
        "project_width", "project_height", "crop_offset", "low_mean_abs",
        "gain_mean_abs", "channel_ratios", "failure_reason",
    ]
    with Path(args.out_csv).open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
