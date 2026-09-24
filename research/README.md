# Dynamic-view temporal reconstruction experiment

This directory turns the interactive renderer into a reproducible experiment for
low-sample path tracing under camera motion. The only proposed component is the
history-confidence estimator; scene loading, path tracing, and shading remain the
same for every comparison method.

## Formal comparison methods

| CLI method | Meaning |
|---|---|
| `fixed` | Original 90% history TAA with neighborhood clipping |
| `geometry` | Depth/normal rejection with fixed history weight; the key ablation |
| `proposed` | Geometry, normalized luminance residual, motion, and history-length confidence |

`reference` is used only to generate high-SPP ground truth and is not counted as a
comparison method. The implemented `none`, `inverse_variance`, `no_luminance`, and
`no_motion` modes remain available for debugging, but are excluded from the formal
experiment so the study stays focused.

The deterministic 180-frame path contains 30 static frames, 60 lateral-motion
frames, 30 slow rotation frames, 30 faster rotation frames, and 30 stopped frames.
All comparison methods use the same pose, resolution, path depth, SPP, and seed;
the reference deliberately uses a much larger sample count.

## Run

Run commands from the executable directory (`bin` in the Visual Studio Release x64
configuration). Paths below are examples and may be replaced with an absolute scene
path.

```powershell
.\PathTracerRenderer_research.exe --scene ..\assets\myscene\scene.scene --experiment-dir ..\research\runs\reference --method reference --reference-spp 256 --spp 8 --seed 104729 --width 640 --height 360
.\PathTracerRenderer_research.exe --scene ..\assets\myscene\scene.scene --experiment-dir ..\research\runs\fixed_s1 --method fixed --spp 1 --seed 1 --width 640 --height 360
.\PathTracerRenderer_research.exe --scene ..\assets\myscene\scene.scene --experiment-dir ..\research\runs\proposed_s1 --method proposed --spp 1 --seed 1 --width 640 --height 360
```

Evaluate the proposed result against the reference and fixed baseline:

```powershell
python ..\research\evaluate.py --candidate ..\research\runs\proposed_s1 --reference ..\research\runs\reference --baseline ..\research\runs\fixed_s1 --output ..\research\metrics\scene_s1
```

Use an independent reference seed, then repeat comparison runs for two scenes and
seeds 1 and 2. The main report keeps four metrics only: disocclusion RMSE and
reference-corrected temporal error as the primary evidence, with PSNR and median
GPU time as quality/performance constraints. `summary.json` reports the
pre-declared target checks. Treat them as evaluation criteria, not guaranteed
results: at least 15% lower disocclusion RMSE, 10% lower reference-corrected temporal
error, no more than 0.3 dB PSNR loss, and no more than 10% median GPU-time overhead
relative to fixed TAA. Other exported metrics are diagnostic only and do not need
to appear in the main result table.

For the focused three-method experiment, use `run_experiments.ps1`:

```powershell
.\research\run_experiments.ps1 -Executable .\bin\PathTracerRenderer_research.exe -Scene .\assets\myscene\still_life_V1_1.glb -OutputRoot .\research\runs\still_life
```

The batch script evaluates the fixed baseline, key geometry ablation, and complete
proposed method, then writes
`aggregate_summary.json` with the mean and sample standard deviation across seeds.

PFM outputs stay in linear HDR. Display-space metrics use one fixed global tone map;
the script also reports a linear-HDR relative error so exposure does not hide error.
