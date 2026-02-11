#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/types.h>
#include <stdbool.h>

typedef enum { 
    FORWARD,
    BACKWARD,
    LEFT_SIDE,
    RIGHT_SIDE
}CameraMovement;

typedef struct {
    vec3 pos;
    vec3 front;
    vec3 up;
    vec3 right;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;
}Camera;

void cameraDefaultInit(Camera* cam);

void cameraInit(Camera* cam,vec3 pos,float speed,float sens);

void cameraViewMat(Camera* const camera,mat4 view);

void cameraUpdateOnMovement(Camera* camera,CameraMovement direction,float deltaTime);

void cameraUpdateMouse(Camera* camera,vec2 mousePos,bool paused);

#endif
