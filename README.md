# Roach

A small and optimized software wavefront path tracer written in C++<br>
Utilizes AVX2 instruction set extensions to vectorize intersection and shading logic<br>
Generates ppm images of scenes<br>
Reads settings.json for scene options, allowing for arbitrarily many spheres of various materials

## Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Build options

- `-DROACH_ENABLE_AVX=OFF` switches to the non-AVX ray tracer implementation
- `-DCMAKE_BUILD_TYPE=Release` enables the optimized release flags
- `-DROACH_USE_NATIVE_ARCH=OFF` disables `-march=native`
