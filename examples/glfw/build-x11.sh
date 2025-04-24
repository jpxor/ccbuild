#!/bin/bash

# get glfw
git clone https://github.com/glfw/glfw.git

# build X11 target with 12 threads in release mode
cc build --target=X11 -j12 --release .

# run example
./install/X11/triangle-opengl
