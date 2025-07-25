# Comp371_Project1
This is project assignment 1 for COMP 371 summer 2025

Group members:
Rui Sun     40121956
Yibo Tang   40285121

Topic:
Mini Solar System

Overview:

Structure:
A sun in the middle
Planet A
Planet B: with a moon (2 level hierarchy)

Details:
Each body is mapped with unique texture
Dynamic lighting & shadow
Flying camera view

Controls:
WASD: control movement
Shift: increase speed of movement
mouse: move flying camera
"1": FPP
"2": TPP


Folder Structure:
Project1/
├── READ ME.md
├── gitignore
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


