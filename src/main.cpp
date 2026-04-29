#include "gamewindow.hpp"
#include "menu.hpp"
#include "motionblur.hpp"
#include <dlfcn.h>

extern "C" [[gnu::visibility("default")]] void mod_preinit() {
    auto gwLib = dlopen("libmcpelauncher_gamewindow.so", 0);

    getPrimaryWindow       = reinterpret_cast<decltype(getPrimaryWindow)>(dlsym(gwLib, "game_window_get_primary_window"));
    isMouseLocked          = reinterpret_cast<decltype(isMouseLocked)>(dlsym(gwLib, "game_window_is_mouse_locked"));
    addSwapBuffersCallback = reinterpret_cast<decltype(addSwapBuffersCallback)>(dlsym(gwLib, "game_window_add_swap_buffers_callback"));

    auto menuLib = dlopen("libmcpelauncher_menu.so", 0);

    addMenu     = reinterpret_cast<decltype(addMenu)>(dlsym(menuLib, "mcpelauncher_addmenu"));
    showWindow  = reinterpret_cast<decltype(showWindow)>(dlsym(menuLib, "mcpelauncher_show_window"));
    closeWindow = reinterpret_cast<decltype(closeWindow)>(dlsym(menuLib, "mcpelauncher_close_window"));

    initMotionBlur();

    addSwapBuffersCallback(nullptr, onSwapBuffers);
}

extern "C" [[gnu::visibility("default")]] void mod_init() {}
