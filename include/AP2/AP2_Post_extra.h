
#ifndef AP2_POST_EXTRA_H
#define AP2_POST_EXTRA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool AP_SetPostExposure(float exposure);
bool AP_SetPostGamma(float gamma);
bool AP_SetPostFilmic(float amount);
bool AP_SetPostPixelate(float pixels);
bool AP_SetPostScanlines(float amount);
bool AP_SetPostBarrel(float amount);
bool AP_SetPostSepia(float amount);
bool AP_SetPostGrayscale(float amount);
bool AP_SetPostPosterize(float levels);
bool AP_SetPostEdge(float amount);

#ifdef __cplusplus
}
#endif

#endif