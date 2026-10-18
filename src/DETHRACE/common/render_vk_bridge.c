#include "render_vk_bridge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "depth.h"
#include "globvars.h"
#include "graphics.h"
#include "harness/hooks.h"
#include "pedestrn.h"
#include "harness/trace.h"
#include "pd/sys.h"
#include "v1db_p.h"
#include "formats.h"

#define GAME_RENDER_MAX_VERTICES 4000000u
#define GAME_RENDER_MAX_INDICES 12000000u
#define GAME_RENDER_MAX_DRAWS 65536u
#define GAME_RENDER_MAX_TEXTURES 1024u
#define GAME_RENDER_MAX_OVERLAY_PIXELS 67108864u
#define GAME_RENDER_INITIAL_VERTICES 4096u
#define GAME_RENDER_INITIAL_INDICES 12288u
#define GAME_RENDER_INITIAL_DRAWS 256u
#define GAME_RENDER_TRANSPARENT_INDEX 0u

typedef struct GameRenderTextureEntry {
    br_pixelmap *source;
    RvkTexture texture;
    int owned;
} GameRenderTextureEntry;

static RvkDevice *gRenderVk_device;
static int gRenderVk_active;
static int gRenderVk_white_texture_valid;
static RvkTexture gRenderVk_white_texture;

static RvkVertex *gRenderVk_vertices;
static uint32_t gRenderVk_vertex_count;
static uint32_t gRenderVk_vertex_capacity;
static uint32_t *gRenderVk_indices;
static uint32_t gRenderVk_index_count;
static uint32_t gRenderVk_index_capacity;
static RvkWorldDraw *gRenderVk_draws;
static uint32_t gRenderVk_draw_count;
static uint32_t gRenderVk_draw_capacity;
static uint32_t gRenderVk_draw_flags;
static uint32_t *gRenderVk_overlay;
static size_t gRenderVk_overlay_capacity;
static float gRenderVk_view_proj[16];
static int gRenderVk_scene_ready;
static br_actor *gRenderVk_camera;
static br_angle gRenderVk_sky_base_yaw;
static int gRenderVk_sky_base_yaw_valid;
static RvkBackgroundDraw gRenderVk_background;
static int gRenderVk_background_valid;

static GameRenderTextureEntry *gRenderVk_textures;
static uint32_t gRenderVk_texture_count;
static uint32_t gRenderVk_texture_capacity;

static int gRenderVk_begin_error_logged;
static int gRenderVk_draw_error_logged;
static int gRenderVk_texture_error_logged;
static int gRenderVk_texture_limit_logged;
static int gRenderVk_resize_error_logged;

static int GameRender_EqualsIgnoreCase(const char *left, const char *right) {
    char a;
    char b;

    while (*left != '\0' && *right != '\0') {
        a = *left++;
        b = *right++;
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return *left == '\0' && *right == '\0';
}

static void GameRender_LogCallback(void *user, int level, const char *message) {
    (void)user;
    fprintf(stderr, "render_vk[%d]: %s\n", level, message != NULL ? message : "");
}

static int GameRender_RowBytes(const br_pixelmap *pm, uint32_t bytes_per_pixel) {
    if (pm->row_bytes != 0) {
        return pm->row_bytes;
    }
    return (int)((uint32_t)pm->width * bytes_per_pixel);
}

static uint8_t *GameRender_RowPointer(br_pixelmap *pm, uint32_t y, uint32_t bytes_per_pixel) {
    int row_bytes = GameRender_RowBytes(pm, bytes_per_pixel);

    return (uint8_t *)pm->pixels + ((int)pm->base_y + (int)y) * row_bytes + (int)pm->base_x * (int)bytes_per_pixel;
}

static uint8_t GameRender_Expand5(uint32_t value) {
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t GameRender_Expand6(uint32_t value) {
    return (uint8_t)((value << 2) | (value >> 4));
}

static int GameRender_PaletteColour(br_pixelmap *palette, uint32_t index, uint32_t *colour) {
    uint8_t *base;
    uint32_t entries;
    uint32_t x;
    uint32_t y;
    uint32_t value;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    int row_bytes;
    int bytes_per_pixel;

    if (palette == NULL || palette->pixels == NULL || palette->width == 0 || palette->height == 0) {
        return 0;
    }
    entries = (uint32_t)palette->width * (uint32_t)palette->height;
    if (index >= entries) {
        return 0;
    }
    switch (palette->type) {
    case BR_PMT_RGB_565:
        bytes_per_pixel = 2;
        break;
    case BR_PMT_RGB_888:
        bytes_per_pixel = 3;
        break;
    case BR_PMT_RGBX_888:
    case BR_PMT_RGBA_8888:
    case BR_PMT_ARGB_8888:
        bytes_per_pixel = 4;
        break;
    default:
        return 0;
    }
    row_bytes = GameRender_RowBytes(palette, (uint32_t)bytes_per_pixel);
    x = index % palette->width;
    y = index / palette->width;
    base = (uint8_t *)palette->pixels + ((int)palette->base_y + (int)y) * row_bytes + ((int)palette->base_x + (int)x) * bytes_per_pixel;

    switch (palette->type) {
    case BR_PMT_RGB_565:
        value = (uint32_t)base[0] | ((uint32_t)base[1] << 8);
        red = (value >> 11) & 31u;
        green = (value >> 5) & 63u;
        blue = value & 31u;
        *colour = ((uint32_t)GameRender_Expand5(red) << 16) | ((uint32_t)GameRender_Expand6(green) << 8) | (uint32_t)GameRender_Expand5(blue);
        return 1;
    case BR_PMT_RGB_888:
        *colour = ((uint32_t)base[2] << 16) | ((uint32_t)base[1] << 8) | (uint32_t)base[0];
        return 1;
    case BR_PMT_RGBX_888:
    case BR_PMT_RGBA_8888:
    case BR_PMT_ARGB_8888:
        *colour = ((uint32_t)base[2] << 16) | ((uint32_t)base[1] << 8) | (uint32_t)base[0];
        return 1;
    default:
        return 0;
    }
}

static int GameRender_ConvertPixelmap(br_pixelmap *pm, uint8_t *rgba8) {
    uint8_t *row;
    uint8_t *texel;
    uint8_t *out;
    br_pixelmap *palette;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t colour;
    uint32_t value;

    if (pm == NULL || rgba8 == NULL || pm->pixels == NULL || pm->width == 0 || pm->height == 0) {
        return 0;
    }
    width = pm->width;
    height = pm->height;
    out = rgba8;

    switch (pm->type) {
    case BR_PMT_INDEX_8:
        palette = pm->map != NULL ? pm->map : gCurrent_palette;
        if (palette == NULL || palette->pixels == NULL) {
            return 0;
        }
        for (y = 0; y < height; y++) {
            row = GameRender_RowPointer(pm, y, 1);
            for (x = 0; x < width; x++) {
                if (!GameRender_PaletteColour(palette, row[x], &colour)) {
                    return 0;
                }
                *out++ = (uint8_t)((colour >> 16) & 0xFFu);
                *out++ = (uint8_t)((colour >> 8) & 0xFFu);
                *out++ = (uint8_t)(colour & 0xFFu);
                *out++ = 255;
            }
        }
        return 1;
    case BR_PMT_RGB_565:
        for (y = 0; y < height; y++) {
            row = GameRender_RowPointer(pm, y, 2);
            for (x = 0; x < width; x++) {
                value = (uint32_t)row[x * 2] | ((uint32_t)row[x * 2 + 1] << 8);
                *out++ = GameRender_Expand5((value >> 11) & 31u);
                *out++ = GameRender_Expand6((value >> 5) & 63u);
                *out++ = GameRender_Expand5(value & 31u);
                *out++ = 255;
            }
        }
        return 1;
    case BR_PMT_RGB_888:
        for (y = 0; y < height; y++) {
            row = GameRender_RowPointer(pm, y, 3);
            for (x = 0; x < width; x++) {
                texel = row + x * 3;
                *out++ = texel[2];
                *out++ = texel[1];
                *out++ = texel[0];
                *out++ = 255;
            }
        }
        return 1;
    case BR_PMT_RGBX_888:
    case BR_PMT_RGBA_8888:
        for (y = 0; y < height; y++) {
            row = GameRender_RowPointer(pm, y, 4);
            for (x = 0; x < width; x++) {
                texel = row + x * 4;
                *out++ = texel[2];
                *out++ = texel[1];
                *out++ = texel[0];
                *out++ = pm->type == BR_PMT_RGBA_8888 ? texel[3] : 255;
            }
        }
        return 1;
    default:
        return 0;
    }
}

static void GameRender_StoreMatrix34(float out[16], const br_matrix34 *matrix) {
    int row;
    int column;

    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            out[column * 4 + row] = (float)matrix->m[column][row];
        }
        out[row * 4 + 3] = 0.0f;
    }
    out[12] = (float)matrix->m[3][0];
    out[13] = (float)matrix->m[3][1];
    out[14] = (float)matrix->m[3][2];
    out[15] = 1.0f;
}

