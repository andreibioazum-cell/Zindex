#include "world.h"
#include <stdlib.h>

void world_init(World* w) { w->count=0; }

void world_add(World* w, int x,int y,int z) {
    Chunk* c=&w->chunks[w->count++];
    c->x=x; c->y=y; c->z=z;
    c->pos=v3(x*CHUNK,y*CHUNK,z*CHUNK);
    for(int bx=0;bx<CHUNK;bx++)
    for(int by=0;by<CHUNK;by++)
    for(int bz=0;bz<CHUNK;bz++) {
        c->blocks[bx][by][bz].type = (bz==0||bz<4)?STONE:(bz==4?GRASS:AIR);
    }
}

static Chunk* find(World* w,int x,int y,int z) {
    for(int i=0;i<w->count;i++) {
        if(w->chunks[i].x==x && w->chunks[i].y==y && w->chunks[i].z==z)
            return &w->chunks[i];
    } return NULL;
}

Block* world_get(World* w,int x,int y,int z) {
    Chunk* c=find(w,x/CHUNK,y/CHUNK,z/CHUNK);
    if(!c) return NULL;
    return &c->blocks[x%CHUNK][y%CHUNK][z%CHUNK];
}

void world_set(World* w,int x,int y,int z,int type) {
    Block* b=world_get(w,x,y,z);
    if(b) b->type=type;
}
