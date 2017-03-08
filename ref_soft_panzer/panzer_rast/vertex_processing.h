#ifndef VERTEX_PROCESSING_H
#define VERTEX_PROCESSING_H

//input vaiables for VertexProcessing::DrawTriangleToBuffer
namespace VertexProcessing
{

extern int triangle_in_vertex_xy[];//screen space x and y
extern fixed16_t triangle_in_vertex_z[];//screen space z
extern unsigned char triangle_in_color[];
extern int triangle_in_light[];
extern fixed16_t triangle_in_tex_coord[];
extern fixed16_t triangle_in_lightmap_tex_coord[];

//culling output variables
extern int cull_passed_vertices[2];
extern int cull_lost_vertices[2];
extern int cull_new_vertices_neighbors[4];
extern float cull_new_vertices_interpolation_k[2];

int CullTriangleByZNearPlane( float z0, float z1, float z2 );

}

#endif//VERTEX_PROCESSING_H