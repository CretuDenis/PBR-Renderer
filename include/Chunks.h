#ifndef CHUNKS_H
#define CHUNKS_H

#include <string.h>
#define FNL_IMPL

#include <cglm/cglm.h>
#include <stdbool.h>
#include "FastNoiseLite.h"
#include "Global.h"
#include <stdlib.h>
#include "MathUtil.h"

#define BlockType   u16
#define TextureType u16

typedef enum { FRONT,BACK,TOP,BOTTOM,LEFT,RIGHT }FaceDir;

typedef enum {
    BLOCK_AIR,
    BLOCK_CAVE_AIR,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_WATER,
    BLOCK_OAK_WOOD,
    BLOCK_OAK_PLANK_STAIRS,
    NUM_BLOCKS
}BlockType_t;

typedef enum {
    TEXTURE_GRASS_SIDE,
    TEXTURE_GRASS_TOP,
    TEXTURE_DIRT,
    TEXTURE_STONE,
    TEXTURE_WATER,
    NUM_TEXTURES
}TextureType_t;

typedef struct {
    FaceDir dir;
    TextureType texture; 
}BlockFace;

typedef struct {
    float roll;  // x rotation
    float pitch; // y rotation
    float yaw;   // z rotation
    TextureType texture; 
}GenericFace;

typedef struct {
    BlockFace faces[6]; 
    ivec3 inChunkPos;
    BlockType type;
}Cube;

typedef struct {
    ivec3 inChunkPos;
    GenericFace* faces;
    u16 numFaces;
}Block;

#define OCCLUDES_NOTHING    0b00000000 
#define OCCLUDES_EVERYTHING 0b00111111
#define OCCLUDES_FRONT      0b00000001 
#define OCCLUDES_BACK       0b00000010 
#define OCCLUDES_TOP        0b00000100 
#define OCCLUDES_BOTTOM     0b00001000 
#define OCCLUDES_LEFT       0b00010000 
#define OCCLUDES_RIGHT      0b00100000 

typedef struct {
    bool occludesFront  : 1; 
    bool occludesBack   : 1; 
    bool occludesTop    : 1; 
    bool occludesBottom : 1; 
    bool occludesLeft   : 1; 
    bool occludesRight  : 1; 
}Occlusion;

static Occlusion occludes[NUM_BLOCKS] = {0};

void initOcclusionLut() {
    BlockType blockType = BLOCK_AIR;
    switch(blockType) {
        case BLOCK_GRASS:
            memset(&occludes[BLOCK_GRASS],OCCLUDES_EVERYTHING,1);
        case BLOCK_DIRT:
            memset(&occludes[BLOCK_GRASS],OCCLUDES_EVERYTHING,1);
        case BLOCK_STONE:
            memset(&occludes[BLOCK_GRASS],OCCLUDES_EVERYTHING,1);
        case BLOCK_OAK_WOOD:
            memset(&occludes[BLOCK_GRASS],OCCLUDES_EVERYTHING,1);
        case BLOCK_OAK_PLANK_STAIRS:
            occludes[BLOCK_OAK_PLANK_STAIRS].occludesBottom = true;
        default:
            break;
    }
}

typedef struct {
    float*      generationSync;
    fnl_state*  heightNoise;
    u8          chunkDim;
}ChunkGenerator;
    
void chunkGeneratorCreate(ChunkGenerator* gen,u8 chunkDim,fnl_state* noise) {
    gen->generationSync = malloc(sizeof(float) * chunkDim * chunkDim);
    gen->heightNoise = noise;
    gen->chunkDim = chunkDim;
}

