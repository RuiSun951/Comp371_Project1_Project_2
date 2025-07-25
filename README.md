# COMP 371 – Project Assignment 1 (Summer 2025)

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

### Features
- Unique texture for each celestial body
- Dynamic lighting and shadows
- Free-flying camera view

### Controls
- `W`, `A`, `S`, `D`: Move the camera  
- `Shift`: Increase movement speed  
- Mouse: Control camera rotation  
- `1`: First-person view (FPP)  
- `2`: Third-person view (TPP)

### Folder Structure(
Project1/
├── README.md
├── .gitignore
├── shaders/
│   ├── vertexShader.glsl
│   └── fragmentShader.glsl
├── textures/
│   ├── sun.jpg
│   ├── planetA.jpg
│   ├── planetB.jpg
│   └── moon.jpg
├── models/
│   └── sphere.obj
├── include/
├── src/
│   ├── main.cpp
│   ├── Camera.cpp/h
│   └── SceneObjects.cpp/h
├── stb_image.h
└── OBJloader.h/.cpp
)
