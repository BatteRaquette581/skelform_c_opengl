// credits to learnopengl.com for the boilerplate!
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <skelform_c_opengl.h>
#include <stdio.h>


#ifdef _WIN32
#define slash "\\"
#else
#define slash "/"
#endif

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    struct skf_Vec2 resolution = {1280.0f, 720.0f};
    GLFWwindow *window = glfwCreateWindow((int) resolution.x, (int) resolution.y, "SkelForm C OpenGL Runtime Test", NULL, NULL);
    if (window == NULL) {
        const char *error;
        glfwGetError(&error);
        fprintf(stderr, "failed to create a GLFW window: %s\n", error);
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        fprintf(stderr, "failed to load GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    GLuint shader_program = skf_opengl_create_shader();
    struct skf_opengl_File *file1 = skf_opengl_load("tests" slash "samples" slash "skellington.skf");
    if (file1 == NULL) {
        fprintf(stderr, "failed to load file1\n");
        glDeleteProgram(shader_program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    struct skf_opengl_File *file2 = skf_opengl_load("tests" slash "samples" slash "skellina.skf");
    if (file2 == NULL) {
        fprintf(stderr, "failed to load file2\n");
        skf_opengl_free_file(file1);
        glDeleteProgram(shader_program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    struct skf_Vec2 scale = {1 / 2000000.0f, 1 / 2000000.0f};
    struct skf_opengl_ConstructOptions options1 = {1.0f, (struct skf_Vec2) {-0.5f, 0.0f}, scale};
    struct skf_Vec_Bone bones1 = skf_opengl_construct(&file1->armature, &options1, resolution);
    skf_opengl_create_mesh(file1, &bones1, &options1, resolution);
    struct skf_opengl_ConstructOptions options2 = {1.0f, (struct skf_Vec2) {0.5f, -0.2f}, scale};
    struct skf_Vec_Bone bones2 = skf_opengl_construct(&file1->armature, &options2, resolution);
    skf_opengl_create_mesh(file2, &bones2, &options2, resolution);
    skf_opengl_enable_transparency();

    struct skf_Vec_Animation animations1 = {0};
    struct skf_Vec_Animation animations2 = {0};
    struct skf_Vec_uint32_t blend_frames = {0};
    struct skf_Vec_uint32_t frames1 = {0};
    struct skf_Vec_uint32_t frames2 = {0};
    skf_Vec_append(animations1, file1->armature.animations.elements[0]);
    skf_Vec_append(animations2, file2->armature.animations.elements[0]);
    skf_Vec_append(blend_frames, 0);
    skf_Vec_append(frames1, 0);
    skf_Vec_append(frames2, 0);
    skf_bool loop = skf_true;
    skf_bool reverse = skf_false;
    skf_bool wireframe = skf_false;
    uint64_t start = skf_opengl_get_epoch_ms();
    glfwSetWindowTitle(window, strcat(strcat(_strdup(file1->armature.animations.elements[0].name), " | "),
        file2->armature.animations.elements[0].name));

    while (!glfwWindowShouldClose(window)) {
        double t1 = glfwGetTime();
        for (size_t i = 0; i < file1->armature.animations.size; i++) {
            if (i == 10)
                break;
            if (glfwGetKey(window, GLFW_KEY_0 + i) == GLFW_PRESS) {
                animations1.elements[0] = file1->armature.animations.elements[i];
            }
        }
        for (size_t i = 0; i < file2->armature.animations.size; i++) {
            if (i == 10)
                break;
            if (glfwGetKey(window, GLFW_KEY_0 + i) == GLFW_PRESS) {
                animations2.elements[0] = file2->armature.animations.elements[i];
                glfwSetWindowTitle(window, strcat(strcat(_strdup(file1->armature.animations.elements[i].name), " | "),
                    file2->armature.animations.elements[i].name));
            }
        }
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
            reverse = !reverse;
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
            loop = !loop;
        frames1.elements[0] = skf_time_frame((skf_opengl_get_epoch_ms() - start) / 1000.0f, &animations1.elements[0], reverse, loop);
        frames2.elements[0] = skf_time_frame((skf_opengl_get_epoch_ms() - start) / 1000.0f, &animations2.elements[0], reverse, loop);

        skf_opengl_animate(file1, &bones1, &animations1, &frames1, &blend_frames, &options1, resolution);
        skf_opengl_animate(file2, &bones2, &animations2, &frames2, &blend_frames, &options2, resolution);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        skf_opengl_draw(file1, shader_program);
        skf_opengl_draw(file2, shader_program);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    free(animations1.elements);
    free(animations2.elements);
    free(blend_frames.elements);
    free(frames1.elements);
    free(frames2.elements);
    glDeleteProgram(shader_program);
    skf_opengl_free_file(file1);
    skf_opengl_free_file(file2);
    glfwDestroyWindow(window);
    glfwTerminate();
}
