#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

static char *frameBuffer;
static float *depthBuffer;
static uint8_t *colorBuffer;
static size_t frameBufferSize;
static int frameWidth;
static int frameHeight;
static const float fontAspectRatio = 2.5f;
static const char shadingGradient[] = {'.', ':', '-', '+', '*', '=', '#', '%', '@'};


static uint8_t closestColorLevel(float channel)
{
    static const uint8_t colorLevels[] = {0, 95, 135, 175, 215, 255};
    uint8_t closestIndex = 0;
    float closestDistance = channel * channel;

    for (uint8_t i = 1; i < 6; ++i)
    {
        float difference = channel - (float)colorLevels[i];
        float distance = difference * difference;
        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestIndex = i;
        }
    }

    return closestIndex;
}

static uint8_t rgbToAnsi256(uint8_t red, uint8_t green, uint8_t blue)
{
    static const uint8_t colorLevels[] = {0, 95, 135, 175, 215, 255};
    uint8_t redIndex = closestColorLevel(red);
    uint8_t greenIndex = closestColorLevel(green);
    uint8_t blueIndex = closestColorLevel(blue);
    int redDifference = (int)red - colorLevels[redIndex];
    int greenDifference = (int)green - colorLevels[greenIndex];
    int blueDifference = (int)blue - colorLevels[blueIndex];
    int bestDistance = redDifference * redDifference +
        greenDifference * greenDifference + blueDifference * blueDifference;
    uint8_t bestIndex = (uint8_t)(16 + 36 * redIndex + 6 * greenIndex + blueIndex);

    for (uint8_t grayIndex = 0; grayIndex < 24; ++grayIndex)
    {
        int gray = 8 + 10 * grayIndex;
        int grayRedDifference = (int)red - gray;
        int grayGreenDifference = (int)green - gray;
        int grayBlueDifference = (int)blue - gray;
        int distance = grayRedDifference * grayRedDifference + grayGreenDifference * grayGreenDifference + grayBlueDifference * grayBlueDifference;
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = (uint8_t)(232 + grayIndex);
        }
    }

    return bestIndex;
}

static uint8_t Vec3ToAnsi256(Vec3 colour)
{
    uint8_t red = (uint8_t)fminf(255.0f, fmaxf(0.0f, colour.x * 255.0f));
    uint8_t green = (uint8_t)fminf(255.0f, fmaxf(0.0f, colour.y * 255.0f));
    uint8_t blue = (uint8_t)fminf(255.0f, fmaxf(0.0f, colour.z * 255.0f));
    return rgbToAnsi256(red, green, blue);
}

