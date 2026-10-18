#include "rvk_smoke.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void smoke_log(void *user, int level, const char *message) {
    (void)user;
    fprintf(level >= 2 ? stderr : stdout, "%s\n", message);
    fflush(level >= 2 ? stderr : stdout);
}

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left;
    double b = *(const double *)right;

    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static double percentile(const double *values, size_t count, double fraction) {
    size_t index;

    if (count == 0) {
        return 0.0;
    }
    index = (size_t)(fraction * (double)(count - 1) + 0.5);
    if (index >= count) {
        index = count - 1;
    }
    return values[index];
}

static void print_usage(void) {
    printf("usage: render_vk_smoke [options]\n");
    printf("  --frames N       number of presented frames (default 300)\n");
    printf("  --width N        window width (default 640)\n");
    printf("  --height N       window height (default 480)\n");
    printf("  --validate       enable Khronos validation\n");
    printf("  --present MODE   fifo or immediate (default fifo)\n");
    printf("  --delay-ms N     sleep between frames (default 0)\n");
    printf("  --resize-test    resize the window twice while running\n");
    printf("  --acquire-ood-test  resize without notifying the renderer first\n");
    printf("  --world-test     draw a textured triangle and an overlay\n");
    printf("  --overlay-only   use the overlay without a world pass\n");
    printf("  --csv PATH       write per-frame timing CSV\n");
    printf("  --help           show this help\n");
}

