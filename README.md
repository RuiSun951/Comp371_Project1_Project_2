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
├── texture/
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

### Updated feature
- added a complex object model for celestial body (spacestation around earth)
- update dynamic lighting with 2 light sources (shooting star and ceilling light)
- implement shadows
- Press `P` to toggle the background, for easier shadow observation (only active in view mode)
- fine tuned camera control and mouse sensitivity
- extra user interaction (eg: shooter game style)
- game UI wrap, with obeservation mode and game mode
- game mode hitbox design and score calculate
- Press `L` to return upper menu in game UI

### Updated Folder Structure Assignment 2

```
Project2/
├── README.md
├── .gitignore
├── shaders/
│   ├── vertexShader.glsl           # scene vertex shader
│   ├── fragmentShader.glsl         # scene fragment shader
│   ├── shadow_vertex.glsl          # shadow vertex shader
│   ├── shadow_fragment.glsl        # shadow fragment shader
│   ├── pointShadow_vertex.glsl     # point shadow vertex shader
│   └── pointShadow_fragment.glsl   # point shadow fragment shader
├── texture/
│   ├── sun.jpg
│   ├── planetA.jpg (earth)
│   ├── planetB.jpg (mars)
│   ├── galaxy.jpg
│   ├── metal.png
│   └── moon.jpg
├── models/
│   ├── sphere.obj (kept unchanged from project 1)
│   └── spacestation.obj (new complex model for project 2)
├── src/
│   ├── camera.h
│   ├── gameUI.cpp
│   ├── gameUI.h
│   ├── main.cpp
│   ├── Vertex.h
│   └── SceneObjects.h
├── OBJloader.h
├── stb/
│   └── stb_image.h
└── compiled test program(s): test...
```