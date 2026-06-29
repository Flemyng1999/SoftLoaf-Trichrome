#!/usr/bin/env python3
import argparse
import subprocess
from pathlib import Path

RAW_EXTS = {
    ".3fr", ".arw", ".cr2", ".cr3", ".dng", ".fff", ".nef", ".orf",
    ".pef", ".raf", ".raw", ".rw2", ".srw", ".x3f",
}

PP3_TEXT = """[Version]
AppVersion=5.11

[Exposure]
Enabled=false
ToneCurve=false

[Color Management]
InputProfile=(camera)
WorkingProfile=Rec2020
WorkingTRC=none
OutputProfile=RTv4_Rec2020
ApplyLookTable=false
ApplyBaselineExposureOffset=false
ApplyHueSatMap=false

[Detail]
SharpeningEnabled=false

[Directional Pyramid Denoising]
Enabled=false

[Impulse Denoising]
Enabled=false

[LensProfile]
LcMode=none
LCPFile=
UseDistortion=false
UseVignette=false
UseCA=false
"""


def collect_inputs(paths):
    out = []
    for path in paths:
        p = Path(path)
        if p.is_dir():
            out.extend(x for x in sorted(p.rglob("*")) if x.suffix.lower() in RAW_EXTS)
        elif p.suffix.lower() in RAW_EXTS:
            out.append(p)
    return out


def read_input_list(path):
    with Path(path).open("r", encoding="utf-8") as f:
        return [line.strip() for line in f if line.strip() and not line.startswith("#")]


def main():
    parser = argparse.ArgumentParser(
        description="Generate RawTherapee 32-bit float scene-linear Rec.2020 TIFF oracles."
    )
    parser.add_argument("inputs", nargs="*", help="RAW files or directories")
    parser.add_argument("--input-list", help="Newline-delimited RAW files or directories")
    parser.add_argument("--out-dir", required=True, help="Oracle TIFF directory")
    parser.add_argument("--pp3", help="Use an existing pp3 instead of writing the default")
    parser.add_argument("--rawtherapee-cli", default="rawtherapee-cli")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    pp3 = Path(args.pp3) if args.pp3 else out_dir / "linear_rec2020_rt32.pp3"
    if not args.pp3:
        pp3.write_text(PP3_TEXT, encoding="utf-8")

    input_paths = list(args.inputs)
    if args.input_list:
        input_paths.extend(read_input_list(args.input_list))
    raws = collect_inputs(input_paths)
    if not raws:
        raise SystemExit("no RAW inputs found")

    print("raw,oracle,status")
    for raw in raws:
        out = out_dir / f"{raw.stem}.rt32.tiff"
        cmd = [
            args.rawtherapee_cli, "-q", "-Y", "-o", str(out), "-p", str(pp3),
            "-t", "-b32", "-c", str(raw),
        ]
        if args.dry_run:
            print(f"{raw},{out},dry-run:{' '.join(cmd)}")
            continue
        result = subprocess.run(cmd, text=True, capture_output=True)
        status = "ok" if result.returncode == 0 else f"failed:{result.returncode}"
        print(f"{raw},{out},{status}")
        if result.returncode != 0:
            if result.stderr:
                print(result.stderr.strip())
            raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
