#include "Window.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <cglm/cglm.h>

#define MOUSE_MOVED  0b00000001
#define RESIZED      0b00000010
#define FIRST_MOUSE  0b00000100
#define MOUSE_LOCKED 0b00001000

struct Window {
    GLFWwindow* windPtr;
    const char* title;
    float lastX;
    float lastY;
    float mouseX;
    float mouseY;
    float xoffset;
    float yoffset;
    i32 width;
    i32 height;
    u8 flags;
};

static void framebufferSizeCallback(GLFWwindow* window, int w, int h) {
    Window* win = (Window*)glfwGetWindowUserPointer(window);
    win->width   = w;
    win->height  = h;
    win->flags |= RESIZED;
    glViewport(0, 0, w, h);
}

static void cursorCallback(GLFWwindow* window, double xposIn, double yposIn) {
    Window* wind = (Window*)glfwGetWindowUserPointer(window);
    float xpos = xposIn;
    float ypos = yposIn;
    wind->mouseX = xpos;
    wind->mouseY = ypos;

    if (wind->flags & FIRST_MOUSE) {
        wind->lastX = xpos;
        wind->lastY = ypos;
        wind->flags &= ~FIRST_MOUSE;
    }

    wind->xoffset = xpos - wind->lastX;
    wind->yoffset = wind->lastY - ypos;
    wind->lastX = xpos;
    wind->lastY = ypos;
    wind->flags |= MOUSE_MOVED;
}

Window* windowCreate(i32 width,i32 height,const char* title) {
    Window* window = malloc(sizeof(Window)); 
    if(!window)
        return NULL;

    window->windPtr    = glfwCreateWindow(width, height,title, NULL, NULL);
    window->width      = width;
    window->height     = height;
    window->lastX      = width/2.0f;
    window->lastY      = height/2.0f;
    window->title      = title;
    window->flags     |= FIRST_MOUSE | MOUSE_LOCKED;

    if (!window->windPtr)
        return NULL;

    glfwSetInputMode(window->windPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwMakeContextCurrent(window->windPtr);
    glfwSetFramebufferSizeCallback(window->windPtr,framebufferSizeCallback);
    glfwSetCursorPosCallback(window->windPtr,cursorCallback);
    glfwSetWindowUserPointer(window->windPtr, window);
    return window;
}

void windowDestroy(Window* window) {
    glfwDestroyWindow(window->windPtr);
    free(window);
}

f32 windowAspectRatio(Window *window) {
    if(window->height == 0) return 1.0f;
    return (f32)window->width/window->height; 
}

bool windowShouldClose(Window *window) {
    return glfwWindowShouldClose(window->windPtr);
}

bool windowResized(Window *window) { 
    bool cpy = window->flags & RESIZED;
    window->flags &= ~RESIZED;
    return cpy;
}

void windowUpdate(Window* window) {
    glfwSwapBuffers(window->windPtr);
    glfwPollEvents();
}

i32 windowWidth(Window* window) {
    return window->width;
}

i32 windowHeight(Window* window) {
    return window->height;
}

bool windowMouseMoved(Window* window) {
    bool cpy = window->flags & MOUSE_MOVED;
    window->flags &= ~MOUSE_MOVED;
    return cpy;
}

void windowMouseOffset(Window* window,vec2 dest) {
    vec2 offsets = { window->xoffset, window->yoffset };
    glm_vec2_copy(offsets,dest);
}

i32 windowGetKey(Window* window,int key) {
    return glfwGetKey(window->windPtr,key);
}

void windowToggleMouseLock(Window* window) {
    if(window->flags & MOUSE_LOCKED)
        glfwSetInputMode(window->windPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
        glfwSetInputMode(window->windPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    window->flags ^= MOUSE_LOCKED;
}

void windowMousePos(Window* window,vec2 dest) {
    vec2 mousePos = { window->mouseX, window->mouseY };
    glm_vec2_copy(mousePos,dest);
}
bool windowPaused(Window* window) {
    return (window->flags & MOUSE_LOCKED);
}

i32 windowKeyState(Window* window,i32 key) {
    return glfwGetKey(window->windPtr,key); 
}

