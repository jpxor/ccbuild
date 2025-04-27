# See C build!
A zero-config* build system for C and C++ projects, inspired by `go` and `cargo` style build tools.

```bash
 cc build [project_root]
```

\* configured with sane defaults, some configuration may be needed for more complex projects.

# A Simple C/C++ Build System

CCBuild will have your C and C++ projects building immediately, without having to fiddle with CMake, Autotools, or other configuration heavy complex build systems.

Primarily aimed at small to medium-sized projects, but enables advanced features like multiple build targets and cross-compilation with minimal configuration.

## How it Works

CCBuild scans the project directory, automatically detecting and compiling source files, only recompiling sources when changed (detects changes to header files as well).  Optionally, enable fully parallel compilation.

After compilation, CCBuild moves on to the linking stage, where it will detect all sources with an entry-point (`int main(...)`), and link each of them with the compiled obj files into separate executables.

A dead simple configuration file can be used to set any requried CFLAGS, LDFLAGS, link to external libraries, create build target, etc... Refer to the configuration section below for more details.

Right out of the box, you get:
1. fast, parrallel, incremental builds with zero configuration
2. support for multiple executables and build targets
3. simple setup for cross-compilation

## Features Recap

- **Simple configuration**: Uses an INI-style configuration file with sections and comments, with familiar variable names and sane defaults
- **Incremental builds**: Only rebuilds what has changed
- **Parallel compilation**: Speeds up build times with multi-threading
- **Multiple targets**: Define different build targets in a single config \**
- **Support for libraries**: Build static and shared libraries in addition to executables
- **Support for cross-compilation**: Build for different OS and architectures

### \** wait, but what is a build target?
Build targets are a way to break a larger project into components that require different build configurations. For example, you might have a project with a library and an application that uses that library. Build targets can also be used to configure builds for different operating systems or even architectures.

## Depends
CCBuild depends on gcc/clang and associated tools to do the actual grunt work...and that's it!

## Usage

```bash
Usage: cc <command>
Commands:
  build [-j NTHREADS] [--target=TARGET] [--release] [PROJECT_ROOT]
  clean
```
## Configuration File (cc.conf)

CCBuild uses an INI-style configuration file named `cc.conf` in your project root. It uses `[sections]` to define build targets, allows comments, and relies on familiar configuration variable names based on Make

Minimal configuration file:
```ini
# empty... this space is intentionally left blank
```

Example configuration to build a simple opengl+glfw project on windows:
```ini
LIBS = -lopengl32 -lgdi32 -lglfw3
LIBPATHS = ${PATH_TO_GLFW_LIB_DIR}
INCPATHS = ${PATH_TO_GLFW_INCLUDE_DIR}

# building for linux is left as an exercise for the reader
```

Example configuration to build all glfw examples and tests:
```ini
# include paths to build glfw and its examples & tests
INCPATHS = ./glfw/include ./glfw/deps

# silence warnings (default flags include -Wall -Wextra)
CCFLAGS =

# this target will build all compilation units found in glfw
# and then link each executable (all tests + examples)
[win32]
CCFLAGS += -D_WIN32 -D_GLFW_WIN32
LIBS = -lopengl32 -lgdi32

[X11] # this target not yet tested!
CCFLAGS += -D_GLFW_X11
LIBS = -lGL -lX11 -lXext
```
\** note: this does not build glfw as a library, it just builds the examples and tests. See the glfw example for a full build of glfw as a library.


## Configuration Options

The following options can be set in the `cc.conf` file:

| Option | Description | Default |
|--------|-------------|---------|
| `type` | Target type: `bin`, `static`, `shared` | `bin` |
| `cc` | Compiler to use (can be a list with \| separator) | `gcc\|clang\|cl` |
| `libname` | Library name | `$(TARGET)` |
| `build_root` | Directory for build artifacts | `./build/$(TARGET)/` |
| `install_root` | Installation root directory | `./install/$(TARGET)/` |
| `installdir` | Installation subdirectory | `""` |
| `srcpaths` | Source directories (space separated list) | `.` |
| `incpaths` | Include directories (space separated list) | `. ./includes` |
| `libpaths` | Library directories (space separated list) | `""` |
| `libs` | Libraries to link against | `""` |
| `ccflags` | Compiler flags | `-Wall -Wextra` |
| `ldflags` | Linker flags | `""` |
| `release` | Release build flags | `-O2 -DNDEBUG -Werror -D_FORTIFY_SOURCE=2` |
| `debug` | Debug build flags | `-g -O0` |
| `compile` | Compile command template | `$(CC) $(CCFLAGS) [DEBUG_OR_RELEASE] -I[INCPATHS] -o [OBJPATH] -c [SRCPATH]` |
| `link` | Link command template for binaries | `$(CC) $(LDFLAGS) [OBJS] -L[LIBPATHS] $(LIBS) -o [BINPATH]` |
| `link_static` | Link command template for static libraries | `ar rcs [BINPATH].a [OBJS]` |
| `link_shared` | Link command template for shared libraries | `$(CC) -shared -fPIC $(LDFLAGS) [OBJS] -L[LIBPATHS] $(LIBS) -o [BINPATH].so` |

### Variable Expansion

The config options can also be used as variables in your configuration with the `$(VARIABLE)` syntax. This allows one option (like a path or command template) to be dependent on another.

This is used to create the command templates and set the build/install roots for different targets.

** [TODO] allow environment variables to be used in config options ** 

### Command-line Options

- `-jN`: Set the number of parallel compilation jobs
- `--release`: Build in release mode (defaults to debug mode)
- `--target=target1`: Build a specific target

## Bootstrap

CCbuild is a C project that is built using CCBuild.

If you want to build CCBuild from source, use the provided `boostrap` script or use the provided `Makefile` and install the generated artifact (`cc`) to your system.

## Contributing

We welcome contributions that help improve this project and make it even better! Whether you have ideas for new features or fixes for existing issues, your input is highly valued. Here's how you can contribute:

1. **Feature Suggestions**: If you have ideas for new features or improvements, feel free to propose them by opening a new issue. We encourage discussions about potential additions to the roadmap.

2. **Bug Reports**: Found a bug? Let us know by submitting a detailed bug report through the issue tracker. Providing clear steps to reproduce the issue will help us address it more effectively.

3. **Pull Requests**: Implemented a roadmap feature or fixed a bug? Fantastic! You can contribute your changes via a pull request. Make sure to:
   - Follow the project's coding standards and guidelines.
   - Include test cases, if applicable.

<!-- 4. **Roadmap Contributions**: Check out our [Roadmap](link_to_roadmap) for planned features. You're welcome to pick any item you'd like to work on or suggest your own enhancements. Contributions aligned with the roadmap are especially appreciated! -->

We aim to foster a collaborative and friendly environment, so don't hesitate to reach out with any questions or suggestions. Thank you for contributing!


## License

This project is licensed under the Mozilla Public License v2.0 (MPL-2.0).

*Intent:* you are free to use CCBuild and its libcc library in your own projects, including proprietary ones, without affecting the licensing of your proprietary code. However, if you make modifications to CCBuild or libcc and distribute the modified software, you are required to share those improvements with the community under the same MPL-2.0 license. This ensures the project continues to evolve and benefit from collective contributions.