void drawTrianglePerspective(Triangle tri, Light *lights, size_t lightCount, Mat4x4 projectionMatrix, Mat4x4 viewMatrix)
{
    if (frameBuffer == NULL || frameWidth <= 0 || frameHeight <= 0)
    {
        return;
    }

    Vec3 projected[3];
    for (int i = 0; i < 3; ++i)
    {
        Vec3 viewSpace = mat4x4MultiplyVec3(&viewMatrix, tri.p[i]);
        viewSpace.y /= fontAspectRatio;
        projected[i] = mat4x4MultiplyVec3(&projectionMatrix, viewSpace);
        if (!isfinite(projected[i].x) || !isfinite(projected[i].y) || !isfinite(projected[i].z))
        {
            return;
        }
    }

    float screenX[3];
    float screenY[3];
    for (int i = 0; i < 3; ++i)
    {
        screenX[i] = (projected[i].x + 1.0f) * 0.5f * (float)(frameWidth - 1);
        screenY[i] = (1.0f - projected[i].y) * 0.5f * (float)(frameHeight - 1);
    }

    float area = (screenX[1] - screenX[0]) * (screenY[2] - screenY[0]) - (screenY[1] - screenY[0]) * (screenX[2] - screenX[0]);
    if (fabsf(area) < 0.000001f)
    {
        return;
    }

    float minX = fminf(screenX[0], fminf(screenX[1], screenX[2]));
    float maxX = fmaxf(screenX[0], fmaxf(screenX[1], screenX[2]));
    float minY = fminf(screenY[0], fminf(screenY[1], screenY[2]));
    float maxY = fmaxf(screenY[0], fmaxf(screenY[1], screenY[2]));
    int startX = (int)fmaxf(0.0f, fminf((float)(frameWidth - 1), floorf(minX)));
    int endX = (int)fmaxf(0.0f, fminf((float)(frameWidth - 1), ceilf(maxX)));
    int startY = (int)fmaxf(0.0f, fminf((float)(frameHeight - 1), floorf(minY)));
    int endY = (int)fmaxf(0.0f, fminf((float)(frameHeight - 1), ceilf(maxY)));
    size_t rowSize = (size_t)frameWidth + 1;

    for (int y = startY; y <= endY; ++y)
    {
        for (int x = startX; x <= endX; ++x)
        {
            //pixel center
            float sampleX = (float)x + 0.5f;
            float sampleY = (float)y + 0.5f;

            // Barycentric coordinates
            float x0 = screenX[0], y0 = screenY[0];
            float x1 = screenX[1], y1 = screenY[1];
            float x2 = screenX[2], y2 = screenY[2];

            float denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);

            if (fabsf(denom) < 1e-6f) //degenerate Triangle
                continue;

            float w0 = ((y1 - y2) * (sampleX - x2) + (x2 - x1) * (sampleY - y2)) / denom;
            float w1 = ((y2 - y0) * (sampleX - x2) + (x0 - x2) * (sampleY - y2)) / denom;
            float w2 = 1.0f - w0 - w1;

            // Check if the pixel is inside the triangle
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
            {
                float depth = w0 * projected[0].z + w1 * projected[1].z + w2 * projected[2].z;
                size_t pixelIndex = (size_t)y * (size_t)frameWidth + (size_t)x;
                size_t frameIndex = (size_t)y * rowSize + (size_t)x;
                if (!isfinite(depth) || depth < -1.0f || depth > 1.0f || depth >= depthBuffer[pixelIndex])
                {
                    continue;
                }

                depthBuffer[pixelIndex] = depth;
                Vec3 interpolatedPosition = vec3Add(vec3Add(vec3Multiply(tri.p[0], w0), vec3Multiply(tri.p[1], w1)), vec3Multiply(tri.p[2], w2));
                Vec3 litColour = {0.0f, 0.0f, 0.0f};
                float brightness = 0.0f;
                for (size_t lightIndex = 0; lightIndex < lightCount; ++lightIndex)
                {
                    Light *light = &lights[lightIndex];
                    Vec3 lightOffset = vec3Subtract(light->position, interpolatedPosition);
                    Vec3 lightDir = vec3Normalize(lightOffset);
                    float distance = vec3Length(lightOffset);
                    float rangeRatio = light->range > 0.0f ? distance / light->range : 0.0f;
                    float attenuation = 1.0f / (1.0f + rangeRatio * rangeRatio);
                    float contribution = fmaxf(0.0f, vec3Dot(tri.normal, lightDir)) * light->intensity * attenuation;
                    brightness += contribution;
                    litColour.x += light->color.x * contribution;
                    litColour.y += light->color.y * contribution;
                    litColour.z += light->color.z * contribution;
                }
                colorBuffer[pixelIndex] = Vec3ToAnsi256(litColour);

                //if (w0 < 0.05f || w1 < 0.05f || w2 < 0.05f)
                //{
                //    //wireframe outline
                //    frameBuffer[frameIndex] = '@';
                //}
                //else
                //{
                    float brightnessClamped = fminf(1.0f, fmaxf(0.0f, brightness));
                    size_t gradientIndex = (size_t)(brightnessClamped * (sizeof(shadingGradient) / sizeof(shadingGradient[0]) - 1));
                    // Set the character in the frame buffer based on the calculated brightness
                    frameBuffer[frameIndex] = shadingGradient[gradientIndex];
               // }
            }
        }
    }
}

