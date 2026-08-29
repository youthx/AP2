#ifndef AP2_H
#define AP2_H

/*
 * AP2 - Aphelion 2D/3D Rendering Library
 *
 * Umbrella header. Include this from application code.
 *
 * Typical usage:
 *
 *     AP_Init(AP_INIT_VIDEO);
 *     AP_CreateWindow("AP2", 1280, 720, AP_WINDOW_RESIZABLE);
 *
 *     while (AP_IsRunning()) {
 *         AP_PumpEvents();
 *         AP_SetRenderDrawColorFloat(0.1f, 0.1f, 0.1f, 1.0f);
 *         AP_RenderClear();
 *         AP_RenderPresent();
 *     }
 *
 *     AP_DestroyWindow(NULL);
 *     AP_Quit();
 */

#include "AP2/AP2_Device.h"
#include "AP2/AP2_Error.h"
#include "AP2/AP2_Init.h"
#include "AP2/AP2_Input.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Shader.h"
#include "AP2/AP2_Sprite.h"
#include "AP2/AP2_Texture.h"
#include "AP2/AP2_Types.h"
#include "AP2/AP2_Video.h"
#include "AP2/AP2_Window.h"

#endif /* AP2_H */
