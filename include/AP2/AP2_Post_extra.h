#ifndef AP2_POST_EXTRA_H
#define AP2_POST_EXTRA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Extended post-processing controls, layered on top of AP2_Post.h.
     * Every setter clamps internally (see AP2_Post.c for exact ranges)
     * and the value is retained even if its effect is currently at 0.
     */

    /* =============================================================
     * REALISM / CINEMATIC
     * ============================================================= */

    /* Exposure / tone */
    bool AP_SetPostExposure(float exposure);
    bool AP_SetPostGamma(float gamma);
    bool AP_SetPostFilmic(float amount);

    /* Color correction (extends AP_SetPostColorGrade in AP2_Post.h) */
    bool AP_SetPostContrast(float contrast);
    bool AP_SetPostSaturation(float saturation);
    bool AP_SetPostBrightness(float brightness);
    bool AP_SetPostTemperature(float temperature);
    bool AP_SetPostTint(float tint);

    /* Image enhancement */
    bool AP_SetPostClarity(float amount);
    bool AP_SetPostDetail(float amount);

    /* Bloom / light (AP_SetPostBloom threshold+intensity lives in AP2_Post.h) */
    bool AP_SetPostBloomRadius(float radius);
    bool AP_SetPostBloomSoftness(float softness);
    bool AP_SetPostLensDirt(float amount);

    /* Lens effects (AP_SetPostChromatic / AP_SetPostVignette live in AP2_Post.h) */
    bool AP_SetPostLensDistortion(float amount);
    bool AP_SetPostBarrel(float amount);

    /* Film (AP_SetPostGrain lives in AP2_Post.h) */
    bool AP_SetPostFilmGrain(float amount);
    bool AP_SetPostFilmResponse(float amount);
    bool AP_SetPostHalation(float amount);

    /* Atmospheric */
    bool AP_SetPostFog(float amount);
    bool AP_SetPostFogDensity(float density);
    bool AP_SetPostFogHeight(float height);

    /* Depth-based effects */
    bool AP_SetPostDepthOfField(float amount);
    bool AP_SetPostDOFFocus(float focus);
    bool AP_SetPostDOFAperture(float aperture);

    /* Motion */
    bool AP_SetPostMotionBlur(float amount);

    /* =============================================================
     * CREATIVE / STYLIZED
     * ============================================================= */

    /* Resolution / geometry-like */
    bool AP_SetPostPixelate(float pixels);
    bool AP_SetPostPosterize(float levels);

    /* Retro */
    bool AP_SetPostScanlines(float amount);
    bool AP_SetPostCRT(float amount);
    bool AP_SetPostVHS(float amount);
    bool AP_SetPostRGBSplit(float amount);

    /* Color */
    bool AP_SetPostSepia(float amount);
    bool AP_SetPostGrayscale(float amount);
    bool AP_SetPostInvert(float amount);
    bool AP_SetPostSolarize(float amount);
    bool AP_SetPostColorize(float amount);
    bool AP_SetPostColorizeHue(float hue);

    /* Stylization */
    bool AP_SetPostEdge(float amount);
    bool AP_SetPostOutline(float amount);
    bool AP_SetPostCelShade(float amount);
    bool AP_SetPostDither(float amount);
    bool AP_SetPostHalftone(float amount);

    /* Glitch */
    bool AP_SetPostGlitch(float amount);
    bool AP_SetPostNoise(float amount);
    bool AP_SetPostDisplacement(float amount);

    /* Distortion */
    bool AP_SetPostKaleidoscope(float amount);
    bool AP_SetPostWave(float amount);
    bool AP_SetPostRipple(float amount);
    bool AP_SetPostFisheye(float amount);

    /* =============================================================
     * QUALITY / MASTER CONTROLS
     * ============================================================= */

    /* Master strength for the complete post stack. */
    bool AP_SetPostIntensity(float intensity);

#ifdef __cplusplus
}
#endif

#endif