static Plane transformFrustumPlaneToWorld(Plane plane, Vec3 cameraPosition, Vec3 right, Vec3 up, Vec3 backward)
{
    Vec3 normal = vec3Add(
        vec3Add(vec3Multiply(right, plane.normal.x),
                vec3Multiply(up, plane.normal.y / fontAspectRatio)),
        vec3Multiply(backward, plane.normal.z));
    Vec3 point = vec3Add(
        cameraPosition,
        vec3Add(vec3Multiply(right, plane.point.x),
                vec3Add(vec3Multiply(up, plane.point.y * fontAspectRatio),
                        vec3Multiply(backward, plane.point.z))));
    return (Plane){normal, point};
}

void perspectiveRender(Scene scene, Mat4x4 projectionMatrix, Camera camera)
{
    Vec3 forward = vec3Normalize(camera.direction);
    Vec3 right = vec3Normalize(vec3Cross(forward, camera.up));
    Vec3 up = vec3Cross(right, forward);
    Vec3 backward = vec3Negate(forward);
    Frutsum worldFrustum = camera.clippingFrutsum;
    worldFrustum.near = transformFrustumPlaneToWorld(
        camera.clippingFrutsum.near, camera.position, right, up, backward);
    worldFrustum.far = transformFrustumPlaneToWorld(
        camera.clippingFrutsum.far, camera.position, right, up, backward);
    for (size_t i = 0; i < 4; ++i)
    {
        worldFrustum.sides[i] = transformFrustumPlaneToWorld(
            camera.clippingFrutsum.sides[i], camera.position, right, up, backward);
    }

    Mat4x4 viewMatrix = getPerspectiveCameraMatrix(&camera);
    for (size_t renderableIndex = 0; renderableIndex < scene.renderableCount; ++renderableIndex)
    {
        Renderable renderable = scene.renderables[renderableIndex];

        for (size_t i = 0; i < renderable.triangleCount; ++i)
        {
            Triangle tri = renderable.triangles[i];
            MultiTriangle triangles;
            if (clipTriangleAgainstFrutsum(worldFrustum, tri, &triangles))
            {
                for (size_t clippedIndex = 0; clippedIndex < triangles.triangleCount; ++clippedIndex)
                {
                    drawTrianglePerspective(
                        triangles.triangles[clippedIndex], scene.lights, scene.lightCount, projectionMatrix, viewMatrix);
                }
                free(triangles.triangles);
            }
        }
    }
}

//void orthographicRender(Triangle *triangles, int triangle_count, )
//{
//    (void)triangles;
//    (void)triangle_count;
//}

int blankFrame(iVec2 terminalSize)
{
    if (terminalSize.x <= 0 || terminalSize.y <= 0)
    {
        return -1;
    }

    size_t rowSize = (size_t)terminalSize.x + 1;
    if ((size_t)terminalSize.y > (SIZE_MAX - 1) / rowSize)
    {
        return -1;
    }

    size_t requiredSize = rowSize * (size_t)terminalSize.y;
    char *newFrameBuffer = realloc(frameBuffer, requiredSize + 1);
    if (newFrameBuffer == NULL)
    {
        return -1;
    }

    size_t pixelCount = (size_t)terminalSize.x * (size_t)terminalSize.y;
    if (pixelCount > SIZE_MAX / sizeof(*depthBuffer))
    {
        frameBuffer = newFrameBuffer;
        return -1;
    }

    float *newDepthBuffer = realloc(depthBuffer, pixelCount * sizeof(*depthBuffer));
    if (newDepthBuffer == NULL)
    {
        frameBuffer = newFrameBuffer;
        return -1;
    }

    uint8_t *newColorBuffer = realloc(colorBuffer, pixelCount * sizeof(*colorBuffer));
    if (newColorBuffer == NULL)
    {
        colorBuffer = newColorBuffer;
        return -1;
    }

    frameBuffer = newFrameBuffer;
    depthBuffer = newDepthBuffer;
    colorBuffer = newColorBuffer;

    frameBufferSize = requiredSize;
    frameWidth = terminalSize.x;
    frameHeight = terminalSize.y;

    for (size_t i = 0; i < pixelCount; ++i)
    {
        depthBuffer[i] = INFINITY;
        colorBuffer[i] = 0;
    }

    for (int y = 0; y < frameHeight; ++y)
    {
        size_t rowStart = (size_t)y * rowSize;
        memset(frameBuffer + rowStart, ' ', (size_t)frameWidth);
        frameBuffer[rowStart + (size_t)frameWidth] = '\n';
    }
    frameBuffer[frameBufferSize] = '\0';
    return 0;
}