static void *GameRender_ReserveMemory(void *buffer, uint32_t *capacity, uint32_t needed, uint32_t maximum, size_t element_size) {
    uint32_t new_capacity;
    void *new_buffer;

    if (needed <= *capacity) {
        return buffer;
    }
    if (needed > maximum) {
        return NULL;
    }
    new_capacity = *capacity == 0 ? 256u : *capacity;
    while (new_capacity < needed) {
        if (new_capacity > maximum / 2u) {
            new_capacity = maximum;
            break;
        }
        new_capacity *= 2u;
    }
    new_buffer = realloc(buffer, (size_t)new_capacity * element_size);
    if (new_buffer == NULL) {
        return NULL;
    }
    *capacity = new_capacity;
    return new_buffer;
}

static int GameRender_ReserveOverlay(size_t pixels) {
    size_t new_capacity;
    uint32_t *new_buffer;

    if (pixels <= gRenderVk_overlay_capacity) {
        return 1;
    }
    if (pixels > (size_t)GAME_RENDER_MAX_OVERLAY_PIXELS) {
        return 0;
    }
    new_capacity = gRenderVk_overlay_capacity == 0 ? 4096u : gRenderVk_overlay_capacity;
    while (new_capacity < pixels) {
        if (new_capacity > (size_t)GAME_RENDER_MAX_OVERLAY_PIXELS / 2u) {
            new_capacity = (size_t)GAME_RENDER_MAX_OVERLAY_PIXELS;
            break;
        }
        new_capacity *= 2u;
    }
    new_buffer = (uint32_t *)realloc(gRenderVk_overlay, new_capacity * sizeof(uint32_t));
    if (new_buffer == NULL) {
        return 0;
    }
    gRenderVk_overlay = new_buffer;
    gRenderVk_overlay_capacity = new_capacity;
    return 1;
}

static int GameRender_ReserveTextures(void) {
    uint32_t new_capacity;
    GameRenderTextureEntry *new_entries;

    if (gRenderVk_texture_count < gRenderVk_texture_capacity) {
        return 1;
    }
    if (gRenderVk_texture_capacity >= GAME_RENDER_MAX_TEXTURES) {
        return 0;
    }
    new_capacity = gRenderVk_texture_capacity == 0 ? 64u : gRenderVk_texture_capacity * 2u;
    if (new_capacity > GAME_RENDER_MAX_TEXTURES) {
        new_capacity = GAME_RENDER_MAX_TEXTURES;
    }
    new_entries = (GameRenderTextureEntry *)realloc(gRenderVk_textures, (size_t)new_capacity * sizeof(GameRenderTextureEntry));
    if (new_entries == NULL) {
        return 0;
    }
    gRenderVk_textures = new_entries;
    gRenderVk_texture_capacity = new_capacity;
    return 1;
}

static RvkTexture GameRender_CreateSolidTexture(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
    uint8_t rgba[4];
    RvkTexture texture = 0;
    RvkResult result;

    rgba[0] = red;
    rgba[1] = green;
    rgba[2] = blue;
    rgba[3] = alpha;
    result = rvk_create_texture(gRenderVk_device, rgba, 1, 1, &texture);
    if (result != RVK_OK) {
        return gRenderVk_white_texture;
    }
    return texture;
}

