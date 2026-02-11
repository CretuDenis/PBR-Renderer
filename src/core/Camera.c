#include "Camera.h"
#include <cglm/cglm.h>

static vec3 worldUp = {0.0f,1.0f,0.0f};
static vec3 defaultFront = {0.0f,0.0f,1.0f};

void cameraDefaultInit(Camera* cam) {
    glm_vec3_zero(cam->pos);
    glm_vec3_copy(defaultFront,cam->front);
    glm_vec3_copy(worldUp,cam->up);
    glm_cross(cam->front,worldUp,cam->right);
    glm_normalize(cam->right);

    cam->yaw         = -90.0f;
    cam->pitch       = 0.0f;
    cam->speed       = 3.0f;
    cam->sensitivity = 0.25f; 
}

void cameraInit(Camera* cam,vec3 pos,float speed,float sens) {
    glm_vec3_copy(pos,cam->pos);
    glm_vec3_copy(defaultFront,cam->front);
    glm_vec3_copy(worldUp,cam->up);
    glm_cross(cam->front,worldUp,cam->right);
    glm_normalize(cam->right);

    cam->yaw         = -90.0f;
    cam->pitch       = 0.0f;
    cam->speed       = speed;
    cam->sensitivity = sens;
}

void cameraViewMat(Camera* const camera,mat4 view) {
    vec3 center;
    glm_vec3_add(camera->front,camera->pos,center);
    glm_lookat(camera->pos,center,worldUp,view);
}

void cameraUpdateOnMovement(Camera* camera,CameraMovement direction,float deltaTime) {
    float velocity = camera->speed * deltaTime;
    if (direction == FORWARD) {
        vec3 newFront;
        glm_vec3_scale(camera->front,velocity,newFront);
        glm_vec3_add(camera->pos,newFront,camera->pos);
    }
    if (direction == BACKWARD) {
        vec3 newFront;
        glm_vec3_scale(camera->front,velocity,newFront);
        glm_vec3_sub(camera->pos,newFront,camera->pos);
    }
    if (direction == LEFT_SIDE) {
        vec3 newRight;
        glm_vec3_scale(camera->right,velocity,newRight);
        glm_vec3_sub(camera->pos,newRight,camera->pos);
    }
    if (direction == RIGHT_SIDE) {
        vec3 newRight;
        glm_vec3_scale(camera->right,velocity,newRight);
        glm_vec3_add(camera->pos,newRight,camera->pos);
    }
}

void cameraUpdateMouse(Camera* camera,vec2 mousePos,bool paused) {
    static float lastX = 0.0f;
    static float lastY = 0.0f;
    static bool firstMouse = true;

    if(paused) {
        firstMouse = true;
        return;
    }

    float mouseX = mousePos[0];
    float mouseY = mousePos[1];

    if(firstMouse) {
        lastX = mouseX;
        lastY = mouseY;
        firstMouse = false;
    }

    float xoffset = mouseX - lastX;
    float yoffset = lastY - mouseY;
    lastX = mouseX;
    lastY = mouseY;
    xoffset *= camera->sensitivity; 
    yoffset *= camera->sensitivity; 

    camera->yaw   += xoffset;
    camera->pitch += yoffset;

    if (camera->pitch > 89.0f)
        camera->pitch = 89.0f;
    if (camera->pitch < -89.0f)
        camera->pitch = -89.0f;

    vec3 direction;
    direction[0] = cos(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
    direction[1] = sin(glm_rad(camera->pitch));
    direction[2] = sin(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
    glm_normalize(direction);
    glm_vec3_copy(direction,camera->front);

    glm_cross(camera->front,worldUp,camera->right);
    glm_normalize(camera->right);
}

