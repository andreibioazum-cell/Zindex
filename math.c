#include "math.h"

void m4_id(Mat4* m) { for(int i=0;i<16;i++) m->m[i]=(i%5==0)?1.0f:0.0f; }
void m4_mul(Mat4* r,Mat4* a,Mat4* b) {
    Mat4 t; for(int i=0;i<4;i++) for(int j=0;j<4;j++) {
        float s=0; for(int k=0;k<4;k++) s+=a->m[i*4+k]*b->m[k*4+j];
        t.m[i*4+j]=s;
    } *r=t;
}
void m4_persp(Mat4* m,float fov,float asp,float near,float far) {
    m4_id(m); float f=1.0f/tanf(fov*0.5f);
    m->m[0]=f/asp; m->m[5]=f; m->m[10]=(far+near)/(near-far); m->m[11]=-1.0f; m->m[14]=(2.0f*far*near)/(near-far); m->m[15]=0.0f;
}
void m4_look(Mat4* m,Vec3 eye,Vec3 target,Vec3 up) {
    Vec3 f=v3_norm(v3_sub(target,eye)), s=v3_norm(v3_cross(f,up)), u=v3_cross(s,f);
    m4_id(m);
    m->m[0]=s.x; m->m[4]=s.y; m->m[8]=s.z;
    m->m[1]=u.x; m->m[5]=u.y; m->m[9]=u.z;
    m->m[2]=-f.x; m->m[6]=-f.y; m->m[10]=-f.z;
    m->m[12]=-v3_dot(s,eye); m->m[13]=-v3_dot(u,eye); m->m[14]=v3_dot(f,eye);
}
