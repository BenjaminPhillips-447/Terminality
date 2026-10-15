#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "units.h"

Vec3 vec3Add(Vec3 left, Vec3 right)
{
    Vec3 result = {left.x + right.x, left.y + right.y, left.z + right.z};
    return result;
}

Vec3 vec3Multiply(Vec3 vector, float scalar)
{
    Vec3 result = {vector.x * scalar, vector.y * scalar, vector.z * scalar};
    return result;
}

Vec3 vec3Subtract(Vec3 left, Vec3 right)
{
    Vec3 result = {left.x - right.x, left.y - right.y, left.z - right.z};
    return result;
}

float vec3Length(Vec3 in)
{
    return sqrtf(in.x * in.x + in.y * in.y + in.z * in.z);
}

Vec3 vec3Cross(Vec3 left, Vec3 right)
{
    Vec3 result = {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
    return result;
}

float vec3Dot(Vec3 left, Vec3 right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 vec3Normalize(Vec3 vector)
{
    float length = sqrtf(vec3Dot(vector, vector));
    if (length == 0.0f)
    {
        Vec3 zero = {0.0f, 0.0f, 0.0f};
        return zero;
    }

    Vec3 normalized = {vector.x / length, vector.y / length, vector.z / length};
    return normalized;
}

Vec3 vec3Negate(Vec3 vector)
{
    Vec3 negated = {-vector.x, -vector.y, -vector.z};
    return negated;
}

static bool vec3Equals(Vec3 left, Vec3 right)
{
    return fabsf(left.x - right.x) < 0.001f &&
           fabsf(left.y - right.y) < 0.001f &&
           fabsf(left.z - right.z) < 0.001f;
}

Vec3 mat4x4MultiplyVec3(const Mat4x4 *matrix, Vec3 vector)
{
    Vec3 result = {
        vector.x * matrix->m[0][0] + vector.y * matrix->m[1][0] +
            vector.z * matrix->m[2][0] + matrix->m[3][0],
        vector.x * matrix->m[0][1] + vector.y * matrix->m[1][1] +
            vector.z * matrix->m[2][1] + matrix->m[3][1],
        vector.x * matrix->m[0][2] + vector.y * matrix->m[1][2] +
            vector.z * matrix->m[2][2] + matrix->m[3][2]
    };
    float w = vector.x * matrix->m[0][3] + vector.y * matrix->m[1][3] +
        vector.z * matrix->m[2][3] + matrix->m[3][3];

    if (w != 0.0f)
    {
        result.x /= w;
        result.y /= w;
        result.z /= w;
    }

    return result;
}


void perspectiveMatrixSetPerspective(
    PerspectiveMatrix *matrix,
    Frutsum *viewFrustum,
    float fov,
    float aspectRatio,
    float nearPlane,
    float farPlane)
{
    memset(&matrix->base, 0, sizeof(matrix->base));
    float f = 1.0f / tanf(fov / 2.0f);
    matrix->base.m[0][0] = f / aspectRatio;
    matrix->base.m[1][1] = f;
    matrix->base.m[2][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
    matrix->base.m[2][3] = -1.0f;
    matrix->base.m[3][2] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);

    float verticalHalfAngle = fov * 0.5f;
    float horizontalHalfAngle = atanf(aspectRatio / f);
    float sh = sinf(horizontalHalfAngle);
    float sv = sinf(verticalHalfAngle);
    float ch = cosf(horizontalHalfAngle);
    float cv = cosf(verticalHalfAngle);

    Vec3 origin = {0.0f, 0.0f, 0.0f};
    Vec3 normal0 = {-ch, 0.0f, -sh};
    Vec3 normal1 = {ch, 0.0f, -sh};
    Vec3 normal2 = {0.0f, -cv, -sv};
    Vec3 normal3 = {0.0f, cv, -sv};

    Vec3 nearNormal = {0.0f, 0.0f, -1.0f};
    Vec3 nearPosition = {0.0f, 0.0f, -nearPlane};
    Vec3 farNormal = {0.0f, 0.0f, 1.0f};
    Vec3 farPosition = {0.0f, 0.0f, -farPlane};
    viewFrustum->sides[0].normal = normal0;
    viewFrustum->sides[1].normal = normal1;
    viewFrustum->sides[2].normal = normal2;
    viewFrustum->sides[3].normal = normal3;

    viewFrustum->sides[0].point = origin;
    viewFrustum->sides[1].point = origin;
    viewFrustum->sides[2].point = origin;
    viewFrustum->sides[3].point = origin;

    viewFrustum->near.normal = nearNormal;
    viewFrustum->near.point = nearPosition;
    viewFrustum->far.normal = farNormal;
    viewFrustum->far.point = farPosition;
}

void orthographicMatrixSetOrthographic(
    OrthographicMatrix *matrix,
    float left,
    float right,
    float bottom,
    float top,
    float nearPlane,
    float farPlane)
{
    memset(&matrix->base, 0, sizeof(matrix->base));
    matrix->base.m[0][0] = 2.0f / (right - left);
    matrix->base.m[1][1] = 2.0f / (top - bottom);
    matrix->base.m[2][2] = -2.0f / (farPlane - nearPlane);
    matrix->base.m[3][0] = -(right + left) / (right - left);
    matrix->base.m[3][1] = -(top + bottom) / (top - bottom);
    matrix->base.m[3][2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    matrix->base.m[3][3] = 1.0f;

    //todo: add frutsum for orthographic


    //on second thought todo: add orthographic
}

void pointAt(Camera *camera, Vec3 pos, Vec3 target, Vec3 up)
{
    Vec3 forward = vec3Normalize(vec3Subtract(target, pos));
    Vec3 right = vec3Normalize(vec3Cross(up, forward));
    Vec3 newUp = vec3Cross(forward, right);
    
    camera->position = pos;
    camera->direction = forward;
    camera->up = newUp;
}

Mat4x4 getPerspectiveCameraMatrix(const Camera *camera)
{ //refresh matrix with new look direction and camera rotations
    Vec3 forward = vec3Normalize(camera->direction);
    Vec3 right = vec3Normalize(vec3Cross(forward, camera->up));
    Vec3 up = vec3Cross(right, forward);
    Vec3 backward = vec3Multiply(forward, -1.0f);
    Mat4x4 view = {0};

    view.m[0][0] = right.x;
    view.m[1][0] = right.y;
    view.m[2][0] = right.z;
    view.m[0][1] = up.x;
    view.m[1][1] = up.y;
    view.m[2][1] = up.z;
    view.m[0][2] = backward.x;
    view.m[1][2] = backward.y;
    view.m[2][2] = backward.z;
    view.m[3][0] = -vec3Dot(right, camera->position);
    view.m[3][1] = -vec3Dot(up, camera->position);
    view.m[3][2] = -vec3Dot(backward, camera->position);
    view.m[3][3] = 1.0f;
    return view;
}

static bool degenerateTriangle(Triangle triangle)
{
    if (vec3Equals(triangle.p[0], triangle.p[1]) ||
        vec3Equals(triangle.p[1], triangle.p[2]) ||
        vec3Equals(triangle.p[2], triangle.p[0]))
    {
        return true;
    }

    Vec3 edge1 = vec3Subtract(triangle.p[1], triangle.p[0]);
    Vec3 edge2 = vec3Subtract(triangle.p[2], triangle.p[0]);
    return vec3Length(vec3Cross(edge1, edge2)) < 1e-6f;
}

static bool clipTriangleAgainstPlane(Plane plane, Triangle triangle, Triangle output[2], size_t *outputCount)
{
    Vec3 polygon[6];
    float distances[3];
    for (size_t i = 0; i < 3; ++i)
    {
        distances[i] = vec3Dot(plane.normal, vec3Subtract(triangle.p[i], plane.point));
    }

    size_t polygonCount = 0;
    for (size_t i = 0; i < 3; ++i)
    {
        size_t previous = (i + 2) % 3;
        bool currentInside = distances[i] >= 0.0f;
        bool previousInside = distances[previous] >= 0.0f;

        if (currentInside != previousInside)
        {
            float t = distances[previous] / (distances[previous] - distances[i]);
            Vec3 edge = vec3Subtract(triangle.p[i], triangle.p[previous]);
            polygon[polygonCount++] = vec3Add(triangle.p[previous], vec3Multiply(edge, t));
        }
        if (currentInside)
        {
            polygon[polygonCount++] = triangle.p[i];
        }
    }

    if (polygonCount < 3)
    {
        *outputCount = 0;
        return false;
    }

    *outputCount = 0;
    for (size_t i = 1; i + 1 < polygonCount; ++i)
    {
        Triangle clipped = {{polygon[0], polygon[i], polygon[i + 1]}, triangle.normal};
        if (!degenerateTriangle(clipped))
        {
            output[(*outputCount)++] = clipped;
        }
    }
    return *outputCount > 0;
}

static bool clipMultiTriangleAgainstPlane(Plane plane, MultiTriangle *triangles)
{
    if (triangles->triangleCount == 0 ||
        triangles->triangleCount > SIZE_MAX / 2 ||
        triangles->triangleCount * 2 > SIZE_MAX / sizeof(*triangles->triangles))
    {
        free(triangles->triangles);
        *triangles = (MultiTriangle){NULL, 0};
        return false;
    }

    size_t capacity = triangles->triangleCount * 2;
    Triangle *clippedTriangles = malloc(capacity * sizeof(*clippedTriangles));
    if (clippedTriangles == NULL)
    {
        free(triangles->triangles);
        *triangles = (MultiTriangle){NULL, 0};
        return false;
    }

    size_t clippedCount = 0;
    for (size_t i = 0; i < triangles->triangleCount; ++i)
    {
        Triangle clipped[2];
        size_t clippedTriangleCount = 0;
        clipTriangleAgainstPlane(plane, triangles->triangles[i], clipped, &clippedTriangleCount);
        for (size_t j = 0; j < clippedTriangleCount; ++j)
        {
            clippedTriangles[clippedCount++] = clipped[j];
        }
    }

    free(triangles->triangles);
    if (clippedCount == 0)
    {
        free(clippedTriangles);
        *triangles = (MultiTriangle){NULL, 0};
        return false;
    }

    *triangles = (MultiTriangle){clippedTriangles, clippedCount};
    return true;
}

bool clipTriangleAgainstFrutsum(Frutsum frustum, Triangle triangle, MultiTriangle *outputTriangles)
{
    *outputTriangles = (MultiTriangle){NULL, 0};
    Triangle *triangles = malloc(sizeof(*triangles));
    if (triangles == NULL)
    {
        return false;
    }

    triangles[0] = triangle;
    MultiTriangle clippedTriangles = {triangles, 1};
    if (!clipMultiTriangleAgainstPlane(frustum.near, &clippedTriangles))
    {
        return false;
    }

    if (!clipMultiTriangleAgainstPlane(frustum.far, &clippedTriangles))
    {
        return false;
    }

    for (size_t i = 0; i < 4; ++i)
    {
        if (!clipMultiTriangleAgainstPlane(frustum.sides[i], &clippedTriangles))
        {
            return false;
        }
    }

    *outputTriangles = clippedTriangles;
    return true;
}