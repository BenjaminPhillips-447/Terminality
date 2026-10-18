#ifndef TERMINALITY_UNITS_H
#define TERMINALITY_UNITS_H

#include <stddef.h>
#include <stdbool.h>

typedef struct
{
    int x;
    int y;
} iVec2;

typedef struct
{
    float x;
    float y;
    float z;
} Vec3;

Vec3 vec3Add(Vec3 left, Vec3 right);
Vec3 vec3Multiply(Vec3 vector, float scalar);
Vec3 vec3Subtract(Vec3 left, Vec3 right);
float vec3Length(Vec3 in);
Vec3 vec3Cross(Vec3 left, Vec3 right);
float vec3Dot(Vec3 left, Vec3 right);
Vec3 vec3Normalize(Vec3 vector);
Vec3 vec3Negate(Vec3 vector);
Vec3 rotateAroundAxis(Vec3 axis, float angle, Vec3 input);


typedef struct
{
    float m[3][3];
} Mat3x3;

typedef struct
{
    float m[4][4];
} Mat4x4;

typedef struct
{
    Vec3 normal;
    Vec3 point;
} Plane;

typedef struct
{
    Plane sides[4];
    Plane near;
    Plane far;
} Frutsum;

typedef struct
{
    Mat4x4 base;
} PerspectiveMatrix;

typedef struct
{
    Mat4x4 base;
} OrthographicMatrix;

void perspectiveMatrixSetPerspective(PerspectiveMatrix *matrix, Frutsum *viewFrustum, float fov, float aspectRatio, float nearPlane, float farPlane);
void orthographicMatrixSetOrthographic(OrthographicMatrix *matrix, float left, float right, float bottom, float top, float nearPlane, float farPlane);
Vec3 mat4x4MultiplyVec3(const Mat4x4 *matrix, Vec3 vector);

typedef struct
{
    Vec3 position;
    Vec3 direction;
    Vec3 up;
    Frutsum clippingFrutsum;
} Camera;

void pointAt(Camera *camera, Vec3 pos, Vec3 target, Vec3 up);
Mat4x4 getPerspectiveCameraMatrix(const Camera *camera);

typedef struct
{
    Vec3 p[3];
    Vec3 normal;
} Triangle;

typedef struct
{
    Triangle *triangles;
    size_t triangleCount;
} Renderable;

typedef struct // I know a multiTriangle is the same as a renderable its just so i dont accidentally do something stupid and free a renderable
{
    Triangle *triangles;
    size_t triangleCount;
} MultiTriangle;

typedef struct
{
    Vec3 p1;
    Vec3 p2;
} Line;

/* On success, the caller owns outputTriangles->triangles and must free it. */
bool clipTriangleAgainstFrutsum(Frutsum frustum, Triangle triangle, MultiTriangle *outputTriangles);

typedef struct
{
    Vec3 position;
    Vec3 color;
    float intensity;
    float range;
} Light;

typedef struct
{
    Renderable *renderables;
    size_t renderableCount;

    Light *lights;
    size_t lightCount;
} Scene;

#endif