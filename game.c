#include "game.h"
#include <string.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"Craft",__VA_ARGS__)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static const char* VS = 
    "attribute vec3 aPos;attribute vec2 aUV;uniform mat4 uMVP;varying vec2 vUV;"
    "void main(){gl_Position=uMVP*vec4(aPos,1.0);vUV=aUV;}";
static const char* FS = 
    "#ifdef GL_ES\nprecision highp float;\n#endif\n"
    "uniform sampler2D uTex;varying vec2 vUV;"
    "void main(){gl_FragColor=texture2D(uTex,vUV);}";

static GLuint load_tex(AAssetManager* a,const char* name) {
    AAsset* f=AAssetManager_open(a,name,AASSET_MODE_BUFFER);
    if(!f) { LOGI("No %s",name); return 0; }
    size_t s=AAsset_getLength(f);
    unsigned char* d=malloc(s);
    AAsset_read(f,d,s); AAsset_close(f);
    int w,h,n; unsigned char* img=stbi_load_from_memory(d,s,&w,&h,&n,4);
    free(d); if(!img) return 0;
    GLuint tex; glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    stbi_image_free(img);
    return tex;
}

static GLuint comp_shader(const char* src,GLenum type) {
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[512]; glGetShaderInfoLog(s,512,NULL,log); LOGI("Shader err:%s",log); return 0; }
    return s;
}

void game_init(Game* g, AAssetManager* assets) {
    memset(g,0,sizeof(Game));
    g->w=1024; g->h=768;
    g->pos=v3(0,0,10);
    g->block_type=GRASS;
    
    world_init(&g->world);
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) world_add(&g->world,x,y,0);
    
    GLuint vs=comp_shader(VS,GL_VERTEX_SHADER);
    GLuint fs=comp_shader(FS,GL_FRAGMENT_SHADER);
    g->prog=glCreateProgram();
    glAttachShader(g->prog,vs); glAttachShader(g->prog,fs); glLinkProgram(g->prog);
    g->mvp=glGetUniformLocation(g->prog,"uMVP");
    g->tex=glGetUniformLocation(g->prog,"uTex");
    g->pos=glGetAttribLocation(g->prog,"aPos");
    g->uv=glGetAttribLocation(g->prog,"aUV");
    
    g->grass=load_tex(assets,"grass.png");
    g->stone=load_tex(assets,"stone.png");
    glEnable(GL_DEPTH_TEST);
}

void game_update(Game* g) {
    g->vel=v3_add(g->vel,v3(0,0,-0.008f));
    if(g->fwd) { float a=g->yaw*3.14159f/180.0f; Vec3 d=v3(cosf(a),sinf(a),0); g->vel=v3_add(g->vel,v3_scale(d,g->fwd*0.05f)); }
    if(g->side) { float a=g->yaw*3.14159f/180.0f; Vec3 d=v3(cosf(a-1.57f),sinf(a-1.57f),0); g->vel=v3_add(g->vel,v3_scale(d,g->side*0.05f)); }
    if(g->jump && g->on_ground) g->vel.z=0.15f;
    
    Vec3 np=v3_add(g->pos,g->vel);
    Block* b=world_get(&g->world,(int)np.x,(int)g->pos.y,(int)g->pos.z);
    if(b && b->type!=AIR) { g->vel.x=0; np.x=g->pos.x; }
    b=world_get(&g->world,(int)g->pos.x,(int)np.y,(int)g->pos.z);
    if(b && b->type!=AIR) { g->vel.y=0; np.y=g->pos.y; }
    b=world_get(&g->world,(int)g->pos.x,(int)g->pos.y,(int)np.z);
    if(b && b->type!=AIR) { if(np.z<g->pos.z) g->on_ground=1; g->vel.z=0; np.z=g->pos.z; } else g->on_ground=0;
    g->pos=np;
    g->vel=v3_scale(g->vel,0.85f);
    
    if(g->dig || g->place) {
        float a=g->yaw*3.14159f/180.0f, p=g->pitch*3.14159f/180.0f;
        Vec3 dir=v3(cosf(p)*cosf(a), cosf(p)*sinf(a), sinf(p));
        Vec3 cam=v3_add(g->pos,v3(0,0,1.6f));
        for(float d=0.5f;d<5.0f;d+=0.5f) {
            Vec3 hit=v3_add(cam,v3_scale(dir,d));
            int bx=(int)hit.x, by=(int)hit.y, bz=(int)hit.z;
            Block* b=world_get(&g->world,bx,by,bz);
            if(b) {
                if(g->dig && b->type!=AIR) { world_set(&g->world,bx,by,bz,AIR); g->dig=0; }
                if(g->place && b->type==AIR) { world_set(&g->world,bx,by,bz,g->block_type); g->place=0; }
                break;
            }
        }
    }
}

