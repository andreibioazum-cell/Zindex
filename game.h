#ifndef GAME_H
#define GAME_H

#include "world.h"
#include <android/asset_manager.h>
#include <GLES2/gl2.h>

typedef struct {
    GLuint prog, grass, stone;
    GLint mvp, tex, pos, uv;
    World world;
    Vec3 pos, vel;
    float pitch, yaw;
    int on_ground, fwd, side, jump, dig, place, block_type;
    int w,h;
} Game;

void game_init(Game* g, AAssetManager* assets);
void game_update(Game* g);
void game_render(Game* g);
void game_touch(Game* g, float x, float y, int action);
void game_move(Game* g, float dx, float dy);

#endif
