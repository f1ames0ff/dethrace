#ifndef DETHRACE_RENDER_VK_BRIDGE_H
#define DETHRACE_RENDER_VK_BRIDGE_H
#include "brender.h"
#include "rvk.h"
#ifdef __cplusplus
extern "C" {
#endif
int GameRender_Init(const RvkPlatform *platform, uint32_t width, uint32_t height);
void GameRender_Shutdown(void);
void GameRender_OnResize(int width, int height);
void GameRender_Begin2D(br_pixelmap *back);
void GameRender_BeginScene(br_actor *camera);
void GameRender_AddUniverse(br_actor *universe);
void GameRender_EndScene(void);
void GameRender_Swap(br_pixelmap *back);
int GameRender_IsActive(void);
#ifdef __cplusplus
}
#endif
#endif
