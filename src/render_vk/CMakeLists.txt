add_library(render_vk STATIC
    src/rvk_api.c
    src/rvk_context.c
    src/rvk_draw.c
    src/rvk_frame.c
    src/rvk_pipeline.c
    src/rvk_resources.c
    src/rvk_swapchain.c
)

target_include_directories(render_vk
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    SYSTEM PRIVATE
        "${DETHRACE_VULKAN_INCLUDE_DIR}"
)
target_compile_definitions(render_vk PUBLIC VK_NO_PROTOTYPES)
set_target_properties(render_vk PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(render_vk PRIVATE /W4)
else()
    target_compile_options(render_vk PRIVATE -Wall -Wextra -Wstrict-prototypes)
endif()

add_library(render_vk_smoke_common STATIC smoke/rvk_smoke.c)
target_link_libraries(render_vk_smoke_common PUBLIC render_vk)
target_include_directories(render_vk_smoke_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/smoke")
set_target_properties(render_vk_smoke_common PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

if(DETHRACE_PLATFORM_SDL2 AND TARGET SDL2::SDL2)
    add_executable(render_vk_smoke_sdl2 smoke/rvk_smoke_sdl2.c)
    target_include_directories(render_vk_smoke_sdl2 SYSTEM PRIVATE "${DETHRACE_VULKAN_INCLUDE_DIR}")
    target_compile_definitions(render_vk_smoke_sdl2 PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(render_vk_smoke_sdl2 PRIVATE render_vk_smoke_common SDL2::SDL2)
    set_target_properties(render_vk_smoke_sdl2 PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    if(WIN32)
        add_custom_command(TARGET render_vk_smoke_sdl2 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL2::SDL2>"
                "$<TARGET_FILE_DIR:render_vk_smoke_sdl2>"
        )
    endif()
endif()

if(DETHRACE_PLATFORM_SDL3 AND TARGET SDL3::SDL3)
    add_executable(render_vk_smoke_sdl3 smoke/rvk_smoke_sdl3.c)
    target_include_directories(render_vk_smoke_sdl3 SYSTEM PRIVATE "${DETHRACE_VULKAN_INCLUDE_DIR}")
    target_compile_definitions(render_vk_smoke_sdl3 PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(render_vk_smoke_sdl3 PRIVATE render_vk_smoke_common SDL3::SDL3)
    set_target_properties(render_vk_smoke_sdl3 PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    if(WIN32)
        add_custom_command(TARGET render_vk_smoke_sdl3 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>"
                "$<TARGET_FILE_DIR:render_vk_smoke_sdl3>"
        )
    endif()
endif()
