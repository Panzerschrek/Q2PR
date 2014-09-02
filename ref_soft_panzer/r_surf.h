#include "r_local.h"

#ifdef __cplusplus
extern "C"
{
#endif

void GenerateSurfaceCache( msurface_t* surf, image_t* current_surf_image, int mip );
void InitSurfaceCache();
void ShutdownSurfaceCache();
int IsSurfaceCachable( msurface_t* surf );//really bool


#ifdef __cplusplus
//extern "C"
}
#endif