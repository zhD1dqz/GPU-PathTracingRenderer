# PathTracer Studio

PathTracer Studio is an interactive OpenGL path tracer implemented in C++ and
GLSL. It supports GPU path tracing, BVH acceleration, physically based
materials, HDR environment lighting, progressive accumulation, temporal
reconstruction, denoising, and interactive camera navigation.

## Portable Windows release

End users do not need Visual Studio. Download `PathTracerStudio-Portable.zip`
from the GitHub Releases page, extract the complete archive, and double-click
`PathTracerStudio.exe`.

Requirements:

- Windows 10 or Windows 11, 64-bit
- An OpenGL 3.3-capable GPU and current graphics driver
- A supported 64-bit CPU

The portable archive includes the application-local runtime libraries, shaders,
default scene, and HDR environment map required by the renderer.

## User interface

The renderer uses an English, scene-first interface:

- The wider left slide-out panel contains scene loading, environment selection,
  statistics, output/post-processing controls, field of view, and navigation help.
- The wider right slide-out panel contains path tracing, motion-aware stable
  preview, and lighting controls.
- Use the edge arrow buttons to collapse or restore either side panel.
- Use the top arrow to collapse the application bar.
- Adjust **UI** in the top bar to change panel transparency.
- Select **Hide UI**, or press `Tab`, for an unobstructed scene view. Press `Tab`
  again or select the small top **v UI** handle to restore the interface.
- The interface uses the Windows Segoe UI font when it is available.

Camera controls:

- Left mouse drag: orbit
- Middle mouse drag: pan
- Mouse wheel: zoom
- `Ctrl+S`: save screenshot

## Creating the portable package

Run the following command from PowerShell in the project directory:

```powershell
.\make_portable_release.ps1
```

The generated archive is written to
`release/PathTracerStudio-Portable.zip`. Upload the ZIP as a GitHub Release
asset rather than committing it to the source repository.

## images
![Main Interface](docs/images/main-interface.png)
![Moving Perspective](docs/images/moving-perspective.png)
![Other Light Source](docs/images/other-light-source.png)
![Render Result](docs/images/render-result.png)
![Test Case](docs/images/test-case.png)