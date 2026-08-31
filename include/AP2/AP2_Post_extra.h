#ifndef AP2_POST_EXTRA_H
#define AP2_POST_EXTRA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================
 * REALISM / CINEMATIC
 * ============================================================= */

/* Exposure / tone */
bool AP_SetPostExposure(float *exposure);
bool AP_SetPostGamma(float *gamma);
bool AP_SetPostFilmic(float *amount);

/* Color correction */
bool AP_SetPostContrast(float *contrast);
bool AP_SetPostSaturation(float *saturation);
bool AP_SetPostBrightness(float *brightness);
bool AP_SetPostTemperature(float *temperature);
bool AP_SetPostTint(float *tint);

/* Image enhancement */
bool AP_SetPostSharpen(float *amount);
bool AP_SetPostClarity(float *amount);
bool AP_SetPostDetail(float *amount);

/* Bloom / light */
bool AP_SetPostBloom(float *threshold, float *intensity);
bool AP_SetPostBloomRadius(float *radius);
bool AP_SetPostBloomSoftness(float *softness);
bool AP_SetPostLensDirt(float *amount);

/* Lens effects */
bool AP_SetPostChromatic(float *amount);
bool AP_SetPostLensDistortion(float *amount);
bool AP_SetPostBarrel(float *amount);
bool AP_SetPostVignette(float *amount);

/* Film */
bool AP_SetPostGrain(float *amount);
bool AP_SetPostFilmGrain(float *amount);
bool AP_SetPostFilmResponse(float *amount);
bool AP_SetPostHalation(float *amount);

/* Atmospheric */
bool AP_SetPostFog(float *amount);
bool AP_SetPostFogDensity(float *density);
bool AP_SetPostFogHeight(float *height);

/* Depth-based effects */
bool AP_SetPostDepthOfField(float *amount);
bool AP_SetPostDOFFocus(float *focus);
bool AP_SetPostDOFAperture(float *aperture);

/* Motion */
bool AP_SetPostMotionBlur(float *amount);

/* =============================================================
 * CREATIVE / STYLIZED
 * ============================================================= */

/* Resolution / geometry-like */
bool AP_SetPostPixelate(float *pixels);
bool AP_SetPostPosterize(float *levels);

/* Retro */
bool AP_SetPostScanlines(float *amount);
bool AP_SetPostCRT(float *amount);
bool AP_SetPostVHS(float *amount);
bool AP_SetPostRGBSplit(float *amount);

/* Color */
bool AP_SetPostSepia(float *amount);
bool AP_SetPostGrayscale(float *amount);
bool AP_SetPostInvert(float *amount);
bool AP_SetPostSolarize(float *amount);
bool AP_SetPostColorize(float *amount, float *hue);

/* Stylization */
bool AP_SetPostEdge(float *amount);
bool AP_SetPostOutline(float *amount);
bool AP_SetPostCelShade(float *amount);
bool AP_SetPostPosterize(float *levels);
bool AP_SetPostDither(float *amount);
bool AP_SetPostHalftone(float *amount);

/* Glitch */
bool AP_SetPostGlitch(float *amount);
bool AP_SetPostNoise(float *amount);
bool AP_SetPostDisplacement(float *amount);

/* Distortion */
bool AP_SetPostKaleidoscope(float *amount);
bool AP_SetPostWave(float *amount);
bool AP_SetPostRipple(float *amount);
bool AP_SetPostFisheye(float *amount);

/* =============================================================
 * QUALITY / MASTER CONTROLS
 * ============================================================= */

bool AP_SetPostEnabled(bool enabled);

/* Master strength for the complete post stack. */
bool AP_SetPostIntensity(float *intensity);

/* Enable/disable individual effects. */
bool AP_SetPostEffectEnabled(
    const char *effect,
    bool enabled
);

#ifdef __cplusplus
}
#endif

#endif