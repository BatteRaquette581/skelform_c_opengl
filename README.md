# SkelForm C OpenGL Engine Runtime

A one-header C99 OpenGL engine runtime for SkelForm.

## Usage

The runtime must be used on an OpenGL compatibility profile, due to the shaders being made to support older OpenGL versions. It also requires the libraries from the `lib` folder moved to your include folder (no static library linking required). The usage of `glad` in the library can be swapped to any other OpenGL headers, by modifying line 42 in `skelform_c_opengl.h`:
```c
#include <glad/glad.h> // NOTE TO DEVELOPERS: you can modify this line to other OpenGL headers
```

### Running tests

Recommended debug configuration (this code is portable and can be used with other compilers, this is just an example with the GCC toolchain):
```shell
# -ggdb3 is recommended for debug builds
gcc --std=c99 -ggdb3 -Wall -Wextra -Wconversion -pedantic -Ilib -Itests/glfw -I. lib/miniz/miniz.c lib/glad/glad.c tests/(...).c -Ltests/glfw -lglfw3 -lgdi32 -lopengl32 -o main.exe # on windows, link with GDI and OpenGL
gcc --std=c99 -ggdb3 -Wall -Wextra -pedantic -Ilib -Itests/glfw -I. lib/miniz/miniz.c lib/glad/glad.c tests/(...).c -Ltests/glfw -lglfw3 -lGL -o main # on linux, link with GL

gdb ...
```

The GLFW static library .so/.a file must be placed in the `tests/glfw` folder.

## Limitations

For now, the only limitation is that only 1 texture atlas per armature is allowed.

## Changing the armature atlas OpenGL texture unit

By default, the runtime uses `GL_TEXTURE_0` to bind the texture atlas when drawing the mesh (but restores the previous binding afterwards). However, if the texture bound to the default `GL_TEXTURE_0` texture unit is not a `GL_TEXTURE_2D` (e.g a `GL_TEXTURE_CUBE_MAP`), that binding will not be restored. If you want to the texture unit used by the runtime to be an unused one, define `SKF_OPENGL_TEXTURE_UNIT` **before including the header**:
```c
// may require importing your GL headers before the skelform_c_opengl header
#define SKF_OPENGL_TEXTURE_UNIT GL_TEXTURE_1
#include <skelform_c_opengl.h>
```