void game_render(Game* g) {
    glViewport(0,0,g->w,g->h);
    glClearColor(0.5f,0.7f,1.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    
    Mat4 proj,view,mvp;
    m4_persp(&proj,1.0f,(float)g->w/g->h,0.1f,100.0f);
    Vec3 cam=v3_add(g->pos,v3(0,0,1.6f));
    float a=g->yaw*3.14159f/180.0f, p=g->pitch*3.14159f/180.0f;
    Vec3 target=v3_add(cam,v3(cosf(p)*cosf(a), cosf(p)*sinf(a), sinf(p)));
    m4_look(&view,cam,target,v3(0,0,1));
    m4_mul(&mvp,&proj,&view);
    
    glUseProgram(g->prog);
    glUniformMatrix4fv(g->mvp,1,GL_FALSE,mvp.m);
    glUniform1i(g->tex,0);
    
    for(int ci=0;ci<g->world.count;ci++) {
        Chunk* c=&g->world.chunks[ci];
        for(int bx=0;bx<CHUNK;bx++)
        for(int by=0;by<CHUNK;by++)
        for(int bz=0;bz<CHUNK;bz++) {
            Block* b=&c->blocks[bx][by][bz];
            if(b->type==AIR) continue;
            GLuint tex=(b->type==GRASS)?g->grass:g->stone;
            glBindTexture(GL_TEXTURE_2D,tex);
            float x=c->pos.x+bx, y=c->pos.y+by, z=c->pos.z+bz;
            float verts[]={
                x,y,z,0,0, x+1,y,z,1,0, x+1,y+1,z,1,1, x,y+1,z,0,1,
                x,y,z+1,0,0, x,y+1,z+1,1,0, x+1,y+1,z+1,1,1, x+1,y,z+1,0,1,
                x,y,z,0,0, x,y,z+1,1,0, x,y+1,z+1,1,1, x,y+1,z,0,1,
                x+1,y,z,0,0, x+1,y+1,z,1,0, x+1,y+1,z+1,1,1, x+1,y,z+1,0,1,
                x,y,z,0,0, x,y,z+1,0,1, x+1,y,z+1,1,1, x+1,y,z,1,0,
                x,y+1,z,0,0, x+1,y+1,z,1,0, x+1,y+1,z+1,1,1, x,y+1,z+1,0,1
            };
            glEnableVertexAttribArray(g->pos);
            glEnableVertexAttribArray(g->uv);
            glVertexAttribPointer(g->pos,3,GL_FLOAT,GL_FALSE,5*sizeof(float),verts);
            glVertexAttribPointer(g->uv,2,GL_FLOAT,GL_FALSE,5*sizeof(float),verts+3);
            glDrawArrays(GL_QUADS,0,24);
        }
    }
    glDisableVertexAttribArray(g->pos);
    glDisableVertexAttribArray(g->uv);
}

void game_touch(Game* g,float x,float y,int action) {
    if(action==0) {
        if(x<g->w/3) g->fwd=1;
        else if(x<2*g->w/3) {
            if(y<g->h/2) g->dig=1;
            else { g->place=1; g->block_type=(g->block_type==GRASS)?STONE:GRASS; }
        } else g->side=1;
    } else {
        g->fwd=g->side=g->dig=g->place=g->jump=0;
    }
}

void game_move(Game* g,float dx,float dy) {
    g->yaw+=dx*0.5f;
    g->pitch+=dy*0.5f;
    if(g->pitch>89) g->pitch=89;
    if(g->pitch<-89) g->pitch=-89;
}
