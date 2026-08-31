#include <AP2/AP2.h>

int main(int argc, char *argv[]) {
  AP_Init(AP_INIT_ALL);
  AP_Window *window = AP_CreateWindow("AP2 Test", 800, 600, AP_WINDOW_CENTERED);
  AP_SetActiveWindow(window);

  AP_Model *model = AP_LoadModel("scene.gltf");

  while (AP_IsRunning()) {
    AP_PollEvents();
    AP_Fill(0.0f, 0.0f, 0.0f, 1.0f);
    AP_PushTransform();
    AP_DrawModel(model);
    AP_PopTransform();
    AP_Present();
  }



}