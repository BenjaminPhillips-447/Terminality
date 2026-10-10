#ifndef TERMINALITY_UNITS_H
#define TERMINALITY_UNITS_H

#include <stddef.h>

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
Vec3 vec3AddAssign(Vec3 *vector, Vec3 other);
Vec3 vec3SubtractAssign(Vec3 *vector, Vec3 other);
Vec3 vec3Cross(Vec3 left, Vec3 right);
float vec3Dot(Vec3 left, Vec3 right);
Vec3 vec3Normalize(Vec3 vector);

typedef struct
{
    float m[3][3];
} Mat3x3;

Mat3x3 mat3x3MultiplyMat3x3(const Mat3x3 *left, const Mat3x3 *right);

Mat3x3 rotateAroundAxis(const Vec3 *axis, float angle);


typedef struct
{
    float m[4][4];
} Mat4x4;

typedef struct
{
    Mat4x4 base;
} PerspectiveMatrix;

typedef struct
{
    Mat4x4 base;
} OrthographicMatrix;

void perspectiveMatrixSetPerspective(PerspectiveMatrix *matrix, float fov, float aspectRatio, float nearPlane, float farPlane);
void orthographicMatrixSetOrthographic(OrthographicMatrix *matrix, float left, float right, float bottom, float top, float nearPlane, float farPlane);
Vec3 mat4x4MultiplyVec3(const Mat4x4 *matrix, Vec3 vector);

typedef struct
{
    Vec3 position;
    Vec3 direction;
    Vec3 up;
} Camera;

Mat4x4 getPerspectiveCameraMatrix(const Camera *camera);

typedef struct
{
    Vec3 p[3];
} triangle;

typedef struct
{
    triangle *triangles;
    size_t triangleCount;
} Renderable;

#endif