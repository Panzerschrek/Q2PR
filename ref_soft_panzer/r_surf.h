#include "r_local.h"

#ifdef __cplusplus
extern "C"
{
#endif

panzer_surf_cache_t* GenerateSurfaceCache( msurface_t* surf, image_t* current_surf_image, int mip );
void InitSurfaceCache();
void ShutdownSurfaceCache();
int IsSurfaceCachable( msurface_t* surf );//really bool
void BeginSurfFrame();

#ifdef __cplusplus
//extern "C"
}
#endif