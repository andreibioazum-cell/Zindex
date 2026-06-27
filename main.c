#include <android_native_app_glue.h>
#include "game.h"

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
    while(1){ int ev; struct android_poll_source* src; while(ALooper_pollAll(e.dpy?0:-1,0,&ev,(void**)&src)>=0){ if(src) src->process(state,src); if(state->destroyRequested) return; } if(e.dpy){ game_update(&e.game); game_render(&e.game); eglSwapBuffers(e.dpy,e.surf); } }
}
