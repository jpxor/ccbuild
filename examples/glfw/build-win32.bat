
@REM get glfw
git clone https://github.com/glfw/glfw.git

@REM buid win32 target with 12 threads in release mode
cc build --target=win32 -j12 --release .

@REM run example
./install/win32/triangle-opengl.exe
