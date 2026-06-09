#ifndef LIBC_MATH_H
#define LIBC_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

double sqrt(double x);
double fabs(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double floor(double x);
double ceil(double x);
double ldexp(double x, int exp);
double pow(double x, double y);
double frexp(double x, int* exp);
double fmod(double x, double y);
float sqrtf(float x);
float fabsf(float x);
float atan2f(float y, float x);
float fmaxf(float a, float b);
float asinf(float x);
float acosf(float x);
float sinf(float x);
float cosf(float x);
float floorf(float x);
float fminf(float a, float b);
float ldexpf(float x, int exp);
float powf(float x, float y);
float frexpf(float x, int* exp);
float fmodf(float x, float y);

#ifdef __cplusplus
}
#endif

#endif
