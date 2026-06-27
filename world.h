#ifndef WORLD_H
#define WORLD_H

#include "math.h"

#define CHUNK 16
enum { AIR=0, GRASS, STONE };

typedef struct { unsigned char type; } Block;

typedef struct {
    Block blocks[CHUNK][CHUNK][CHUNK];
    int x,y,z;
    Vec3 pos;
} Chunk;

typedef struct {
    Chunk chunks[64];
    int count;
} World;

void world_init(World* w);
void world_add(World* w, int x, int y, int z);
Block* world_get(World* w, int x, int y, int z);
void world_set(World* w, int x, int y, int z, int type);

#endif
