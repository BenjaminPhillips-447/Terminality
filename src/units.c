#include <math.h>
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
{
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