int rvk_smoke_run(int argc, char **argv, const RvkSmokeHost *host) {
    uint32_t target_frames = 300;
    uint32_t width = 640;
    uint32_t height = 480;
    uint32_t delay_ms = 0;
    int enable_validation = 0;
    int resize_test = 0;
    int acquire_ood_test = 0;
    int world_test = 0;
    int overlay_only = 0;
    RvkPresentMode present_mode = RVK_PRESENT_FIFO;
    const char *csv_path = NULL;
    RvkSmokeHost backend;
    RvkPlatform platform;
    RvkInitInfo init;
    RvkDevice *device = NULL;
    RvkResult result;
    RvkTexture test_texture = 0;
    RvkVertex test_vertices[3];
    uint32_t test_indices[3];
    RvkWorldDraw test_draw;
    float test_view_proj[16];
    float test_model[16];
    uint32_t *test_overlay = NULL;
    double *samples = NULL;
    size_t sample_count = 0;
    FILE *csv = NULL;
    uint32_t stalls = 0;
    uint32_t out_of_date_frames = 0;
    int lost = 0;
    int i;

    if (host == NULL || host->init == NULL || host->shutdown == NULL || host->poll == NULL
        || host->sleep_ms == NULL || host->platform == NULL) {
        fprintf(stderr, "render_vk_smoke: invalid host\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            target_frames = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            width = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--delay-ms") == 0 && i + 1 < argc) {
            delay_ms = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--validate") == 0) {
            enable_validation = 1;
        } else if (strcmp(argv[i], "--resize-test") == 0) {
            resize_test = 1;
        } else if (strcmp(argv[i], "--acquire-ood-test") == 0) {
            acquire_ood_test = 1;
        } else if (strcmp(argv[i], "--world-test") == 0) {
            world_test = 1;
        } else if (strcmp(argv[i], "--overlay-only") == 0) {
            overlay_only = 1;
        } else if (strcmp(argv[i], "--present") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "fifo") == 0) {
                present_mode = RVK_PRESENT_FIFO;
            } else if (strcmp(argv[i], "immediate") == 0) {
                present_mode = RVK_PRESENT_IMMEDIATE;
            } else {
                fprintf(stderr, "render_vk_smoke: unknown present mode '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else {
            fprintf(stderr, "render_vk_smoke: unknown argument '%s'\n", argv[i]);
            print_usage();
            return 1;
        }
    }
    if (width == 0 || height == 0) {
        fprintf(stderr, "render_vk_smoke: width and height must be non-zero\n");
        return 1;
    }
    if ((resize_test || acquire_ood_test) && host->resize == NULL) {
        fprintf(stderr, "render_vk_smoke: this backend does not support resize tests\n");
        return 1;
    }

    backend = *host;
    if (backend.init(backend.user, width, height, "render_vk smoke") != 0) {
        fprintf(stderr, "render_vk_smoke: could not create the window\n");
        return 1;
    }
    platform = backend.platform(backend.user);

    memset(&init, 0, sizeof(init));
    init.struct_size = sizeof(init);
    init.app_name = "render_vk_smoke";
    init.width = width;
    init.height = height;
    init.platform = platform;
    init.enable_validation = enable_validation;
    init.present_mode = present_mode;
    init.log = smoke_log;

    result = rvk_create(&init, &device);
    if (result != RVK_OK) {
        fprintf(stderr, "render_vk_smoke: rvk_create failed: %s\n", rvk_result_string(result));
        backend.shutdown(backend.user);
        return result == RVK_ERROR_UNSUPPORTED ? 2 : 1;
    }

    printf("adapter: %s\n", rvk_get_adapter_name(device));
    printf("present mode: %s\n", present_mode == RVK_PRESENT_IMMEDIATE ? "immediate" : "fifo");

    memset(test_view_proj, 0, sizeof(test_view_proj));
    memset(test_model, 0, sizeof(test_model));
    test_view_proj[0] = 1.0f;
    test_view_proj[5] = 1.0f;
    test_view_proj[10] = 1.0f;
    test_view_proj[15] = 1.0f;
    test_model[0] = 1.0f;
    test_model[5] = 1.0f;
    test_model[10] = 1.0f;
    test_model[15] = 1.0f;
    memset(test_vertices, 0, sizeof(test_vertices));
    test_vertices[0].position[0] = -0.5f;
    test_vertices[0].position[1] = -0.5f;
    test_vertices[0].position[2] = 0.5f;
    test_vertices[0].normal[2] = 1.0f;
    test_vertices[0].uv[0] = 0.0f;
    test_vertices[0].uv[1] = 1.0f;
    test_vertices[1].position[0] = 0.5f;
    test_vertices[1].position[1] = -0.5f;
    test_vertices[1].position[2] = 0.5f;
    test_vertices[1].normal[2] = 1.0f;
    test_vertices[1].uv[0] = 1.0f;
    test_vertices[1].uv[1] = 1.0f;
    test_vertices[2].position[0] = 0.0f;
    test_vertices[2].position[1] = 0.5f;
    test_vertices[2].position[2] = 0.5f;
    test_vertices[2].normal[2] = 1.0f;
    test_vertices[2].uv[0] = 0.5f;
    test_vertices[2].uv[1] = 0.0f;
    test_indices[0] = 0;
    test_indices[1] = 1;
    test_indices[2] = 2;
    memset(&test_draw, 0, sizeof(test_draw));
    test_draw.index_count = 3;
    memcpy(test_draw.model, test_model, sizeof(test_model));
    test_draw.tint[0] = 1.0f;
    test_draw.tint[1] = 1.0f;
    test_draw.tint[2] = 1.0f;
    test_draw.tint[3] = 1.0f;
    if (world_test || overlay_only) {
        uint8_t test_pixels[16] = {
            255, 0, 0, 255, 0, 255, 0, 255,
            0, 0, 255, 255, 255, 255, 255, 255
        };
        uint32_t x;
        uint32_t y;

        result = rvk_create_texture(device, test_pixels, 2, 2, &test_texture);
        if (result != RVK_OK) {
            fprintf(stderr, "render_vk_smoke: texture creation failed: %s\n", rvk_result_string(result));
            rvk_destroy(device);
            backend.shutdown(backend.user);
            return 1;
        }
        test_draw.texture = test_texture;
        test_overlay = (uint32_t *)calloc((size_t)width * height, sizeof(uint32_t));
        if (test_overlay == NULL) {
            rvk_destroy_texture(device, test_texture);
            rvk_destroy(device);
            backend.shutdown(backend.user);
            return 1;
        }
        for (y = 0; y < height && y < 100; y++) {
            for (x = 0; x < width && x < 100; x++) {
                test_overlay[(size_t)y * width + x] = 0xFF0000FFu;
            }
        }
    }

    if (target_frames > 0) {
        samples = (double *)calloc(target_frames, sizeof(double));
        if (samples == NULL) {
            target_frames = 0;
        }
    }
    if (csv_path != NULL) {
        csv = fopen(csv_path, "w");
        if (csv == NULL) {
            fprintf(stderr, "render_vk_smoke: could not open %s\n", csv_path);
        } else {
            fprintf(csv, "frame,cpu_frame_ms,cpu_wait_ms,width,height,passes,validation_messages\n");
        }
    }

    while (target_frames > 0 && sample_count < target_frames) {
        int quit = 0;
        int resized = 0;
        uint32_t new_width = 0;
        uint32_t new_height = 0;
        RvkPassInfo pass_info;
        const RvkStats *stats;

        backend.poll(backend.user, &quit, &resized, &new_width, &new_height);
        if (quit) {
            break;
        }
        if (resized) {
            rvk_resize(device, new_width, new_height);
        }
        if (sample_count >= target_frames && target_frames > 0) {
            break;
        }
        if (acquire_ood_test && sample_count == 30) {
            backend.resize(backend.user, 800, 600);
        }

        result = rvk_begin_frame(device);
        if (result == RVK_ERROR_OUT_OF_DATE) {
            out_of_date_frames++;
            stalls++;
            backend.sleep_ms(backend.user, 5);
            if (stalls > 1000) {
                fprintf(stderr, "render_vk_smoke: too many out-of-date frames\n");
                break;
            }
            continue;
        }
        if (result != RVK_OK) {
            fprintf(stderr, "render_vk_smoke: rvk_begin_frame failed: %s\n", rvk_result_string(result));
            lost = result == RVK_ERROR_DEVICE_LOST;
            break;
        }

        if (world_test || overlay_only) {
            float clear_color[4] = { 0.05f, 0.10f, 0.16f, 1.0f };

            if (!overlay_only) {
                result = rvk_draw_world(device, test_view_proj, test_vertices, 3, test_indices, 3, &test_draw, 1, NULL, clear_color);
            } else {
                result = RVK_OK;
            }
            if (result == RVK_OK) {
                result = rvk_draw_overlay(device, test_overlay, width, height);
            }
            if (result == RVK_OK) {
                result = rvk_end_frame(device);
            }
        } else {
            memset(&pass_info, 0, sizeof(pass_info));
            pass_info.struct_size = sizeof(pass_info);
            pass_info.pass = RVK_PASS_MAIN;
            pass_info.clear = 1;
            pass_info.clear_color[0] = 0.05f;
            pass_info.clear_color[1] = 0.10f;
            pass_info.clear_color[2] = 0.16f;
            pass_info.clear_color[3] = 1.0f;

            result = rvk_begin_pass(device, &pass_info);
            if (result == RVK_OK) {
                rvk_end_pass(device);
                result = rvk_end_frame(device);
            }
        }
        if (result == RVK_ERROR_OUT_OF_DATE) {
            out_of_date_frames++;
            stalls++;
            backend.sleep_ms(backend.user, 5);
            continue;
        }
        if (result != RVK_OK) {
            fprintf(stderr, "render_vk_smoke: frame failed: %s\n", rvk_result_string(result));
            lost = result == RVK_ERROR_DEVICE_LOST;
            break;
        }

        stalls = 0;
        stats = rvk_get_stats(device);
        if (stats != NULL && samples != NULL && sample_count < target_frames) {
            samples[sample_count++] = stats->cpu_frame_ms;
            if (csv != NULL) {
                fprintf(csv, "%llu,%.3f,%.3f,%u,%u,%u,%u\n",
                    (unsigned long long)stats->frame_index,
                    stats->cpu_frame_ms,
                    stats->cpu_wait_ms,
                    stats->width,
                    stats->height,
                    stats->pass_count,
                    stats->validation_messages);
            }
        }
        if (stats != NULL && (stats->frame_index % 60) == 0) {
            printf("frame %llu: cpu %.3f ms, wait %.3f ms, %ux%u\n",
                (unsigned long long)stats->frame_index,
                stats->cpu_frame_ms,
                stats->cpu_wait_ms,
                stats->width,
                stats->height);
        }
        if (resize_test && sample_count == 30) {
            backend.resize(backend.user, 800, 600);
        } else if (resize_test && sample_count == 60) {
            backend.resize(backend.user, 640, 480);
        }
        if (delay_ms > 0) {
            backend.sleep_ms(backend.user, delay_ms);
        }
    }

    if (csv != NULL) {
        fclose(csv);
    }
    printf("out-of-date frames: %u\n", out_of_date_frames);
    if (samples != NULL && sample_count > 0) {
        double sum = 0.0;
        size_t index;

        qsort(samples, sample_count, sizeof(double), compare_double);
        for (index = 0; index < sample_count; index++) {
            sum += samples[index];
        }
        printf("frames: %zu, mean %.3f ms, p50 %.3f ms, p95 %.3f ms, p99 %.3f ms, min %.3f ms, max %.3f ms\n",
            sample_count,
            sum / (double)sample_count,
            percentile(samples, sample_count, 0.50),
            percentile(samples, sample_count, 0.95),
            percentile(samples, sample_count, 0.99),
            samples[0],
            samples[sample_count - 1]);
    }
    if (device != NULL) {
        const RvkStats *stats = rvk_get_stats(device);

        if (stats != NULL) {
            printf("validation messages: %u\n", stats->validation_messages);
            if (enable_validation && stats->validation_messages > 0) {
                rvk_destroy(device);
                free(samples);
                backend.shutdown(backend.user);
                return 4;
            }
        }
    }
    rvk_destroy(device);
    free(test_overlay);
    free(samples);
    backend.shutdown(backend.user);
    return lost ? 3 : 0;
}
