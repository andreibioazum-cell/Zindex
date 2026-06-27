#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Craft", __VA_ARGS__)

// ===== МАТЕМАТИКА =====
typedef struct { float x,y,z; } Vec3;
static inline Vec3 v3(float x,float y,float z) { Vec3 v={x,y,z}; return v; }
static inline Vec3 v3_add(Vec3 a,Vec3 b) { return v3(a.x+b.x,a.y+b.y,a.z+b.z); }
static inline Vec3 v3_sub(Vec3 a,Vec3 b) { return v3(a.x-b.x,a.y-b.y,a.z-b.z); }
static inline Vec3 v3_scale(Vec3 a,float s) { return v3(a.x*s,a.y*s,a.z*s); }
static inline float v3_dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static inline float v3_len(Vec3 a) { return sqrtf(v3_dot(a,a)); }
static inline Vec3 v3_norm(Vec3 a) { float l=v3_len(a); return l?v3_scale(a,1.0f/l):v3(0,0,0); }
static inline Vec3 v3_cross(Vec3 a,Vec3 b) { return v3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }

typedef struct { float m[16]; } Mat4;
static void m4_id(Mat4* m) { for(int i=0;i<16;i++) m->m[i]=(i%5==0)?1.0f:0.0f; }
static void m4_mul(Mat4* r,Mat4* a,Mat4* b) {
    Mat4 t; for(int i=0;i<4;i++) for(int j=0;j<4;j++) {
        float s=0; for(int k=0;k<4;k++) s+=a->m[i*4+k]*b->m[k*4+j];
        t.m[i*4+j]=s;
    } *r=t;
}
static void m4_persp(Mat4* m,float fov,float asp,float near,float far) {
    m4_id(m); float f=1.0f/tanf(fov*0.5f);
    m->m[0]=f/asp; m->m[5]=f; m->m[10]=(far+near)/(near-far); m->m[11]=-1.0f; m->m[14]=(2.0f*far*near)/(near-far); m->m[15]=0.0f;
}
static void m4_look(Mat4* m,Vec3 eye,Vec3 target,Vec3 up) {
    Vec3 f=v3_norm(v3_sub(target,eye)), s=v3_norm(v3_cross(f,up)), u=v3_cross(s,f);
    m4_id(m);
    m->m[0]=s.x; m->m[4]=s.y; m->m[8]=s.z;
    m->m[1]=u.x; m->m[5]=u.y; m->m[9]=u.z;
    m->m[2]=-f.x; m->m[6]=-f.y; m->m[10]=-f.z;
    m->m[12]=-v3_dot(s,eye); m->m[13]=-v3_dot(u,eye); m->m[14]=v3_dot(f,eye);
}

// ===== МИР =====
#define CHUNK_SIZE 16
enum { AIR=0, GRASS, STONE };
typedef struct { unsigned char type; } Block;
typedef struct { Block blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]; int x,y,z; Vec3 pos; } Chunk;
typedef struct { Chunk chunks[64]; int count; } World;

static void world_init(World* w) { w->count=0; }
static void world_add(World* w,int x,int y,int z) {
    Chunk* c=&w->chunks[w->count++];
    c->x=x; c->y=y; c->z=z; c->pos=v3(x*CHUNK_SIZE,y*CHUNK_SIZE,z*CHUNK_SIZE);
    for(int bx=0;bx<CHUNK_SIZE;bx++) for(int by=0;by<CHUNK_SIZE;by++) for(int bz=0;bz<CHUNK_SIZE;bz++) {
        c->blocks[bx][by][bz].type = (bz==0||bz<4)?STONE:(bz==4?GRASS:AIR);
    }
}
static Chunk* world_find(World* w,int x,int y,int z) {
    for(int i=0;i<w->count;i++) if(w->chunks[i].x==x&&w->chunks[i].y==y&&w->chunks[i].z==z) return &w->chunks[i];
    return NULL;
}
static Block* world_get(World* w,int x,int y,int z) {
    Chunk* c=world_find(w,x/CHUNK_SIZE,y/CHUNK_SIZE,z/CHUNK_SIZE);
    if(!c) return NULL;
    return &c->blocks[x%CHUNK_SIZE][y%CHUNK_SIZE][z%CHUNK_SIZE];
}
static void world_set(World* w,int x,int y,int z,int type) {
    Block* b=world_get(w,x,y,z);
    if(b) b->type=type;
}