void finishFrame(void)
{
    if (frameBuffer == NULL)
    {
        return;
    }

    size_t outputCapacity = frameBufferSize * 20u + 64u;
    char *outputBuffer = malloc(outputCapacity);
    if (outputBuffer == NULL)
    {
        return;
    }

    size_t outputIndex = 0;
    static const char homeAndClear[] = "\033[H\033[2J";
    static const char resetEscape[] = "\033[0m";

    memcpy(outputBuffer + outputIndex, homeAndClear, sizeof(homeAndClear) - 1u);
    outputIndex += sizeof(homeAndClear) - 1u;

    size_t rowSize = (size_t)frameWidth + 1u;
    int activeColour = -1;
    for (int y = 0; y < frameHeight; ++y)
    {
        for (int x = 0; x < frameWidth; ++x)
        {
            size_t frameIndex = (size_t)y * rowSize + (size_t)x;
            size_t pixelIndex = (size_t)y * (size_t)frameWidth + (size_t)x;
            char current = frameBuffer[frameIndex];

            if (current != ' ' && colorBuffer != NULL && colorBuffer[pixelIndex] != activeColour)
            {
                int written = snprintf(
                    outputBuffer + outputIndex,
                    outputCapacity - outputIndex,
                    "\033[38;5;%um",
                    (unsigned)colorBuffer[pixelIndex]);
                if (written < 0)
                {
                    free(outputBuffer);
                    return;
                }
                outputIndex += (size_t)written;
                activeColour = colorBuffer[pixelIndex];
            }

            outputBuffer[outputIndex++] = current;
        }

        outputBuffer[outputIndex++] = '\n';
    }

    memcpy(outputBuffer + outputIndex, resetEscape, sizeof(resetEscape) - 1u);
    outputIndex += sizeof(resetEscape) - 1u;

    fwrite(outputBuffer, 1, outputIndex, stdout);
    fflush(stdout);
    free(outputBuffer);
}

void freeFrameBuffer(void)
{
    free(frameBuffer);
    frameBuffer = NULL;
    free(depthBuffer);
    depthBuffer = NULL;
    free(colorBuffer);
    colorBuffer = NULL;
    frameBufferSize = 0;
    frameWidth = 0;
    frameHeight = 0;
}

void initializeRenderer(void)
{
    frameBuffer = NULL;
    depthBuffer = NULL;
    colorBuffer = NULL;
    frameBufferSize = 0;
    frameWidth = 0;
    frameHeight = 0;
}

