#ifndef LIGHT_H
#define LIGHT_H
#include "r_local.h"
#include "r_model.h"

#ifdef __cplusplus
extern "C"
{
#endif

void R_PushDlights (model_t *model);
unsigned char* L_GetSurfaceDynamicLightmap( msurface_t* surf );
void R_LightInit();
void R_SwapLightmapBuffers();

#ifdef __cplusplus
//extern "C"
}
#endif

#endif//LIGHT_H