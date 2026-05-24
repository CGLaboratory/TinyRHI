# TinyRHI

TinyRHI is a small rendering hardware interface experiment. The current backend is OpenGL.

## Build

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Examples are enabled by default. Disable them with:

```powershell
cmake -S . -B build -DTINYRHI_BUILD_EXAMPLES=OFF
```

## Examples

The example window helper is currently Win32/WGL-only and requests an OpenGL 4.5 core context.

- `tinyrhi_clear_window`: creates an RHI instance and clears the swapchain.
- `tinyrhi_triangle`: draws a vertex-buffer triangle.
- `tinyrhi_indexed_quad`: draws an indexed quad.
- `tinyrhi_uniform_triangle`: updates a uniform buffer each frame.
- `tinyrhi_textured_quad`: uploads a procedural checker texture and samples it through a bind group.
- `tinyrhi_depth_test`: draws overlapping geometry with a depth attachment.

After building, run them from:

```powershell
.\build\examples\Debug\tinyrhi_clear_window.exe
.\build\examples\Debug\tinyrhi_triangle.exe
.\build\examples\Debug\tinyrhi_indexed_quad.exe
.\build\examples\Debug\tinyrhi_uniform_triangle.exe
.\build\examples\Debug\tinyrhi_textured_quad.exe
.\build\examples\Debug\tinyrhi_depth_test.exe
```