static RvkTexture GameRender_CreatePixelmapTexture(br_pixelmap *pm) {
    uint8_t *rgba;
    size_t bytes;
    RvkTexture texture = 0;
    RvkResult result;

    if (pm == NULL) {
        return gRenderVk_white_texture;
    }
    if (pm->width == 0 || pm->height == 0 || pm->pixels == NULL) {
        return GameRender_CreateSolidTexture(255, 0, 255, 255);
    }
    if ((size_t)pm->width > ((size_t)-1) / 4u / (size_t)pm->height) {
        return gRenderVk_white_texture;
    }
    bytes = (size_t)pm->width * (size_t)pm->height * 4u;
    rgba = (uint8_t *)malloc(bytes);
    if (rgba == NULL) {
        return gRenderVk_white_texture;
    }
    if (!GameRender_ConvertPixelmap(pm, rgba)) {
        free(rgba);
        return GameRender_CreateSolidTexture(255, 0, 255, 255);
    }
    result = rvk_create_texture(gRenderVk_device, rgba, pm->width, pm->height, &texture);
    free(rgba);
    if (result != RVK_OK) {
        if (!gRenderVk_texture_error_logged) {
            LOG_WARN("render_vk: texture creation failed: %s\n", rvk_result_string(result));
            gRenderVk_texture_error_logged = 1;
        }
        return gRenderVk_white_texture;
    }
    return texture;
}

static RvkTexture GameRender_GetTexture(br_pixelmap *pm) {
    GameRenderTextureEntry *entry;
    uint32_t i;
    RvkTexture texture;

    if (gRenderVk_device == NULL) {
        return 0;
    }
    if (pm == NULL) {
        return gRenderVk_white_texture;
    }
    for (i = 0; i < gRenderVk_texture_count; i++) {
        if (gRenderVk_textures[i].source == pm) {
            return gRenderVk_textures[i].texture;
        }
    }
    if (gRenderVk_texture_count >= GAME_RENDER_MAX_TEXTURES) {
        if (!gRenderVk_texture_limit_logged) {
            LOG_WARN("render_vk: texture cache limit reached\n");
            gRenderVk_texture_limit_logged = 1;
        }
        return gRenderVk_white_texture;
    }
    if (!GameRender_ReserveTextures()) {
        LOG_WARN("render_vk: texture cache allocation failed\n");
        return gRenderVk_white_texture;
    }
    texture = GameRender_CreatePixelmapTexture(pm);
    entry = &gRenderVk_textures[gRenderVk_texture_count++];
    entry->source = pm;
    entry->texture = texture;
    entry->owned = texture != 0 && texture != gRenderVk_white_texture;
    return texture;
}

static int GameRender_EmitRun(const float model[16], br_material *material, uint32_t first_index, uint32_t index_count) {
    RvkWorldDraw *draw;
    void *reserved;
    uint32_t colour;

    if (index_count == 0) {
        return 1;
    }
    reserved = GameRender_ReserveMemory(gRenderVk_draws, &gRenderVk_draw_capacity, gRenderVk_draw_count + 1u, GAME_RENDER_MAX_DRAWS, sizeof(RvkWorldDraw));
    if (reserved == NULL) {
        LOG_WARN("render_vk: draw limit reached\n");
        return 0;
    }
    gRenderVk_draws = (RvkWorldDraw *)reserved;
    draw = &gRenderVk_draws[gRenderVk_draw_count++];
    draw->first_index = first_index;
    draw->index_count = index_count;
    draw->flags = gRenderVk_draw_flags;
    memcpy(draw->model, model, sizeof(draw->model));
    draw->texture = GameRender_GetTexture(material != NULL ? material->colour_map : NULL);
    draw->tint[0] = 1.0f;
    draw->tint[1] = 1.0f;
    draw->tint[2] = 1.0f;
    draw->tint[3] = 1.0f;
    if (material != NULL) {
        draw->tint[3] = (float)material->opacity / 255.0f;
        if ((gRenderVk_draw_flags & RVK_DRAW_FLAG_SKY) == 0 && material->colour_map != NULL
            && (material->flags & BR_MATF_DISABLE_COLOUR_KEY) == 0) {
            draw->flags |= RVK_DRAW_FLAG_COLOUR_KEY;
        }
        if (material->colour_map == NULL) {
            colour = material->colour;
            draw->tint[0] = (float)((colour >> 16) & 0xFFu) / 255.0f;
            draw->tint[1] = (float)((colour >> 8) & 0xFFu) / 255.0f;
            draw->tint[2] = (float)(colour & 0xFFu) / 255.0f;
        }
    }
    return 1;
}

