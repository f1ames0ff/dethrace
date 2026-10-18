#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#include "rvk_smoke.h"

#if SDL_VERSION_ATLEAST(3, 2, 0)
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#endif

typedef struct RvkSdl3State {
    SDL_Window *window;
} RvkSdl3State;

#if SDL_VERSION_ATLEAST(3, 2, 0)

static int sdl3_load_vulkan(void *user) {
    (void)user;
    return SDL_Vulkan_LoadLibrary(NULL) ? 0 : 1;
}

static void sdl3_unload_vulkan(void *user) {
    (void)user;
    SDL_Vulkan_UnloadLibrary();
}

static RvkGetInstanceProcAddrFn sdl3_get_instance_proc_addr(void *user) {
    (void)user;
    return (RvkGetInstanceProcAddrFn)SDL_Vulkan_GetVkGetInstanceProcAddr();
}

static int sdl3_get_instance_extensions(void *user, uint32_t *count, const char **names, uint32_t max_names) {
    const char *const *sdl_names;
    Uint32 sdl_count = 0;
    Uint32 i;

    (void)user;
    sdl_names = SDL_Vulkan_GetInstanceExtensions(&sdl_count);
    if (sdl_names == NULL || sdl_count > max_names) {
        return 1;
    }
    for (i = 0; i < sdl_count; i++) {
        names[i] = sdl_names[i];
    }
    *count = sdl_count;
    return 0;
}

static int sdl3_create_surface(void *user, uint64_t instance, uint64_t *surface) {
    RvkSdl3State *state = (RvkSdl3State *)user;
    VkSurfaceKHR created = VK_NULL_HANDLE;

    if (!SDL_Vulkan_CreateSurface(state->window, (VkInstance)instance, NULL, &created)) {
        return 1;
    }
    *surface = (uint64_t)created;
    return 0;
}

static const char *sdl3_get_error(void *user) {
    (void)user;
    return SDL_GetError();
}

static int sdl3_init(void *user, uint32_t width, uint32_t height, const char *title) {
    RvkSdl3State *state = (RvkSdl3State *)user;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }
    state->window = SDL_CreateWindow(title, (int)width, (int)height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (state->window == NULL) {
        SDL_Quit();
        return 1;
    }
    return 0;
}

static void sdl3_shutdown(void *user) {
    RvkSdl3State *state = (RvkSdl3State *)user;

    if (state->window != NULL) {
        SDL_DestroyWindow(state->window);
        state->window = NULL;
    }
    SDL_Quit();
}

static int sdl3_poll(void *user, int *quit, int *resize, uint32_t *width, uint32_t *height) {
    RvkSdl3State *state = (RvkSdl3State *)user;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            *quit = 1;
        } else if (event.type == SDL_EVENT_WINDOW_RESIZED
            || event.type == SDL_EVENT_WINDOW_MINIMIZED
            || event.type == SDL_EVENT_WINDOW_RESTORED) {
            int window_width = 0;
            int window_height = 0;

            SDL_GetWindowSize(state->window, &window_width, &window_height);
            *resize = 1;
            *width = (uint32_t)(window_width > 0 ? window_width : 0);
            *height = (uint32_t)(window_height > 0 ? window_height : 0);
        }
    }
    return 0;
}

static void sdl3_sleep(void *user, uint32_t milliseconds) {
    (void)user;
    SDL_Delay(milliseconds);
}

static void sdl3_resize(void *user, uint32_t width, uint32_t height) {
    RvkSdl3State *state = (RvkSdl3State *)user;

    SDL_SetWindowSize(state->window, (int)width, (int)height);
}

static RvkPlatform sdl3_platform(void *user) {
    RvkPlatform platform;

    memset(&platform, 0, sizeof(platform));
    platform.user = user;
    platform.load_vulkan = sdl3_load_vulkan;
    platform.unload_vulkan = sdl3_unload_vulkan;
    platform.get_instance_proc_addr = sdl3_get_instance_proc_addr;
    platform.get_instance_extensions = sdl3_get_instance_extensions;
    platform.create_surface = sdl3_create_surface;
    platform.get_error = sdl3_get_error;
    return platform;
}

int main(int argc, char **argv) {
    RvkSdl3State state;
    RvkSmokeHost host;

    memset(&state, 0, sizeof(state));
    memset(&host, 0, sizeof(host));
    host.user = &state;
    host.init = sdl3_init;
    host.shutdown = sdl3_shutdown;
    host.poll = sdl3_poll;
    host.sleep_ms = sdl3_sleep;
    host.resize = sdl3_resize;
    host.platform = sdl3_platform;
    return rvk_smoke_run(argc, argv, &host);
}

#else

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "render_vk_smoke: SDL3 3.2.0 or newer is required\n");
    return 2;
}

#endif
