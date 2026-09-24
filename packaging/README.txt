PathTracer Studio - Portable Edition
====================================

QUICK START

1. Extract the entire ZIP archive to a normal folder.
2. Double-click PathTracerStudio.exe.
3. Keep the bin, assets, and src folders together. They contain required
   runtime libraries, scenes, environment maps, and shaders.

Launch.bat is provided as a fallback launcher. Do not run the application
directly from inside the ZIP archive.

SYSTEM REQUIREMENTS

- Windows 10 or Windows 11, 64-bit
- A GPU and graphics driver supporting OpenGL 3.3 or later
- A 64-bit CPU supported by Intel Open Image Denoise

The Microsoft Visual C++ runtime libraries required by this build are included
for app-local deployment. Visual Studio is not required to run the application.

INTERFACE

- Left panel: scene, environment, output, field of view, and navigation
- Right panel: path tracing, stable preview, and lighting
- Edge arrows: hide or restore the left and right panels
- Top arrow: hide or restore the top application bar
- UI slider: adjust panel transparency
- Hide UI or Tab: switch to an unobstructed scene view
- Ctrl+S: write an image to the captures folder

CAMERA

- Left mouse drag: orbit
- Middle mouse drag: pan
- Mouse wheel: zoom

TROUBLESHOOTING

- If the renderer does not open, update the graphics driver and try again.
- Windows SmartScreen may warn about an unsigned application. Verify that the
  archive came from the official project release before choosing Run anyway.
- If PathTracerStudio.exe is blocked, right-click it, open Properties, enable
  Unblock when that option is shown, and retry.
- Keep all extracted files in their original directory structure.