static void GameRender_AddPreparedModel(br_actor *actor, br_actor *universe, struct v11model *prepared, br_material *actor_material) {
    br_matrix34 model34;
    float model_matrix[16];
    uint32_t group_index;

    if (prepared == NULL || prepared->groups == NULL || prepared->ngroups == 0) {
        return;
    }
    BrActorToActorMatrix34(&model34, actor, universe);
    GameRender_StoreMatrix34(model_matrix, &model34);

    for (group_index = 0; group_index < prepared->ngroups; group_index++) {
        struct v11group *group = &prepared->groups[group_index];
        uint32_t base;
        uint32_t i;
        uint32_t first_index;
        uint32_t emitted = 0;
        void *reserved;
        br_material *material;

        if (group->position == NULL || group->vertex_numbers == NULL || group->nvertices == 0 || group->nfaces == 0) {
            continue;
        }
        if (gRenderVk_vertex_count + (uint32_t)group->nvertices > GAME_RENDER_MAX_VERTICES) {
            LOG_WARN("render_vk: vertex limit reached\n");
            return;
        }
        reserved = GameRender_ReserveMemory(gRenderVk_vertices, &gRenderVk_vertex_capacity,
            gRenderVk_vertex_count + (uint32_t)group->nvertices, GAME_RENDER_MAX_VERTICES, sizeof(RvkVertex));
        if (reserved == NULL) {
            LOG_WARN("render_vk: vertex allocation failed\n");
            return;
        }
        gRenderVk_vertices = (RvkVertex *)reserved;
        base = gRenderVk_vertex_count;
        for (i = 0; i < (uint32_t)group->nvertices; i++) {
            gRenderVk_vertices[base + i].position[0] = (float)group->position[i].v[0];
            gRenderVk_vertices[base + i].position[1] = (float)group->position[i].v[1];
            gRenderVk_vertices[base + i].position[2] = (float)group->position[i].v[2];
            if (group->normal != NULL) {
                gRenderVk_vertices[base + i].normal[0] = (float)group->normal[i].v[0];
                gRenderVk_vertices[base + i].normal[1] = (float)group->normal[i].v[1];
                gRenderVk_vertices[base + i].normal[2] = (float)group->normal[i].v[2];
            } else {
                gRenderVk_vertices[base + i].normal[0] = 0.0f;
                gRenderVk_vertices[base + i].normal[1] = 0.0f;
                gRenderVk_vertices[base + i].normal[2] = 0.0f;
            }
            if (group->map != NULL) {
                gRenderVk_vertices[base + i].uv[0] = (float)group->map[i].v[0];
                gRenderVk_vertices[base + i].uv[1] = (float)group->map[i].v[1];
            } else {
                gRenderVk_vertices[base + i].uv[0] = 0.0f;
                gRenderVk_vertices[base + i].uv[1] = 0.0f;
            }
        }
        gRenderVk_vertex_count += (uint32_t)group->nvertices;

        if (gRenderVk_index_count + (uint32_t)group->nfaces * 3u > GAME_RENDER_MAX_INDICES) {
            LOG_WARN("render_vk: index limit reached\n");
            return;
        }
        reserved = GameRender_ReserveMemory(gRenderVk_indices, &gRenderVk_index_capacity,
            gRenderVk_index_count + (uint32_t)group->nfaces * 3u, GAME_RENDER_MAX_INDICES, sizeof(uint32_t));
        if (reserved == NULL) {
            LOG_WARN("render_vk: index allocation failed\n");
            return;
        }
        gRenderVk_indices = (uint32_t *)reserved;
        first_index = gRenderVk_index_count;
        for (i = 0; i < (uint32_t)group->nfaces; i++) {
            const br_vector3_u16 *face = &group->vertex_numbers[i];

            if (face->v[0] >= group->nvertices || face->v[1] >= group->nvertices || face->v[2] >= group->nvertices) {
                continue;
            }
            gRenderVk_indices[gRenderVk_index_count++] = base + face->v[0];
            gRenderVk_indices[gRenderVk_index_count++] = base + face->v[1];
            gRenderVk_indices[gRenderVk_index_count++] = base + face->v[2];
            emitted += 3u;
        }
        material = group->user != NULL ? (br_material *)group->user : actor_material;
        if (emitted > 0 && !GameRender_EmitRun(model_matrix, material, first_index, emitted)) {
            return;
        }
    }
}

static void GameRender_AddModel(br_actor *actor, br_actor *universe, br_model *model, br_material *actor_material) {
    br_matrix34 model34;
    float model_matrix[16];
    br_vertex *vertex;
    br_face *face;
    br_material *material;
    uint32_t base;
    uint32_t i;
    uint32_t face_index;
    uint32_t run_start;
    uint32_t run_count;
    br_material *run_material;
    uint16_t vertex_count;
    void *reserved;

    if (actor == NULL || universe == NULL || model == NULL) {
        return;
    }
    if (model->vertices == NULL || model->faces == NULL || model->nvertices == 0 || model->nfaces == 0) {
        if (model->prepared != NULL) {
            GameRender_AddPreparedModel(actor, universe, (struct v11model *)model->prepared, actor_material);
        }
        return;
    }
    vertex_count = model->nvertices;
    if (vertex_count == 0 || model->nfaces == 0) {
        return;
    }
    if (gRenderVk_vertex_count + (uint32_t)vertex_count > GAME_RENDER_MAX_VERTICES) {
        LOG_WARN("render_vk: vertex limit reached\n");
        return;
    }
    reserved = GameRender_ReserveMemory(gRenderVk_vertices, &gRenderVk_vertex_capacity, gRenderVk_vertex_count + (uint32_t)vertex_count, GAME_RENDER_MAX_VERTICES, sizeof(RvkVertex));
    if (reserved == NULL) {
        LOG_WARN("render_vk: vertex allocation failed\n");
        return;
    }
    gRenderVk_vertices = (RvkVertex *)reserved;
    base = gRenderVk_vertex_count;
    for (i = 0; i < (uint32_t)vertex_count; i++) {
        vertex = &model->vertices[i];
        gRenderVk_vertices[base + i].position[0] = (float)vertex->p.v[0];
        gRenderVk_vertices[base + i].position[1] = (float)vertex->p.v[1];
        gRenderVk_vertices[base + i].position[2] = (float)vertex->p.v[2];
        gRenderVk_vertices[base + i].normal[0] = (float)vertex->n.v[0];
        gRenderVk_vertices[base + i].normal[1] = (float)vertex->n.v[1];
        gRenderVk_vertices[base + i].normal[2] = (float)vertex->n.v[2];
        gRenderVk_vertices[base + i].uv[0] = (float)vertex->map.v[0];
        gRenderVk_vertices[base + i].uv[1] = (float)vertex->map.v[1];
    }
    gRenderVk_vertex_count += (uint32_t)vertex_count;
    BrActorToActorMatrix34(&model34, actor, universe);
    GameRender_StoreMatrix34(model_matrix, &model34);

    run_start = gRenderVk_index_count;
    run_count = 0;
    run_material = NULL;
    for (face_index = 0; face_index < model->nfaces; face_index++) {
        face = &model->faces[face_index];
        if (face->vertices[0] >= vertex_count || face->vertices[1] >= vertex_count || face->vertices[2] >= vertex_count) {
            continue;
        }
        material = face->material != NULL ? face->material : actor_material;
        if (run_count > 0 && material != run_material) {
            if (!GameRender_EmitRun(model_matrix, run_material, run_start, run_count)) {
                return;
            }
            run_count = 0;
        }
        if (run_count == 0) {
            if (gRenderVk_draw_count >= GAME_RENDER_MAX_DRAWS) {
                LOG_WARN("render_vk: draw limit reached\n");
                return;
            }
            run_material = material;
            run_start = gRenderVk_index_count;
        }
        if (gRenderVk_index_count + 3u > GAME_RENDER_MAX_INDICES) {
            LOG_WARN("render_vk: index limit reached\n");
            break;
        }
        reserved = GameRender_ReserveMemory(gRenderVk_indices, &gRenderVk_index_capacity, gRenderVk_index_count + 3u, GAME_RENDER_MAX_INDICES, sizeof(uint32_t));
        if (reserved == NULL) {
            LOG_WARN("render_vk: index allocation failed\n");
            break;
        }
        gRenderVk_indices = (uint32_t *)reserved;
        gRenderVk_indices[gRenderVk_index_count++] = base + face->vertices[0];
        gRenderVk_indices[gRenderVk_index_count++] = base + face->vertices[1];
        gRenderVk_indices[gRenderVk_index_count++] = base + face->vertices[2];
        run_count += 3u;
    }
    if (run_count > 0) {
        GameRender_EmitRun(model_matrix, run_material, run_start, run_count);
    }
}

