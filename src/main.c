/*
 * main.c is a switch: each example under src/examples/ exposes one
 * `Example_*` entry point. Call exactly one below to run it.
 */

#include "examples/gui_app.h"
#include "examples/hello_window.h"
#include "examples/materials_textures.h"
#include "examples/scene_3d.h"
#include "examples/shaders_post.h"
#include "examples/shapes_2d.h"

#include <AP2/AP2.h>

int main(void) {
  //return Example_GuiApp();
  return Example_MaterialsTextures();
  //return Example_HelloWindow();
  //return Example_Shapes2D();
  //return Example_Scene3D();
  //return Example_ShadersPost();
}
