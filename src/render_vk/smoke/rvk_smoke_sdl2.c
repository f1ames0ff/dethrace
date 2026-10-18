#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <string.h>

#include "rvk_smoke.h"

typedef struct RvkSdl2State {
    SDL_Window *window;
} RvkSdl2State;

static int sdl2_load_vulkan(void *user) {
    (void)user;
    return SDL_Vulkan_LoadLibrary(NULL) == 0 ? 0 : 1;
}

static void sdl2_unload_vulkan(void *user) {
    (void)user;
    SDL_Vulkan_UnloadLibrary();
}

static RvkGetInstanceProcAddrFn sdl2_get_instance_proc_addr(void *user) {
    (void)user;
    return (RvkGetInstanceProcAddrFn)SDL_Vulkan_GetVkGetInstanceProcAddr();
}

static int sdl2_get_instance_extensions(void *user, uint32_t *count, const char **names, uint32_t max_names) {
    RvkSdl2State *state = (RvkSdl2State *)user;
    unsigned int sdl_count = 0;

    if (SDL_Vulkan_GetInstanceExtensions(state->window, &sdl_count, NULL) == SDL_FALSE) {
        return 1;
    }
    if (sdl_count > max_names) {
        return 1;
    }
    if (SDL_Vulkan_GetInstanceExtensions(state->window, &sdl_count, names) == SDL_FALSE) {
        return 1;
    }
    *count = sdl_count;
    return 0;
}

static int sdl2_create_surface(void *user, uint64_t instance, uint64_t *surface) {
    RvkSdl2State *state = (RvkSdl2State *)user;
    VkSurfaceKHR created = VK_NULL_HANDLE;

    if (SDL_Vulkan_CreateSurface(state->window, (VkInstance)instance, &created) == SDL_FALSE) {
        return 1;
    }
    *surface = (uint64_t)created;
    return 0;
}

static const char *sdl2_get_error(void *user) {
    (void)user;
    return SDL_GetError();
}

static int sdl2_init(void *user, uint32_t width, uint32_t height, const char *title) {
    RvkSdl2State *state = (RvkSdl2State *)user;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1;
    }
    state->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)width, (int)height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (state->window == NULL) {
        SDL_Quit();
        return 1;
    }
    return 0;
}

static void sdl2_shutdown(void *user) {
    RvkSdl2State *state = (RvkSdl2State *)user;

    if (state->window != NULL) {
        SDL_DestroyWindow(state->window);
        state->window = NULL;
    }
    SDL_Quit();
}

static int sdl2_poll(void *user, int *quit, int *resize, uint32_t *width, uint32_t *height) {
    RvkSdl2State *state = (RvkSdl2State *)user;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            *quit = 1;
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                *quit = 1;
            } else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED
                || event.window.event == SDL_WINDOWEVENT_RESIZED
                || event.window.event == SDL_WINDOWEVENT_MINIMIZED
                || event.window.event == SDL_WINDOWEVENT_RESTORED) {
                int window_width = 0;
                int window_height = 0;

                SDL_GetWindowSize(state->window, &window_width, &window_height);
                *resize = 1;
                *width = (uint32_t)(window_width > 0 ? window_width : 0);
                *height = (uint32_t)(window_height > 0 ? window_height : 0);
            }
        }
    }
    return 0;
}

static void sdl2_sleep(void *user, uint32_t milliseconds) {
    (void)user;
    SDL_Delay(milliseconds);
}

static void sdl2_resize(void *user, uint32_t width, uint32_t height) {
    RvkSdl2State *state = (RvkSdl2State *)user;

    SDL_SetWindowSize(state->window, (int)width, (int)height);
}

static RvkPlatform sdl2_platform(void *user) {
    RvkPlatform platform;

    memset(&platform, 0, sizeof(platform));
    platform.user = user;
    platform.load_vulkan = sdl2_load_vulkan;
    platform.unload_vulkan = sdl2_unload_vulkan;
    platform.get_instance_proc_addr = sdl2_get_instance_proc_addr;
    platform.get_instance_extensions = sdl2_get_instance_extensions;
    platform.create_surface = sdl2_create_surface;
    platform.get_error = sdl2_get_error;
    return platform;
}

int main(int argc, char **argv) {
    RvkSdl2State state;
    RvkSmokeHost host;

    memset(&state, 0, sizeof(state));
    memset(&host, 0, sizeof(host));
    host.user = &state;
    host.init = sdl2_init;
    host.shutdown = sdl2_shutdown;
    host.poll = sdl2_poll;
    host.sleep_ms = sdl2_sleep;
    host.resize = sdl2_resize;
    host.platform = sdl2_platform;
    return rvk_smoke_run(argc, argv, &host);
}