static void GameRender_AddActor(br_actor *actor, br_actor *universe, br_model *model, br_material *material, br_uint_8 style) {
    br_actor *child;

    if (actor == NULL || actor == gDont_render_actor) {
        return;
    }
    if (actor->model != NULL) {
        model = actor->model;
    }
    if (actor->material != NULL) {
        material = actor->material;
    }
    if (actor->render_style != BR_RSTYLE_DEFAULT) {
        style = actor->render_style;
    }
    if (style == BR_RSTYLE_NONE) {
        return;
    }
    if (actor->type == BR_ACTOR_MODEL && model != NULL
        && ((model->vertices != NULL && model->faces != NULL && model->nvertices != 0 && model->nfaces != 0) || model->prepared != NULL)) {
        GameRender_AddModel(actor, universe, model, material);
    }
    for (child = actor->children; child != NULL; child = child->next) {
        GameRender_AddActor(child, universe, model, material, style);
    }
}

static void GameRender_ResetScene(void) {
    gRenderVk_scene_ready = 0;
    gRenderVk_vertex_count = 0;
    gRenderVk_index_count = 0;
    gRenderVk_draw_count = 0;
    gRenderVk_draw_flags = 0;
}

static int GameRender_BuildOverlay(br_pixelmap *back) {
    uint8_t *row;
    uint32_t *dest;
    br_pixelmap *palette;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t value;
    uint32_t colour;
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    if (back == NULL || back->pixels == NULL || back->width == 0 || back->height == 0) {
        return 0;
    }
    width = back->width;
    height = back->height;
    if (!GameRender_ReserveOverlay((size_t)width * (size_t)height)) {
        LOG_WARN("render_vk: overlay allocation failed\n");
        return 0;
    }
    switch (back->type) {
    case BR_PMT_INDEX_8:
        palette = back->map != NULL ? back->map : gCurrent_palette;
        for (y = 0; y < height; y++) {
            row = GameRender_RowPointer(back, y, 1);
            dest = gRenderVk_overlay + (size_t)y * width;
            for (x = 0; x < width; x++) {
                value = row[x];
                if (value == GAME_RENDER_TRANSPARENT_INDEX) {
                    dest[x] = 0;
                } else if (palette != NULL && GameRender_PaletteColour(palette, value, &colour)) {
                    dest[x] = ((colour >> 16) & 0xFFu) | (colour & 0x0000FF00u) | ((colour & 0xFFu) << 16) | 0xFF000000u;
                } else {
                    dest[x] = 0xFFFF00FFu;
                }
            }
        }
        return 1;
    case BR_PMT_RGB_565:
        for (y = 0; y < height; y++) {
            row = GameRender_RowPointer(back, y, 2);
            dest = gRenderVk_overlay + (size_t)y * width;
            for (x = 0; x < width; x++) {
                value = (uint32_t)row[x * 2] | ((uint32_t)row[x * 2 + 1] << 8);
                if (value == 0xF81Fu) {
                    dest[x] = 0;
                } else {
                    red = (value >> 11) & 31u;
                    green = (value >> 5) & 63u;
                    blue = value & 31u;
                    dest[x] = (uint32_t)GameRender_Expand5(red) | ((uint32_t)GameRender_Expand6(green) << 8) | ((uint32_t)GameRender_Expand5(blue) << 16) | 0xFF000000u;
                }
            }
        }
        return 1;
    default:
        memset(gRenderVk_overlay, 0, (size_t)width * (size_t)height * sizeof(uint32_t));
        return 1;
    }
}

int GameRender_Init(const RvkPlatform *platform, uint32_t width, uint32_t height) {
    RvkInitInfo info;
    uint8_t white[4];
    const char *value;
    RvkResult result;
    void *reserved;

    if (gRenderVk_active && gRenderVk_device != NULL) {
        return 0;
    }
    if (platform == NULL || width == 0 || height == 0) {
        return -1;
    }
    if (gRenderVk_device != NULL) {
        GameRender_Shutdown();
    }

    memset(&info, 0, sizeof(info));
    info.struct_size = (uint32_t)sizeof(info);
    info.app_name = "dethrace-render-vk";
    info.width = width;
    info.height = height;
    info.platform = *platform;
    value = getenv("RVK_VALIDATION");
    info.enable_validation = value != NULL && (strcmp(value, "1") == 0 || GameRender_EqualsIgnoreCase(value, "true"));
    value = getenv("RVK_PRESENT_MODE");
    info.present_mode = value != NULL && GameRender_EqualsIgnoreCase(value, "immediate") ? RVK_PRESENT_IMMEDIATE : RVK_PRESENT_FIFO;
    info.log = GameRender_LogCallback;
    info.log_user = NULL;

    result = rvk_create(&info, &gRenderVk_device);
    if (result != RVK_OK) {
        fprintf(stderr, "render_vk: rvk_create failed: %s\n", rvk_result_string(result));
        gRenderVk_device = NULL;
        return -1;
    }
    gRenderVk_active = 1;
    gRenderVk_begin_error_logged = 0;
    gRenderVk_draw_error_logged = 0;
    gRenderVk_texture_error_logged = 0;
    gRenderVk_texture_limit_logged = 0;
    gRenderVk_resize_error_logged = 0;

    white[0] = 255;
    white[1] = 255;
    white[2] = 255;
    white[3] = 255;
    result = rvk_create_texture(gRenderVk_device, white, 1, 1, &gRenderVk_white_texture);
    if (result != RVK_OK) {
        fprintf(stderr, "render_vk: white texture creation failed: %s\n", rvk_result_string(result));
        GameRender_Shutdown();
        return -1;
    }
    gRenderVk_white_texture_valid = 1;

    reserved = GameRender_ReserveMemory(NULL, &gRenderVk_vertex_capacity, GAME_RENDER_INITIAL_VERTICES, GAME_RENDER_MAX_VERTICES, sizeof(RvkVertex));
    if (reserved == NULL) {
        LOG_WARN("render_vk: vertex buffer allocation failed\n");
        GameRender_Shutdown();
        return -1;
    }
    gRenderVk_vertices = (RvkVertex *)reserved;

    reserved = GameRender_ReserveMemory(NULL, &gRenderVk_index_capacity, GAME_RENDER_INITIAL_INDICES, GAME_RENDER_MAX_INDICES, sizeof(uint32_t));
    if (reserved == NULL) {
        LOG_WARN("render_vk: index buffer allocation failed\n");
        GameRender_Shutdown();
        return -1;
    }
    gRenderVk_indices = (uint32_t *)reserved;

    reserved = GameRender_ReserveMemory(NULL, &gRenderVk_draw_capacity, GAME_RENDER_INITIAL_DRAWS, GAME_RENDER_MAX_DRAWS, sizeof(RvkWorldDraw));
    if (reserved == NULL) {
        LOG_WARN("render_vk: draw buffer allocation failed\n");
        GameRender_Shutdown();
        return -1;
    }
    gRenderVk_draws = (RvkWorldDraw *)reserved;

    if (!GameRender_ReserveOverlay((size_t)width * (size_t)height)) {
        LOG_WARN("render_vk: overlay buffer allocation failed\n");
        GameRender_Shutdown();
        return -1;
    }

    gRenderVk_active = 1;
    gRenderVk_scene_ready = 0;
    gRenderVk_camera = NULL;
    gHarness_platform.RenderVk_OnResize = GameRender_OnResize;
    return 0;
}

