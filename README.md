# Roach

A small, optimized wavefront software path tracer written in C++<br>
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

This produces a `roach` executable in the build directory.

## Run

```bash
./build/roach config.json
```

The first command-line argument is the JSON file path.

## Configuration format

The config file is a JSON object with:

- `samples`: the number of samples to render
- `spheres`: an array of sphere entries

If a sphere has no `material` field, it defaults to `lambertian`.

A lambertian sphere:

```json
{
  "position": [0.0, 0.0, 8.0],
  "radius": 2.0,
  "material": "lambertian",
  "albedo": "A0A0A0"
}
```

A reflective sphere:

```json
{
  "position": [0.0, 0.0, 10.0],
  "radius": 2.0,
  "material": "reflective"
}
```

A dielectric sphere:

```json
{
  "position": [0.0, -2.0, 8.0],
  "radius": 2.0,
  "material": "dielectric",
  "ior": 1.33,
  "width": 0.1
}
```

## Build options

- `-DROACH_ENABLE_AVX=OFF` switches to the non-AVX ray tracer implementation
- `-DCMAKE_BUILD_TYPE=Release` enables the optimized release flags
- `-DROACH_USE_NATIVE_ARCH=OFF` disables `-march=native`

Example:

```bash
cmake -S . -B build -DROACH_ENABLE_AVX=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
