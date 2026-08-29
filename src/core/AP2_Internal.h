#ifndef AP2_INTERNAL_H
#define AP2_INTERNAL_H

/*
 * Private AP2 internals.
 *
 * These declarations are for AP2 source files only. They must never
 * appear in public headers and must never be used by applications.
 */

#include <stdbool.h>

#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Types.h"

typedef struct AP_Window AP_Window;
typedef struct AP_Shader AP_Shader;
typedef struct AP_Texture AP_Texture;
typedef struct GLFWwindow GLFWwindow;

#ifdef __cplusplus
extern "C" {
#endif

GLFWwindow *AP_WindowGetGLFW(const AP_Window *window);

void AP_WindowGetFramebufferPixels(const AP_Window *window, int *width,
                                   int *height);

bool AP_RendererBindWindow(AP_Window *window);

void AP_RendererUnbindWindow(AP_Window *window);

bool AP_RendererMakeCurrent(AP_Window *window);

void AP_RendererNotifyResize(int width, int height);

void AP_RendererFlushCurrent(void);

bool AP_RendererSetUserShader(AP_Shader *shader);

AP_Shader *AP_RendererGetUserShader(void);

AP_Shader *AP_RendererGetBuiltinShader(void);

void AP_ShaderDestroyInternal(AP_Shader *shader);

AP_UInt AP_ShaderNativeProgram(const AP_Shader *shader);

AP_Int AP_ShaderResolutionUniform(AP_Shader *shader);

AP_Int AP_ShaderTextureUniform(AP_Shader *shader);

AP_UInt AP_TextureNativeId(const AP_Texture *texture);

bool AP_RendererSubmitTexturedQuad(AP_UInt texture, AP_BlendMode blend,
                                   const AP_FPoint corners[4],
                                   const AP_FPoint uvs[4], AP_Color tint);

bool AP_RendererBindTarget(AP_UInt fbo, int width, int height);

bool AP_RendererDrawMesh(AP_UInt texture, AP_BlendMode blend,
                         const AP_Vertex *vertices, int vertex_count,
                         const AP_U16 *indices, int index_count,
                         AP_Primitive primitive);

bool AP_FontInit(void);

void AP_FontShutdown(void);

void AP_GuiShutdown(void);

void AP_InputInit(void);

void AP_InputShutdown(void);

void AP_InputAttachWindow(GLFWwindow *handle);

void AP_InputDetachWindow(GLFWwindow *handle);

void AP_InputBeginFrame(void);

void AP_InputEndFrame(GLFWwindow *active_handle);

void AP_InputOnCursorMove(double x, double y);

void AP_InputOnCursorWarp(double x, double y);

void AP_InputOnFocusChanged(bool focused);

#ifdef __cplusplus
}
#endif

#endif /* AP2_INTERNAL_H */
