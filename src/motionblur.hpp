#pragma once
#include <EGL/egl.h>

void initMotionBlur();

void onSwapBuffers(void* user, EGLDisplay display, EGLSurface surface);