void GameRender_ResetTrackResources(void) {
    uint32_t i;

    gRenderVk_sky_base_yaw_valid = 0;
    gOld_fov = 0;
    gOld_yon = 0;
    if (gRenderVk_device != NULL) {
        rvk_wait_idle(gRenderVk_device);
        for (i = 0; i < gRenderVk_texture_count; i++) {
            if (gRenderVk_textures[i].owned) {
                rvk_destroy_texture(gRenderVk_device, gRenderVk_textures[i].texture);
            }
        }
    }
    gRenderVk_texture_count = 0;
}

void GameRender_Shutdown(void) {
    uint32_t i;

    if (gRenderVk_device != NULL) {
        rvk_wait_idle(gRenderVk_device);
        if (gRenderVk_white_texture_valid) {
            rvk_destroy_texture(gRenderVk_device, gRenderVk_white_texture);
            gRenderVk_white_texture_valid = 0;
        }
        for (i = 0; i < gRenderVk_texture_count; i++) {
            if (gRenderVk_textures[i].owned) {
                rvk_destroy_texture(gRenderVk_device, gRenderVk_textures[i].texture);
            }
        }
    }

    free(gRenderVk_textures);
    gRenderVk_textures = NULL;
    gRenderVk_texture_count = 0;
    gRenderVk_texture_capacity = 0;

    free(gRenderVk_vertices);
    gRenderVk_vertices = NULL;
    gRenderVk_vertex_count = 0;
    gRenderVk_vertex_capacity = 0;

    free(gRenderVk_indices);
    gRenderVk_indices = NULL;
    gRenderVk_index_count = 0;
    gRenderVk_index_capacity = 0;

    free(gRenderVk_draws);
    gRenderVk_draws = NULL;
    gRenderVk_draw_count = 0;
    gRenderVk_draw_capacity = 0;

    free(gRenderVk_overlay);
    gRenderVk_overlay = NULL;
    gRenderVk_overlay_capacity = 0;

    if (gRenderVk_device != NULL) {
        rvk_destroy(gRenderVk_device);
        gRenderVk_device = NULL;
    }

    gRenderVk_white_texture = 0;
    gRenderVk_active = 0;
    gRenderVk_scene_ready = 0;
    gRenderVk_camera = NULL;
    memset(gRenderVk_view_proj, 0, sizeof(gRenderVk_view_proj));
    gRenderVk_begin_error_logged = 0;
    gRenderVk_draw_error_logged = 0;
    gRenderVk_texture_error_logged = 0;
    gRenderVk_texture_limit_logged = 0;
    gRenderVk_resize_error_logged = 0;
}

void GameRender_OnResize(int width, int height) {
    RvkResult result;

    if (!GameRender_IsActive() || width <= 0 || height <= 0) {
        return;
    }
    result = rvk_resize(gRenderVk_device, (uint32_t)width, (uint32_t)height);
    if (result == RVK_ERROR_DEVICE_LOST) {
        gRenderVk_active = 0;
        LOG_WARN("render_vk: device lost during resize\n");
        return;
    }
    if (result != RVK_OK && !gRenderVk_resize_error_logged) {
        LOG_WARN("render_vk: resize failed: %s\n", rvk_result_string(result));
        gRenderVk_resize_error_logged = 1;
    }
}

void GameRender_Begin2D(br_pixelmap *back) {
    uint8_t *row;
    uint16_t *row16;
    uint32_t x;
    uint32_t y;

    if (!GameRender_IsActive() || back == NULL || back->pixels == NULL) {
        return;
    }
    switch (back->type) {
    case BR_PMT_INDEX_8:
        for (y = 0; y < back->height; y++) {
            row = GameRender_RowPointer(back, y, 1);
            memset(row, GAME_RENDER_TRANSPARENT_INDEX, back->width);
        }
        break;
    case BR_PMT_RGB_565:
        for (y = 0; y < back->height; y++) {
            row16 = (uint16_t *)GameRender_RowPointer(back, y, 2);
            for (x = 0; x < back->width; x++) {
                row16[x] = 0xF81F;
            }
        }
        break;
    default:
        break;
    }
}