bool generateChunk(ChunkGenerator* gen,ivec3 chunkPos,BlockType* chunkBlocks) {
    ivec3 inChunkPos; 
    u8 chunkDim = gen->chunkDim;
    float* generationSync = gen->generationSync;
    fnl_state* heightNoise = gen->heightNoise;
    
    ivec2 surfacePos;
    for(int i = 0; i < chunkDim * chunkDim; ++i) {
        unflatten2d(i,chunkDim,surfacePos);
        generationSync[i] = fnlGetNoise2D(heightNoise,surfacePos[0],surfacePos[1]);
    }

    for(int i = 0; i < chunkDim * chunkDim * chunkDim; i++) {
        if(generationSync[i/chunkDim] >= 0.0f)
            chunkBlocks[i] = BLOCK_DIRT;
        else
            chunkBlocks[i] = BLOCK_AIR;
    }
    return SUCCESS;
}

// 32 16 8 4 2 1

typedef struct {
    u16 posIndex            : 15;
    u8 faceDir              : 3;
    u8 topLeftOcclusion     : 2;
    u8 topRightOcclusion    : 2;
    u8 bottomLeftOcclusion  : 2;
    u8 bottomRightOcclusion : 2;
    u8 materialId           : 8;
}FaceData32;

typedef struct {
    u16 posIndex            : 12;
    u8 faceDir              : 3;
    u8 topLeftOcclusion     : 2;
    u8 topRightOcclusion    : 2;
    u8 bottomLeftOcclusion  : 2;
    u8 bottomRightOcclusion : 2;
    u8 materialId           : 8;
}FaceData16;

typedef struct {
    u16 posIndex            : 9;
    u8 faceDir              : 3;
    u8 topLeftOcclusion     : 2;
    u8 topRightOcclusion    : 2;
    u8 bottomLeftOcclusion  : 2;
    u8 bottomRightOcclusion : 2;
    u8 materialId           : 8;
}FaceData8;

typedef struct {
    u16 posIndex            : 6;
    u8 faceDir              : 3;
    u8 topLeftOcclusion     : 2;
    u8 topRightOcclusion    : 2;
    u8 bottomLeftOcclusion  : 2;
    u8 bottomRightOcclusion : 2;
    u8 materialId           : 8;
}FaceData4;

typedef struct {
    u16 posIndex            : 3;
    u8 faceDir              : 3;
    u8 topLeftOcclusion     : 2;
    u8 topRightOcclusion    : 2;
    u8 bottomLeftOcclusion  : 2;
    u8 bottomRightOcclusion : 2;
    u8 materialId           : 8;
}FaceData2;

// precodition needs the global array that holds the occlusion for each block to be initialized
bool meshChunk(ivec3 chunkPos,i32 chunkDim,BlockType* chunkBlocks,void* faceBuffer) {
    bool empty = true; 
    ivec3 pos3d;
    const int totalBlocks = chunkDim * chunkDim * chunkDim;

    for(i32 i = 0; i < totalBlocks; ++i) {
        if(chunkBlocks[i] != BLOCK_AIR && chunkBlocks[i] != BLOCK_CAVE_AIR)
            empty = false;
        unflatten3d(i,chunkDim,pos3d);

        i32 topIndex    = i+1;        
        i32 bottomIndex = i-1;        

        i32 leftIndex   = i+chunkDim;
        i32 rightIndex  = i-chunkDim;

        i32 frontIndex  = i + chunkDim * chunkDim;
        i32 backIndex   = i - chunkDim * chunkDim;

        if(topIndex >= 0 && topIndex < totalBlocks && !occludes[chunkBlocks[topIndex]].occludesBottom) {
            
        }

        if(bottomIndex >= 0 && bottomIndex < totalBlocks && !occludes[chunkBlocks[bottomIndex]].occludesTop) {
            
        }

        if(leftIndex >= 0 && leftIndex < totalBlocks && !occludes[chunkBlocks[leftIndex]].occludesRight) {

        }

        if(rightIndex >= 0 && rightIndex < totalBlocks && !occludes[chunkBlocks[rightIndex]].occludesLeft) {

        }

        if(frontIndex >= 0 && frontIndex < totalBlocks && !occludes[chunkBlocks[frontIndex]].occludesBack) {

        }

        if(backIndex >= 0 && backIndex < totalBlocks && !occludes[chunkBlocks[backIndex]].occludesFront) {

        }
    }

    return empty;
}

#endif
