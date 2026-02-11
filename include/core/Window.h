#ifndef WINDOW_H
#define WINDOW_H

#include "Global.h"
#include <stdbool.h>
#include <cglm/types.h>

typedef struct Window Window;

Window* windowCreate(i32 width,i32 height,const char* title); 

void    windowDestroy(Window* window);

f32     windowAspectRatio(Window *window);

bool    windowShouldClose(Window *window);

bool    windowResized(Window *window);

void    windowUpdate(Window* window);

i32     windowWidth(Window* window);

i32     windowHeight(Window* window);

bool    windowMouseMoved(Window* window);

void    windowMouseOffset(Window* window,vec2 dest);

i32     windowGetKey(Window* window,int key);

void    windowToggleMouseLock(Window* window);

void    windowMousePos(Window* window,vec2 dest);

bool    windowPaused(Window* window);

i32     windowKeyState(Window* window,i32 key);

#endif
