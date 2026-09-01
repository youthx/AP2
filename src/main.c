#include <AP2/AP2.h>

int main(void)
{
  AP_Init(AP_INIT_ALL);
  AP_Window *window = AP_CreateWindow("AP2 - Advanced Materials & Textures", 1280, 720,
                                      AP_WINDOW_CENTERED | AP_WINDOW_RESIZABLE);
  AP_SetActiveWindow(window);

  /* Better camera angle for product photography - 3/4 view */
  AP_Camera camera = AP_CameraPerspective(AP_V3(6.0f, 3.5f, 6.0f),
                                          AP_V3(0.0f, 0.5f, 0.0f), 50.0f);

  /* ================================================================
   * Example 1: Load a GLTF model with full materials and textures
   * ================================================================ */
  AP_Model *model = AP_LoadModel("microphone.glb");

  if (!AP_ModelIsValid(model))
  {
    AP_WARN("Failed to load model. Creating fallback cube...");
    /* Fallback: Create a simple cube with custom material */
    AP_Mesh *cubeMesh = AP_CreateMeshCube(2.0f);
    AP_Material *fallbackMat = AP_CreateMaterial("Fallback", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    AP_MaterialInitPbrMetallicRoughness(fallbackMat,
                                        AP_C4(0.8f, 0.2f, 0.2f, 1.0f),
                                        0.2f, 0.6f);
    AP_MeshSetMaterial(cubeMesh, fallbackMat);

    model = AP_CreateModel();
    if (model)
    {
      /* TODO: attach cubeMesh/fallbackMat to the model once
       * AP_Model exposes a mesh-append API. */
    }
  }

  if (AP_ModelIsValid(model))
  {
    /* ================================================================
     * Example 2: Access materials and textures from loaded model
     * ================================================================ */
    int material_count = AP_ModelGetMaterialCount(model);
    int texture_count = AP_ModelGetTextureCount(model);
    int mesh_count = AP_ModelMeshCount(model);

    AP_INFO("Loaded model with %d meshes, %d materials, %d textures",
            mesh_count, material_count, texture_count);

    /* ================================================================
     * Example 3: Modify material properties
     * ================================================================ */
    if (material_count > 0)
    {
      AP_Material *mat = AP_ModelGetMaterial(model, 0);
      if (mat)
      {
        AP_INFO("Material: %s (type: %d)", mat->name ? mat->name : "Unnamed", mat->type);
        AP_INFO("  Base Color: RGBA(%.2f, %.2f, %.2f, %.2f)",
                mat->base_color.r, mat->base_color.g, mat->base_color.b, mat->base_color.a);
        AP_INFO("  Metallic: %.2f, Roughness: %.2f", mat->metallic, mat->roughness);
        AP_INFO("  Has Base Color Texture: %s", mat->has_base_color_texture ? "yes" : "no");
        AP_INFO("  Has Normal Map: %s", mat->has_normal_texture ? "yes" : "no");
        AP_INFO("  Has Metallic/Roughness Texture: %s (id=%u)",
                mat->has_metallic_roughness_texture ? "yes" : "no",
                (unsigned)mat->metallic_roughness_texture);
        AP_INFO("  base_color_texture id=%u normal_texture id=%u",
                (unsigned)mat->base_color_texture, (unsigned)mat->normal_texture);
      }
    }

    /* ================================================================
     * Example 4: Access textures from model
     * ================================================================ */
    if (texture_count > 0)
    {
      for (int i = 0; i < texture_count && i < 3; ++i)
      {
        AP_Texture *tex = AP_ModelGetTexture(model, i);
        if (tex)
        {
          AP_INFO("Texture %d: Valid=%s", i, tex ? "yes" : "no");
        }
      }
    }

    /* ================================================================
     * Example 5: Per-mesh material and transform control
     * ================================================================ */
    for (int i = 0; i < mesh_count; ++i)
    {
      AP_Mesh *mesh = AP_ModelGetMesh(model, i);
      if (mesh && AP_MeshIsValid(mesh))
      {
        AP_Material *mesh_mat = AP_MeshGetMaterial(mesh);
        AP_INFO("Mesh %d: %d vertices, %d indices, Material: %s",
                i, AP_MeshVertexCount(mesh), AP_MeshIndexCount(mesh),
                mesh_mat && mesh_mat->name ? mesh_mat->name : "None");

        /* You can override the material if needed */
        if (i == 0 && mesh_mat)
        {
          /* Create a variant of the material */
          AP_Material *variant = AP_CreateMaterial("Variant", mesh_mat->type);
          if (variant)
          {
            *variant = *mesh_mat;     /* Copy properties */
            variant->metallic = 0.8f; /* Make it more metallic */
            variant->roughness = 0.2f;
            /* Note: Don't call AP_MeshSetMaterial here since mesh owns it */
          }
        }
      }
    }

    /* ================================================================
     * Example 6: Set model transforms
     * ================================================================ */
    AP_ModelSetPosition(model, AP_V3(0.0f, 0.0f, 0.0f));
    AP_ModelSetTRS(model, AP_V3(0.0f, 0.0f, 0.0f),
                   AP_QuatFromEuler(0.0f, 0.0f, 0.0f),
                   AP_V3(1.0f, 1.0f, 1.0f));
  }

  /* Create additional test materials */
  AP_Material *pbr_mat = AP_CreateMaterial("PBR_Material", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
  AP_MaterialInitPbrMetallicRoughness(pbr_mat, AP_C4(0.9f, 0.9f, 0.9f, 1.0f), 0.0f, 0.3f);

  AP_Material *unlit_mat = AP_CreateMaterial("Unlit_Material", AP_MATERIAL_TYPE_UNLIT);
  AP_MaterialInitUnlit(unlit_mat, AP_C4(1.0f, 0.5f, 0.0f, 1.0f));

  /* Create test mesh with custom material */
  AP_Mesh *sphere = AP_CreateMeshSphere(1.0f, 16, 12);
  AP_MeshSetMaterial(sphere, pbr_mat);

  while (AP_IsRunning())
  {
    AP_ClearLights();
    AP_Fill(0.15f, 0.15f, 0.15f, 1.0f);

    AP_PumpEvents();

    /* Animate model */
    if (AP_ModelIsValid(model))
    {
      AP_ModelSetPosition(model, AP_V3(0.0f, 0.0f, 0.0f));
      AP_ModelRotate(model, AP_V3(0.0f, 1.0f, 0.0f), 0.5f);
    }

    AP_Begin3D(&camera);

    /* Professional studio lighting setup */
    /* Key light - strong, from upper right front */
    AP_AddLight(AP_LightDirectional(AP_V3(-0.7f, -0.95f, -0.3f),
                                    AP_C4(1.0f, 1.0f, 0.95f, 1.0f), 1.8f));

    /* Fill light - from left to reduce harsh shadows */
    AP_AddLight(AP_LightPoint(AP_V3(-4.0f, 1.5f, 1.0f),
                              AP_C4(0.5f, 0.5f, 0.6f, 1.0f), 0.7f, 12.0f));

    /* Rim/back light - definition and separation */
    AP_AddLight(AP_LightPoint(AP_V3(2.0f, 4.0f, -6.0f),
                              AP_C4(0.3f, 0.3f, 0.4f, 1.0f), 0.5f, 15.0f));

    /* Very low ambient for high contrast */
    AP_SetAmbientLight(AP_C4(0.08f, 0.08f, 0.1f, 1.0f));

    /* Draw main model */
    if (AP_ModelIsValid(model))
    {
      AP_DrawModel(model);
    }

    /* Draw sphere with custom material */
    AP_Set3DPosition(AP_V3(-4.0f, 0.0f, 0.0f));
    AP_DrawMesh(sphere);

    /* Draw another sphere with unlit material */
    AP_Set3DPosition(AP_V3(4.0f, 0.0f, 0.0f));
    AP_DrawMesh(AP_CreateMeshSphere(1.0f, 16, 12));

    /* Draw a grid for reference */
    AP_Reset3DModel();
    AP_DrawGrid3D(10.0f, 10, AP_C4(0.3f, 0.3f, 0.3f, 1.0f));

    AP_End3D();
    AP_Present();
  }

  /* Cleanup */
  AP_DestroyMaterial(pbr_mat);
  AP_DestroyMaterial(unlit_mat);
  AP_DestroyMesh(sphere);
  if (AP_ModelIsValid(model))
  {
    AP_DestroyModel(model);
  }
  AP_DestroyWindow(window);
  AP_Quit();

  return 0;
}