int main(void)
{
    initializeRenderer();

    Camera camera = {
        .position = {0.0f, 1.0f, 5.0f},
        .direction = {0.0f, 0.0f, -1.0f},
        .up = {0.0f, 1.0f, 0.0f}
    };

    PerspectiveMatrix perspectiveMatrix;
    //OrthographicMatrix orthographicMatrix;

    Vec3 tetrahedronVertices[4] = {
        { 1.0f,  1.0f,  1.0f},
        {-1.0f, -1.0f,  1.0f},
        {-1.0f,  1.0f, -1.0f},
        { 1.0f, -1.0f, -1.0f}
    };
    const size_t tetrahedronFaceIndices[4][3] = {
        {0, 1, 2},
        {0, 3, 1},
        {0, 2, 3},
        {1, 3, 2}
    };
    Triangle tetrahedronTriangles[4];
    for (size_t i = 0; i < sizeof(tetrahedronTriangles) / sizeof(tetrahedronTriangles[0]); ++i)
    {
        tetrahedronTriangles[i].p[0] = tetrahedronVertices[tetrahedronFaceIndices[i][0]];
        tetrahedronTriangles[i].p[1] = tetrahedronVertices[tetrahedronFaceIndices[i][1]];
        tetrahedronTriangles[i].p[2] = tetrahedronVertices[tetrahedronFaceIndices[i][2]];

        Vec3 edge1 = vec3Subtract(tetrahedronTriangles[i].p[1], tetrahedronTriangles[i].p[0]);
        Vec3 edge2 = vec3Subtract(tetrahedronTriangles[i].p[2], tetrahedronTriangles[i].p[0]);
        Vec3 normal = vec3Cross(edge1, edge2);
        Vec3 centerDirection = vec3Add(vec3Add(tetrahedronTriangles[i].p[0], tetrahedronTriangles[i].p[1]), tetrahedronTriangles[i].p[2]);
        if (vec3Dot(normal, centerDirection) < 0.0f)
        {
            Vec3 temp = tetrahedronTriangles[i].p[1];
            tetrahedronTriangles[i].p[1] = tetrahedronTriangles[i].p[2];
            tetrahedronTriangles[i].p[2] = temp;
            normal = vec3Multiply(normal, -1.0f);
        }
        tetrahedronTriangles[i].normal = vec3Normalize(normal);
    }
    Renderable tetrahedron = {tetrahedronTriangles, sizeof(tetrahedronTriangles) / sizeof(tetrahedronTriangles[0])};
    Renderable renderables[] = {tetrahedron};
    size_t renderablesLength = 1;

    Light light = {
        .position = {5.0f, 5.0f, 5.0f},
        .color = {1.0f, 0.0f, 0.0f},
        .intensity = 1.0f,
        .range = 40.0f
    };

    Light light2 = {
        .position = {-5.0f, 5.0f, -5.0f},
        .color = {0.0f, 0.0f, 1.0f},
        .intensity = 1.0f,
        .range = 32.0f
    };

    Light lights[] = {light, light2};
    size_t lightsLength = 2;

    Scene scene = {
        .renderables = renderables,
        .renderableCount = renderablesLength,
        .lights = lights,
        .lightCount = lightsLength
    };
    if (beginFrame() != 0)
    {
        fputs("Failed to enable terminal ANSI processing.\n", stderr);
        return 1;
    }

    for (int i = 0; i < 400; ++i)
    {
        iVec2 terminal_size = getTerminalSize();
        if (blankFrame(terminal_size) != 0)
        {
            fputs("Failed to clear terminal frame.\n", stderr);
            return 1;
        }

        camera.position.x = 2.0f * sinf((float)i * 0.03f);
        camera.position.y = 1.0f;
        camera.position.z = 2.0f * cosf((float)i * 0.03f);

        pointAt(&camera, camera.position, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){0.0f, 1.0f, 0.0f});



        float aspectRatio = (float)terminal_size.x / (float)terminal_size.y;
        perspectiveMatrixSetPerspective(
            &perspectiveMatrix,
            &camera.clippingFrutsum,
            3.14159265358979323846f / 6.0f,
            aspectRatio,
            0.1f,
            100.0f);

        perspectiveRender(scene, perspectiveMatrix.base, camera);

        finishFrame();
        sleepMS(32);
    }

    puts("Done.");
    return 0;
}