void GameRender_BeginScene(br_actor *camera) {
    br_camera *camera_data;
    br_matrix34 camera_to_world;
    br_matrix4 projection;
    br_matrix4 view_proj;
    br_pixelmap *screen;
    br_scalar origin_x = BR_SCALAR(0);
    br_scalar origin_y = BR_SCALAR(0);
    br_scalar aspect;
    int row;
    int column;

    gRenderVk_camera = camera;
    GameRender_ResetScene();
    memset(gRenderVk_view_proj, 0, sizeof(gRenderVk_view_proj));
    if (!GameRender_IsActive() || camera == NULL || gUniverse_actor == NULL || camera->type_data == NULL) {
        return;
    }
    camera_data = (br_camera *)camera->type_data;
    {
        static float yon_base;
        static int yon_base_valid;
        const char *far_value = getenv("RVK_FAR_MULTIPLIER");
        float multiplier = 20.0f;

        if (far_value != NULL) {
            char *end = NULL;
            float parsed = (float)strtod(far_value, &end);
            if (end != far_value && parsed > 0.0f) {
                multiplier = parsed;
            }
        }
        if (!yon_base_valid) {
            yon_base = (float)gCamera_yon;
            yon_base_valid = 1;
        }
        camera_data->yon_z = (br_scalar)(yon_base * multiplier);
    }
    memset(&gRenderVk_background, 0, sizeof(gRenderVk_background));
    gRenderVk_background_valid = 0;
    if (gProgram_state.current_depth_effect.sky_texture != NULL
        && (gLast_camera_special_volume == NULL || gLast_camera_special_volume->sky_col < 0)) {
        br_pixelmap *sky = gProgram_state.current_depth_effect.sky_texture;
        br_pixelmap *bg_screen = gRender_screen;
        float tan_half_fov;
        float tan_half_hori_fov;
        float tan_half_hori_sky;
        float hori_sky;
        float yaw;
        float src_x;
        float tan_pitch;
        float hori_y;
        float hori_pixels;

        if (bg_screen != NULL && bg_screen->width != 0 && bg_screen->height != 0
            && sky->width != 0 && sky->height != 0 && gSky_image_height != 0) {
            tan_half_fov = (float)Tan(camera_data->field_of_view / 2);
            if (tan_half_fov > 0.0f) {
                tan_half_hori_fov = tan_half_fov * (float)camera_data->aspect;
                tan_half_hori_sky = (float)sky->width * tan_half_hori_fov / (float)bg_screen->width;
                hori_sky = 2.0f * atanf(tan_half_hori_sky);
                if (hori_sky > 0.0f) {
                    int repetitions = (int)(6.2831853f / hori_sky + 0.5f);
                    if (repetitions < 1) {
                        repetitions = 1;
                    }
                    hori_sky = 6.2831853f / (float)repetitions;
                    yaw = atan2f((float)gCamera_to_world.m[2][0], (float)gCamera_to_world.m[2][2]);
                    src_x = (float)sky->width * (-yaw / hori_sky);
                    src_x = fmodf(src_x, (float)sky->width);
                    if (src_x < 0.0f) {
                        src_x += (float)sky->width;
                    }
                    tan_pitch = (float)gCamera_to_world.m[2][1]
                        / sqrtf((float)gCamera_to_world.m[2][0] * (float)gCamera_to_world.m[2][0]
                            + (float)gCamera_to_world.m[2][2] * (float)gCamera_to_world.m[2][2]);
                    hori_y = tan_pitch / tan_half_fov * (float)bg_screen->height / 2.0f;
                    hori_pixels = (float)sky->height
                        - (float)gSky_image_underground * (float)sky->height / (float)gSky_image_height;
                    gRenderVk_background.texture = GameRender_GetTexture(sky);
                    gRenderVk_background.src_x = src_x;
                    gRenderVk_background.top_y = -hori_y - hori_pixels;
                    gRenderVk_background.texture_width = (float)sky->width;
                    gRenderVk_background.texture_height = (float)sky->height;
                    gRenderVk_background_valid = gRenderVk_background.texture != 0;
                }
            }
        }
    }
    screen = gRender_screen;
    if (screen != NULL && screen->width != 0 && screen->height != 0) {
        origin_x = (br_scalar)(screen->origin_x - screen->width / 2) / (br_scalar)(screen->width / 2);
        origin_y = -(br_scalar)(screen->origin_y - screen->height / 2) / (br_scalar)(screen->height / 2);
        aspect = (br_scalar)screen->width / (br_scalar)screen->height;
    } else {
        aspect = camera_data->aspect;
    }
    if (aspect <= BR_SCALAR(0)) {
        aspect = BR_SCALAR(1);
    }
    BrMatrix4Perspective(&projection, camera_data->field_of_view, aspect,
        -camera_data->hither_z, -camera_data->yon_z);
    projection.m[2][0] = -origin_x;
    projection.m[2][1] = -origin_y;
    BrActorToActorMatrix34(&camera_to_world, gUniverse_actor, camera);
    BrMatrix4Mul34(&view_proj, &camera_to_world, &projection);
    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            gRenderVk_view_proj[column * 4 + row] = (float)view_proj.m[column][row];
        }
    }
}

static int GameRender_PrepareSky(br_actor *camera) {
    br_camera *camera_data;

    if (camera == NULL || camera->type_data == NULL || gForward_sky_actor == NULL || gForward_sky_model == NULL
        || gHorizon_material == NULL || gProgram_state.current_depth_effect.sky_texture == NULL
        || (gLast_camera_special_volume != NULL && gLast_camera_special_volume->sky_col >= 0)) {
        return 0;
    }
    camera_data = (br_camera *)camera->type_data;
    if (camera_data->field_of_view != gOld_fov || camera_data->yon_z != gOld_yon) {
        MungeSkyModel(camera, gForward_sky_model);
        gOld_fov = camera_data->field_of_view;
        gOld_yon = camera_data->yon_z;
    }
    if (!gRenderVk_sky_base_yaw_valid) {
        gRenderVk_sky_base_yaw = BrRadianToAngle(atan2(gCamera_to_world.m[2][0], gCamera_to_world.m[2][2]));
        gRenderVk_sky_base_yaw_valid = 1;
    }
    BrVector3Copy(&gForward_sky_actor->t.t.translate.t, (br_vector3 *)gCamera_to_world.m[3]);
    gForward_sky_actor->render_style = BR_RSTYLE_FACES;
    return 1;
}

void GameRender_AddUniverse(br_actor *universe) {
    if (!GameRender_IsActive() || universe == NULL || universe == gDont_render_actor) {
        return;
    }
    GameRender_AddActor(universe, universe, NULL, NULL, BR_RSTYLE_DEFAULT);
    {
        int i;
        for (i = 0; i < gNumber_of_lollipops; i++) {
            br_actor *lollipop = gLollipops[i];
            if (lollipop != NULL && lollipop->render_style == BR_RSTYLE_NONE) {
                lollipop->render_style = BR_RSTYLE_FACES;
                SetPedMaterialForRender(lollipop);
                GameRender_AddActor(lollipop, universe, NULL, NULL, BR_RSTYLE_FACES);
                lollipop->render_style = BR_RSTYLE_NONE;
            }
        }
    }
    if (gRenderVk_draw_count > 0) {
        gRenderVk_scene_ready = 1;
    }
}

