#pragma once
#include <EGL/egl.h>

inline void* (*getPrimaryWindow)();
inline bool  (*isMouseLocked)(void* handle);
inline void  (*addSwapBuffersCallback)(void* user, void (*callback)(void* user, EGLDisplay display, EGLSurface surface));
