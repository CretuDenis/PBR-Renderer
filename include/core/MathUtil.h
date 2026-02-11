#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include "Global.h"
#include <cglm/types.h>

void unflatten3d(u32 i,u8 gridDim,ivec3 result);

u32 flatten3d(ivec3 pos,u8 gridDim);

void unflatten2d(u32 i,u8 gridDim,ivec2 result);

u32 flatten2d(ivec3 pos,u8 gridDim);

#endif
