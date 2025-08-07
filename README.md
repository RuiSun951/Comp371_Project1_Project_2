# COMP 371 – Project Assignment 1&2 (Summer 2025)

**Group Members:**  
- Rui Sun (40121956)  
- Yibo Tang (40285121)

## Topic: Mini Solar System

### Overview
A mini solar system simulation featuring:
- A central sun
- Two orbiting planets:
  - **Planet A**
  - **Planet B** with a **moon** (2-level hierarchy)
- Shooting star, as a dynamic light source

### Features
- Unique textures for each celestial body
- Dynamic lighting
- Free-flying camera view

### Controls
- `W`, `A`, `S`, `D`: Move the camera  
- `Shift`: Increase movement speed  
- Mouse: Control camera rotation  
- Press `Tab` to toggle between First-person view (FPP) and Third-person view (TPP)
- Press `Caps` to toggle between standard time lapse and 3x time lapse

### Folder Structure

```
Project1/
├── README.md
├── .gitignore
├── shaders/
│   ├── vertexShader.glsl
│   └── fragmentShader.glsl
├── textures/
│   ├── sun.jpg
│   ├── planetA.jpg (earth)
│   ├── planetB.jpg (mars)
│   ├── galaxy.jpg
│   └── moon.jpg
├── models/
│   └── sphere.obj
├── src/
│   ├── main.cpp
│   ├── camera.h
│   ├── Vertex.h
│   └── SceneObjects.h
├── OBJloader.h
└── compiled test program(s)
```

## Assignment 2

### Updated requirement
- complex object models for celestial bodies
- dynamic lighting with 2 light sources
- shadow map
- loosen requirement for camera control, more potential movements
- extra user interaction (eg: shooter game style)

### Updated Folder Structure Assignment 2

```
Project2/
├── README.md
├── .gitignore
├── shaders/
│   ├── vertexShader.glsl      # scene vertex shader
│   ├── fragmentShader.glsl    # scene fragment shader
│   ├── shadow_vertex.glsl     # shadow vertex shader
│   └── shadow_fragment.glsl   # shadow fragment shader
├── textures/
│   ├── sun.jpg
│   ├── planetA.jpg (earth)
│   ├── planetB.jpg (mars)
│   ├── galaxy.jpg
│   └── moon.jpg
├── models/
│   ├── sphere.obj (used in project 1)
│   ├── sun.obj
│   ├── mars.obj
│   ├── earth.obj
│   └── moon.obj
├── src/
│   ├── main.cpp
│   ├── camera.h
│   ├── Vertex.h
│   └── SceneObjects.h
├── OBJloader.h
└── compiled test program(s)
```