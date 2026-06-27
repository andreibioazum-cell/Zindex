#ifndef MATH_H
#define MATH_H

#include <math.h>

typedef struct { float x,y,z; } Vec3;
typedef struct { float m[16]; } Mat4;

static inline Vec3 v3(float x,float y,float z) { Vec3 v={x,y,z}; return v; }
static inline Vec3 v3_add(Vec3 a,Vec3 b) { return v3(a.x+b.x,a.y+b.y,a.z+b.z); }
static inline Vec3 v3_sub(Vec3 a,Vec3 b) { return v3(a.x-b.x,a.y-b.y,a.z-b.z); }
static inline Vec3 v3_scale(Vec3 a,float s) { return v3(a.x*s,a.y*s,a.z*s); }
static inline float v3_dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static inline float v3_len(Vec3 a) { return sqrtf(v3_dot(a,a)); }
static inline Vec3 v3_norm(Vec3 a) { float l=v3_len(a); return l?v3_scale(a,1.0f/l):v3(0,0,0); }
static inline Vec3 v3_cross(Vec3 a,Vec3 b) { return v3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }

void m4_id(Mat4* m);
void m4_mul(Mat4* r,Mat4* a,Mat4* b);
void m4_persp(Mat4* m,float fov,float asp,float near,float far);
void m4_look(Mat4* m,Vec3 eye,Vec3 target,Vec3 up);

#endif