// ===== STB IMAGE =====
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ===== ИГРА =====
typedef struct {
    GLuint prog, grass, stone;
    GLint mvp, tex, pos, uv;
    World world;
    Vec3 pos, vel;
    float pitch, yaw;
    int on_ground, fwd, side, jump, dig, place, block_type;
    int w,h;
} Game;

static const char* VS = "attribute vec3 aPos;attribute vec2 aUV;uniform mat4 uMVP;varying vec2 vUV;void main(){gl_Position=uMVP*vec4(aPos,1.0);vUV=aUV;}";
static const char* FS = "#ifdef GL_ES\nprecision highp float;\n#endif\nuniform sampler2D uTex;varying vec2 vUV;void main(){gl_FragColor=texture2D(uTex,vUV);}";

static GLuint load_tex(AAssetManager* a,const char* name) {
    AAsset* f=AAssetManager_open(a,name,AASSET_MODE_BUFFER);
    if(!f) return 0;
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

static void game_init(Game* g, AAssetManager* assets) {
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

static void game_update(Game* g) {
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

static void game_render(Game* g) {
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
    
    // Вершинные данные для куба (треугольники вместо QUADS)
    float cube_verts[] = {
        // Передняя
        -0.5,-0.5,0.5, 0,0,  0.5,-0.5,0.5, 1,0,  0.5,0.5,0.5, 1,1,
        -0.5,-0.5,0.5, 0,0,  0.5,0.5,0.5, 1,1,  -0.5,0.5,0.5, 0,1,
        // Задняя
        -0.5,-0.5,-0.5, 0,0,  -0.5,0.5,-0.5, 1,0,  0.5,0.5,-0.5, 1,1,
        -0.5,-0.5,-0.5, 0,0,  0.5,0.5,-0.5, 1,1,  0.5,-0.5,-0.5, 0,1,
        // Левая
        -0.5,-0.5,-0.5, 0,0,  -0.5,-0.5,0.5, 1,0,  -0.5,0.5,0.5, 1,1,
        -0.5,-0.5,-0.5, 0,0,  -0.5,0.5,0.5, 1,1,  -0.5,0.5,-0.5, 0,1,
        // Правая
        0.5,-0.5,-0.5, 0,0,  0.5,0.5,-0.5, 1,0,  0.5,0.5,0.5, 1,1,
        0.5,-0.5,-0.5, 0,0,  0.5,0.5,0.5, 1,1,  0.5,-0.5,0.5, 0,1,
        // Нижняя
        -0.5,-0.5,-0.5, 0,0,  -0.5,-0.5,0.5, 0,1,  0.5,-0.5,0.5, 1,1,
        -0.5,-0.5,-0.5, 0,0,  0.5,-0.5,0.5, 1,1,  0.5,-0.5,-0.5, 1,0,
        // Верхняя
        -0.5,0.5,-0.5, 0,0,  -0.5,0.5,0.5, 0,1,  0.5,0.5,0.5, 1,1,
        -0.5,0.5,-0.5, 0,0,  0.5,0.5,0.5, 1,1,  0.5,0.5,-0.5, 1,0
    };
    
    for(int ci=0;ci<g->world.count;ci++) {
        Chunk* c=&g->world.chunks[ci];
        for(int bx=0;bx<CHUNK_SIZE;bx++)
        for(int by=0;by<CHUNK_SIZE;by++)
        for(int bz=0;bz<CHUNK_SIZE;bz++) {
            Block* b=&c->blocks[bx][by][bz];
            if(b->type==AIR) continue;
            GLuint tex=(b->type==GRASS)?g->grass:g->stone;
            glBindTexture(GL_TEXTURE_2D,tex);
            
            float x=c->pos.x+bx+0.5f, y=c->pos.y+by+0.5f, z=c->pos.z+bz+0.5f;
            float verts[36*5];
            for(int i=0;i<36*5;i++) {
                if(i%5<3) verts[i] = cube_verts[i] + (i%5==0?x:(i%5==1?y:z));
                else verts[i] = cube_verts[i];
            }
            glEnableVertexAttribArray(g->pos);
            glEnableVertexAttribArray(g->uv);
            glVertexAttribPointer(g->pos,3,GL_FLOAT,GL_FALSE,5*sizeof(float),verts);
            glVertexAttribPointer(g->uv,2,GL_FLOAT,GL_FALSE,5*sizeof(float),verts+3);
            glDrawArrays(GL_TRIANGLES,0,36);
        }
    }
    glDisableVertexAttribArray(g->pos);
    glDisableVertexAttribArray(g->uv);
}

static void game_touch(Game* g,float x,float y,int action) {
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

static void game_move(Game* g,float dx,float dy) {
    g->yaw+=dx*0.5f;
    g->pitch+=dy*0.5f;
    if(g->pitch>89) g->pitch=89;
    if(g->pitch<-89) g->pitch=-89;
}

// ===== MAIN =====
struct engine { struct android_app* app; EGLDisplay dpy; EGLSurface surf; EGLContext ctx; Game game; int init; int tid; float lx,ly; };

static int32_t input(struct android_app* app, AInputEvent* ev) {
    struct engine* e=(struct engine*)app->userData;
    if(AInputEvent_getType(ev)!=AINPUT_EVENT_TYPE_MOTION) return 0;
    int act=AMotionEvent_getAction(ev);
    int idx=(act&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    act&=AMOTION_EVENT_ACTION_MASK;
    float x=AMotionEvent_getX(ev,idx), y=AMotionEvent_getY(ev,idx);
    if(act==AMOTION_EVENT_ACTION_DOWN||act==AMOTION_EVENT_ACTION_POINTER_DOWN) {
        e->tid=AMotionEvent_getPointerId(ev,idx); e->lx=x; e->ly=y; game_touch(&e->game,x,y,0); return 1;
    } else if(act==AMOTION_EVENT_ACTION_MOVE) {
        for(int i=0;i<AMotionEvent_getPointerCount(ev);i++) if(AMotionEvent_getPointerId(ev,i)==e->tid) {
            float px=AMotionEvent_getX(ev,i), py=AMotionEvent_getY(ev,i);
            if(px>e->game.w*2/3) game_move(&e->game,px-e->lx,py-e->ly);
            e->lx=px; e->ly=py;
        } return 1;
    } else if(act==AMOTION_EVENT_ACTION_UP||act==AMOTION_EVENT_ACTION_POINTER_UP) {
        game_touch(&e->game,x,y,1); return 1;
    } return 0;
}

static void cmd(struct android_app* app, int32_t cmd) {
    struct engine* e=(struct engine*)app->userData;
    if(cmd==APP_CMD_INIT_WINDOW) {
        e->dpy=eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(e->dpy,0,0);
        EGLConfig cfg; EGLint n;
        EGLint att[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_BLUE_SIZE,8,EGL_GREEN_SIZE,8,EGL_RED_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE};
        eglChooseConfig(e->dpy,att,&cfg,1,&n);
        e->surf=eglCreateWindowSurface(e->dpy,cfg,app->window,0);
        EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
        e->ctx=eglCreateContext(e->dpy,cfg,0,ca);
        eglMakeCurrent(e->dpy,e->surf,e->surf,e->ctx);
        if(!e->init){ game_init(&e->game,app->activity->assetManager); e->init=1; }
    } else if(cmd==APP_CMD_TERM_WINDOW) {
        if(e->dpy!=EGL_NO_DISPLAY){ eglMakeCurrent(e->dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT); eglDestroyContext(e->dpy,e->ctx); eglDestroySurface(e->dpy,e->surf); eglTerminate(e->dpy); e->dpy=EGL_NO_DISPLAY; }
    }
}

void android_main(struct android_app* state) {
    struct engine e={0}; state->userData=&e; state->onAppCmd=cmd; state->onInputEvent=input; e.app=state;
    while(1){ int ev; struct android_poll_source* src; while(ALooper_pollOnce(e.dpy?0:-1,0,&ev,(void**)&src)>=0){ if(src) src->process(state,src); if(state->destroyRequested) return; } if(e.dpy){ game_update(&e.game); game_render(&e.game); eglSwapBuffers(e.dpy,e.surf); } }
}
