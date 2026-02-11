#include "MathUtil.h"


void unflatten3d(u32 i,u8 gridDim,ivec3 result) {
    const i32 z = i / (gridDim * gridDim);
    const i32 remainder = i % (gridDim * gridDim);
    const i32 x = remainder / gridDim;
    const i32 y = remainder % gridDim;
    result[0] = x;
    result[1] = y;
    result[2] = z;
}

u32 flatten3d(ivec3 pos,u8 gridDim) {
    return pos[2] * gridDim * gridDim + pos[0] * gridDim + pos[1];
}

void unflatten2d(u32 i,u8 gridDim,ivec2 result) {
    const i32 x = i % gridDim;
    const i32 y = i / gridDim;
    result[0] = x;
    result[1] = y;
}

u32 flatten2d(ivec3 pos,u8 gridDim) {
    return pos[1] * gridDim + pos[0];
}
