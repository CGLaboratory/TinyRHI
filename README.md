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
- `tinyrhi_dynamic_uniform_offsets`: draws two triangles from one uniform buffer using dynamic offsets.
- `tinyrhi_multi_vertex_buffers`: binds position and color from separate vertex buffers.
- `tinyrhi_push_constants`: updates fragment color through push constants.
- `tinyrhi_textured_quad`: uploads a procedural checker texture and samples it through a bind group.
- `tinyrhi_depth_test`: draws overlapping geometry with a depth attachment.
- `tinyrhi_alpha_blend`: draws overlapping translucent geometry with blending.
- `tinyrhi_line_grid`: draws line primitives.
- `tinyrhi_scissor`: draws geometry clipped by a scissor rect.

After building, run them from:

```powershell
.\build\examples\Debug\tinyrhi_clear_window.exe
.\build\examples\Debug\tinyrhi_triangle.exe
.\build\examples\Debug\tinyrhi_dynamic_uniform_offsets.exe
.\build\examples\Debug\tinyrhi_multi_vertex_buffers.exe
.\build\examples\Debug\tinyrhi_push_constants.exe
.\build\examples\Debug\tinyrhi_textured_quad.exe
.\build\examples\Debug\tinyrhi_depth_test.exe
.\build\examples\Debug\tinyrhi_alpha_blend.exe
.\build\examples\Debug\tinyrhi_line_grid.exe
.\build\examples\Debug\tinyrhi_scissor.exe
```