void GameRender_EndScene(void) {
    if (!GameRender_IsActive()) {
        return;
    }
    if (gRenderVk_vertex_count > 0 && gRenderVk_index_count > 0 && gRenderVk_draw_count > 0) {
        gRenderVk_scene_ready = 1;
    }
}

static void GameRender_LogFps(void) {
    static int last_time;
    static uint32_t frames;
    int now;

    if (getenv("RVK_FPS_LOG") == NULL) {
        return;
    }
    now = PDGetTotalTime();
    frames++;
    if (last_time == 0) {
        last_time = now;
        return;
    }
    if (now - last_time >= 1000) {
        fprintf(stderr, "render_vk fps=%.1f game_fps=%d frames=%u draws=%u verts=%u indices=%u\n",
            (double)gFrame_rate / 10.0, gFrame_rate, frames, gRenderVk_draw_count,
            gRenderVk_vertex_count, gRenderVk_index_count);
        frames = 0;
        last_time = now;
    }
}

static void GameRender_BackgroundClearColour(float colour[4]) {
    int fill_index = 0;
    uint32_t rgb = 0;

    colour[0] = 0.0f;
    colour[1] = 0.0f;
    colour[2] = 0.0f;
    colour[3] = 1.0f;
    if (gProgram_state.current_depth_effect.sky_texture != NULL
        && (gLast_camera_special_volume == NULL || gLast_camera_special_volume->sky_col < 0)) {
        return;
    }
    if (gProgram_state.current_depth_effect.type == eDepth_effect_fog
        || gSwap_depth_effect_type == eDepth_effect_fog) {
        fill_index = 255;
    } else if (gProgram_state.current_depth_effect.type && gSwap_depth_effect_type
        && gLast_camera_special_volume != NULL && gLast_camera_special_volume->sky_col >= 0) {
        fill_index = gLast_camera_special_volume->sky_col;
    }
    if (gCurrent_palette != NULL && GameRender_PaletteColour(gCurrent_palette, (uint32_t)fill_index, &rgb)) {
        colour[0] = (float)((rgb >> 16) & 0xFFu) / 255.0f;
        colour[1] = (float)((rgb >> 8) & 0xFFu) / 255.0f;
        colour[2] = (float)(rgb & 0xFFu) / 255.0f;
    }
}

void GameRender_Swap(br_pixelmap *back) {
    float clear_color[4];
    RvkResult result;
    int have_overlay;
    int device_lost;

    if (!GameRender_IsActive()) {
        return;
    }
    GameRender_BackgroundClearColour(clear_color);
    if (gHarness_platform.ProcessWindowMessages != NULL) {
        gHarness_platform.ProcessWindowMessages();
    }
    if (gBack_screen != NULL && gBack_screen->pixels != NULL && gBack_screen->width != 0 && gBack_screen->height != 0) {
        back = gBack_screen;
    }
    have_overlay = GameRender_BuildOverlay(back);

    result = rvk_begin_frame(gRenderVk_device);
    if (result == RVK_ERROR_OUT_OF_DATE) {
        return;
    }
    if (result != RVK_OK) {
        if (!gRenderVk_begin_error_logged) {
            LOG_WARN("render_vk: begin frame failed: %s\n", rvk_result_string(result));
            gRenderVk_begin_error_logged = 1;
        }
        if (result == RVK_ERROR_DEVICE_LOST) {
            gRenderVk_active = 0;
            LOG_WARN("render_vk: device lost\n");
        }
        return;
    }

    device_lost = 0;
    if (gRenderVk_scene_ready && gRenderVk_vertex_count > 0 && gRenderVk_index_count > 0 && gRenderVk_draw_count > 0) {
        result = rvk_draw_world(gRenderVk_device, gRenderVk_view_proj, gRenderVk_vertices, gRenderVk_vertex_count, gRenderVk_indices, gRenderVk_index_count, gRenderVk_draws, gRenderVk_draw_count, gRenderVk_background_valid ? &gRenderVk_background : NULL, clear_color);
        if (result == RVK_ERROR_DEVICE_LOST) {
            device_lost = 1;
            gRenderVk_active = 0;
            LOG_WARN("render_vk: device lost while drawing the world\n");
        } else if (result != RVK_OK && !gRenderVk_draw_error_logged) {
            LOG_WARN("render_vk: world draw failed: %s\n", rvk_result_string(result));
            gRenderVk_draw_error_logged = 1;
        }
    }

    if (have_overlay && !device_lost) {
        result = rvk_draw_overlay(gRenderVk_device, gRenderVk_overlay, back->width, back->height);
        if (result == RVK_ERROR_DEVICE_LOST) {
            device_lost = 1;
            gRenderVk_active = 0;
            LOG_WARN("render_vk: device lost while drawing the overlay\n");
        } else if (result != RVK_OK && !gRenderVk_draw_error_logged) {
            LOG_WARN("render_vk: overlay draw failed: %s\n", rvk_result_string(result));
            gRenderVk_draw_error_logged = 1;
        }
    }

    if (!device_lost) {
        result = rvk_end_frame(gRenderVk_device);
        if (result == RVK_ERROR_DEVICE_LOST) {
            gRenderVk_active = 0;
            LOG_WARN("render_vk: device lost while presenting\n");
        } else if (result == RVK_ERROR_OUT_OF_DATE) {
            gRenderVk_scene_ready = 0;
            gRenderVk_vertex_count = 0;
            gRenderVk_index_count = 0;
            gRenderVk_draw_count = 0;
            return;
        } else if (result != RVK_OK && !gRenderVk_draw_error_logged) {
            LOG_WARN("render_vk: end frame failed: %s\n", rvk_result_string(result));
            gRenderVk_draw_error_logged = 1;
        }
    }

    GameRender_LogFps();
    gRenderVk_scene_ready = 0;
    gRenderVk_vertex_count = 0;
    gRenderVk_index_count = 0;
    gRenderVk_draw_count = 0;
}

int GameRender_IsActive(void) {
    return gRenderVk_active && gRenderVk_device != NULL;
}
