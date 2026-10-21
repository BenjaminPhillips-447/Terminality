#include <math.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform.h"

static char *frameBuffer;
static float *depthBuffer;
static uint8_t *colorBuffer;
static size_t frameBufferSize;
static char *previousFrameBuffer;
static uint8_t *previousColorBuffer;
static size_t previousFramePixelCount;
static int previousFrameWidth;
static int previousFrameHeight;
static int previousFrameValid;
static int frameWidth;
static int frameHeight;
static const float fontAspectRatio = 2.5f;
static const char shadingGradient[] = {'.', ':', '-', '+', '*', '=', '#', '%', '@'};

static int loadObjFile(const char *path, Vec3 **vertices, size_t *vertexCount, size_t **indices, size_t *indexCount);
static int doRender(const Vec3 *vertices, size_t vertexCount, const size_t *indices, size_t indexCount);

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
            if (vec3Dot(tri.normal, forward) < 0)
            {
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

    if (pixelCount != previousFramePixelCount)
    {
        char *newPreviousFrameBuffer = realloc(previousFrameBuffer, pixelCount);
        if (newPreviousFrameBuffer == NULL)
        {
            frameBuffer = newFrameBuffer;
            return -1;
        }
        previousFrameBuffer = newPreviousFrameBuffer;

        uint8_t *newPreviousColorBuffer = realloc(previousColorBuffer, pixelCount);
        if (newPreviousColorBuffer == NULL)
        {
            frameBuffer = newFrameBuffer;
            return -1;
        }
        previousColorBuffer = newPreviousColorBuffer;
    }
    if (terminalSize.x != previousFrameWidth || terminalSize.y != previousFrameHeight)
    {
        previousFrameValid = 0;
    }
    previousFramePixelCount = pixelCount;
    previousFrameWidth = terminalSize.x;
    previousFrameHeight = terminalSize.y;

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

double finishFrame(bool reRenderFrame, double lastFrameTime)
{
    if (frameBuffer == NULL)
    {
        return lastFrameTime;
    }

    if (frameBufferSize > (SIZE_MAX - 512u) / 40u)
    {
        return lastFrameTime;
    }

    size_t outputCapacity = frameBufferSize * 40u + 512u;
    char *outputBuffer = malloc(outputCapacity);
    if (outputBuffer == NULL)
    {
        return lastFrameTime;
    }

    size_t outputIndex = 0;
    static const char resetEscape[] = "\033[0m";
    size_t rowSize = (size_t)frameWidth + 1u;

    if (reRenderFrame)
    {
        for (int y = 0; y < frameHeight; ++y)
        {
            int x = 0;
            while (x < frameWidth)
            {
                size_t pixelIndex = (size_t)y * (size_t)frameWidth + (size_t)x;
                size_t frameIndex = (size_t)y * rowSize + (size_t)x;
                int changed = !previousFrameValid ||
                    frameBuffer[frameIndex] != previousFrameBuffer[pixelIndex] ||
                    (frameBuffer[frameIndex] != ' ' && colorBuffer[pixelIndex] != previousColorBuffer[pixelIndex]);
                if (!changed)
                {
                    ++x;
                    continue;
                }

                int runStart = x;
                do
                {
                    ++x;
                    if (x >= frameWidth)
                    {
                        break;
                    }
                    pixelIndex = (size_t)y * (size_t)frameWidth + (size_t)x;
                    frameIndex = (size_t)y * rowSize + (size_t)x;
                    changed = !previousFrameValid ||
                        frameBuffer[frameIndex] != previousFrameBuffer[pixelIndex] ||
                        (frameBuffer[frameIndex] != ' ' &&
                            colorBuffer[pixelIndex] != previousColorBuffer[pixelIndex]);
                } while (changed);

                int cursorWritten = snprintf(
                    outputBuffer + outputIndex,
                    outputCapacity - outputIndex,
                    "\033[%d;%dH",
                    y + 1,
                    runStart + 1);
                if (cursorWritten < 0 || (size_t)cursorWritten >= outputCapacity - outputIndex)
                {
                    free(outputBuffer);
                    return lastFrameTime;
                }
                outputIndex += (size_t)cursorWritten;

                int activeColour = -1;
                for (int runX = runStart; runX < x; ++runX)
                {
                    pixelIndex = (size_t)y * (size_t)frameWidth + (size_t)runX;
                    frameIndex = (size_t)y * rowSize + (size_t)runX;
                    if (frameBuffer[frameIndex] != ' ' &&
                        colorBuffer[pixelIndex] != activeColour)
                    {
                        int colourWritten = snprintf(
                            outputBuffer + outputIndex,
                            outputCapacity - outputIndex,
                            "\033[38;5;%um",
                            (unsigned)colorBuffer[pixelIndex]);
                        if (colourWritten < 0 ||
                            (size_t)colourWritten >= outputCapacity - outputIndex)
                        {
                            free(outputBuffer);
                            return lastFrameTime;
                        }
                        outputIndex += (size_t)colourWritten;
                        activeColour = colorBuffer[pixelIndex];
                    }
                    outputBuffer[outputIndex++] = frameBuffer[frameIndex];
                }
            }
        }
    }

    double now = getMonotonicSeconds();
    double elapsedSeconds = now - lastFrameTime;
    double fps = elapsedSeconds > 0.0 ? 1.0 / elapsedSeconds : 0.0;

    memcpy(outputBuffer + outputIndex, resetEscape, sizeof(resetEscape) - 1u);
    outputIndex += sizeof(resetEscape) - 1u;

    char status[256];
    int statusLength = snprintf(
        status,
        sizeof(status),
        "Operating System : %s   Terminal Size : (%i, %i)   FPS : %f",
        platformName(), frameWidth, frameHeight + 1, fps);
    if (statusLength < 0 || (size_t)statusLength >= sizeof(status))
    {
        free(outputBuffer);
        return lastFrameTime;
    }
    int statusWritten = snprintf(
        outputBuffer + outputIndex,
        outputCapacity - outputIndex,
        "\033[%d;1H",
        frameHeight + 1);
    if (statusWritten < 0 || (size_t)statusWritten >= outputCapacity - outputIndex)
    {
        free(outputBuffer);
        return lastFrameTime;
    }
    outputIndex += (size_t)statusWritten;
    size_t statusRowLength = (size_t)statusLength < (size_t)frameWidth
        ? (size_t)statusLength
        : (size_t)frameWidth;
    memcpy(outputBuffer + outputIndex, status, statusRowLength);
    outputIndex += statusRowLength;
    if (statusRowLength < (size_t)frameWidth)
    {
        memset(outputBuffer + outputIndex, ' ', (size_t)frameWidth - statusRowLength);
        outputIndex += (size_t)frameWidth - statusRowLength;
    }
    fwrite(outputBuffer, 1, outputIndex, stdout);
    fflush(stdout);
    free(outputBuffer);

    if (reRenderFrame)
    {
        for (int y = 0; y < frameHeight; ++y)
        {
            for (int x = 0; x < frameWidth; ++x)
            {
                size_t pixelIndex = (size_t)y * (size_t)frameWidth + (size_t)x;
                size_t frameIndex = (size_t)y * rowSize + (size_t)x;
                previousFrameBuffer[pixelIndex] = frameBuffer[frameIndex];
                previousColorBuffer[pixelIndex] = colorBuffer[pixelIndex];
            }
        }
        previousFrameValid = 1;
    }
    return now;
}

void freeFrameBuffer(void)
{
    free(frameBuffer);
    frameBuffer = NULL;
    free(depthBuffer);
    depthBuffer = NULL;
    free(colorBuffer);
    colorBuffer = NULL;
    free(previousFrameBuffer);
    previousFrameBuffer = NULL;
    free(previousColorBuffer);
    previousColorBuffer = NULL;
    previousFramePixelCount = 0;
    previousFrameWidth = 0;
    previousFrameHeight = 0;
    previousFrameValid = 0;
    frameBufferSize = 0;
    frameWidth = 0;
    frameHeight = 0;
}

void initializeRenderer(void)
{
    frameBuffer = NULL;
    depthBuffer = NULL;
    colorBuffer = NULL;
    previousFrameBuffer = NULL;
    previousColorBuffer = NULL;
    previousFramePixelCount = 0;
    previousFrameWidth = 0;
    previousFrameHeight = 0;
    previousFrameValid = 0;
    frameBufferSize = 0;
    frameWidth = 0;
    frameHeight = 0;
}

static int appendVertex(Vec3 **vertices, size_t *count, size_t *capacity, Vec3 vertex)
{
    if (*count == *capacity)
    {
        size_t newCapacity = *capacity == 0 ? 64 : *capacity * 2;
        if (newCapacity < *capacity || newCapacity > SIZE_MAX / sizeof(**vertices))
        {
            return -1;
        }

        Vec3 *newVertices = realloc(*vertices, newCapacity * sizeof(**vertices));
        if (newVertices == NULL)
        {
            return -1;
        }
        *vertices = newVertices;
        *capacity = newCapacity;
    }

    (*vertices)[(*count)++] = vertex;
    return 0;
}

static int appendTriangleIndices(size_t **indices, size_t *count, size_t *capacity, size_t first, size_t second, size_t third)
{
    if (*count > SIZE_MAX - 3)
    {
        return -1;
    }

    if (*count + 3 > *capacity)
    {
        size_t newCapacity = *capacity == 0 ? 192 : *capacity;
        while (newCapacity < *count + 3)
        {
            if (newCapacity > SIZE_MAX / 2)
            {
                return -1;
            }
            newCapacity *= 2;
        }
        if (newCapacity > SIZE_MAX / sizeof(**indices))
        {
            return -1;
        }

        size_t *newIndices = realloc(*indices, newCapacity * sizeof(**indices));
        if (newIndices == NULL)
        {
            return -1;
        }
        *indices = newIndices;
        *capacity = newCapacity;
    }

    (*indices)[(*count)++] = first;
    (*indices)[(*count)++] = second;
    (*indices)[(*count)++] = third;
    return 0;
}

static int parseObjPositionIndex(const char *token, size_t vertexCount, size_t *index)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(token, &end, 10);
    if (errno != 0 || end == token || (*end != '\0' && *end != '/') || value == 0)
    {
        return -1;
    }

    if (value > 0)
    {
        *index = (size_t)(value - 1);
        return 0;
    }

    unsigned long magnitude = (unsigned long)(-(value + 1)) + 1;
    if (magnitude > vertexCount)
    {
        return -1;
    }
    *index = vertexCount - (size_t)magnitude;
    return 0;
}

static int parseObjFloat(const char *token, float *value)
{
    char *end = NULL;
    errno = 0;
    float parsed = strtof(token, &end);
    if (errno != 0 || end == token || *end != '\0' || !isfinite(parsed))
    {
        return -1;
    }
    *value = parsed;
    return 0;
}

static char *nextObjToken(char **cursor)
{
    while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n')
    {
        ++*cursor;
    }
    if (**cursor == '\0')
    {
        return NULL;
    }

    char *token = *cursor;
    while (**cursor != '\0' && **cursor != ' ' && **cursor != '\t' &&
        **cursor != '\r' && **cursor != '\n')
    {
        ++*cursor;
    }
    if (**cursor != '\0')
    {
        **cursor = '\0';
        ++*cursor;
    }
    return token;
}

static int loadObjFile(const char *path, Vec3 **vertices, size_t *vertexCount, size_t **indices, size_t *indexCount)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Could not open OBJ file '%s'.\n", path);
        return -1;
    }

    *vertices = NULL;
    *vertexCount = 0;
    *indices = NULL;
    *indexCount = 0;
    size_t vertexCapacity = 0;
    size_t indexCapacity = 0;
    size_t lineNumber = 0;
    char line[4096];
    int result = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        ++lineNumber;
        if (strchr(line, '\n') == NULL && !feof(file))
        {
            int character;
            while ((character = fgetc(file)) != '\n' && character != EOF)
            {
            }
            fprintf(stderr, "OBJ line %zu is too long.\n", lineNumber);
            result = -1;
            break;
        }

        char *cursor = line;
        char *token = nextObjToken(&cursor);
        if (token == NULL || token[0] == '#')
        {
            continue;
        }

        if (strcmp(token, "v") == 0)
        {
            Vec3 vertex = {0.0f, 0.0f, 0.0f};
            token = nextObjToken(&cursor);
            int valid = token != NULL && parseObjFloat(token, &vertex.x) == 0;
            token = nextObjToken(&cursor);
            valid = valid && token != NULL && parseObjFloat(token, &vertex.y) == 0;
            token = nextObjToken(&cursor);
            valid = valid && token != NULL && parseObjFloat(token, &vertex.z) == 0;
            if (!valid || appendVertex(vertices, vertexCount, &vertexCapacity, vertex) != 0)
            {
                fprintf(stderr, "Invalid vertex or out of memory at OBJ line %zu.\n", lineNumber);
                result = -1;
                break;
            }
        }
        else if (strcmp(token, "f") == 0)
        {
            size_t first = 0;
            size_t previous = 0;
            size_t faceVertexCount = 0;
            token = nextObjToken(&cursor);
            while (token != NULL && token[0] != '#')
            {
                size_t current;
                if (parseObjPositionIndex(token, *vertexCount, &current) != 0)
                {
                    fprintf(stderr, "Invalid face index at OBJ line %zu.\n", lineNumber);
                    result = -1;
                    break;
                }

                if (faceVertexCount == 0)
                {
                    first = current;
                }
                else if (faceVertexCount >= 2 &&
                    appendTriangleIndices(indices, indexCount, &indexCapacity, first, previous, current) != 0)
                {
                    fprintf(stderr, "Out of memory while reading OBJ line %zu.\n", lineNumber);
                    result = -1;
                    break;
                }
                previous = current;
                ++faceVertexCount;
                token = nextObjToken(&cursor);
            }

            if (result != 0)
            {
                break;
            }
            if (faceVertexCount < 3)
            {
                fprintf(stderr, "Face has fewer than three vertices at OBJ line %zu.\n", lineNumber);
                result = -1;
                break;
            }
        }
    }

    if (ferror(file))
    {
        fputs("Error while reading OBJ file.\n", stderr);
        result = -1;
    }
    fclose(file);

    if (result == 0)
    {
        if (*vertexCount == 0 || *indexCount == 0)
        {
            fputs("OBJ file contains no renderable vertices or faces.\n", stderr);
            result = -1;
        }
        else
        {
            for (size_t i = 0; i < *indexCount; ++i)
            {
                if ((*indices)[i] >= *vertexCount)
                {
                    fprintf(stderr, "Face references a missing vertex (index %zu).\n", (*indices)[i] + 1);
                    result = -1;
                    break;
                }
            }
        }
    }

    if (result != 0)
    {
        free(*vertices);
        free(*indices);
        *vertices = NULL;
        *indices = NULL;
        *vertexCount = 0;
        *indexCount = 0;
    }
    return result;
}

int main(void)
{
    char path[256];
    printf("Select a valid 3d .OBJ file:\n");

    if (fgets(path, sizeof(path), stdin) == NULL)
    {
        fputs("Input error.\n", stderr);
        return 1;
    }

    if (strchr(path, '\n') == NULL && !feof(stdin))
    {
        fputs("OBJ file path is too long.\n", stderr);
        return 1;
    }
    path[strcspn(path, "\r\n")] = '\0';

    Vec3 *vertices = NULL;
    size_t vertexCount = 0;
    size_t *indices = NULL;
    size_t indexCount = 0;
    if (loadObjFile(path, &vertices, &vertexCount, &indices, &indexCount) != 0)
    {
        return 1;
    }

    int result = doRender(vertices, vertexCount, indices, indexCount);
    free(vertices);
    free(indices);
    return result;
}

static int doRender(const Vec3 *vertices, size_t vertexCount, const size_t *indices, size_t indexCount)
{
    initializeRenderer();
    size_t triangleCount = indexCount / 3;
    if (indexCount % 3 != 0 || triangleCount > SIZE_MAX / sizeof(Triangle))
    {
        fputs("Invalid mesh index data.\n", stderr);
        return 1;
    }

    Triangle *triangles = malloc(triangleCount * sizeof(*triangles));
    if (triangles == NULL)
    {
        fputs("Unable to allocate mesh triangles.\n", stderr);
        return 1;
    }
    for (size_t i = 0; i < triangleCount; ++i)
    {
        for (size_t corner = 0; corner < 3; ++corner)
        {
            size_t vertexIndex = indices[i * 3 + corner];
            if (vertexIndex >= vertexCount)
            {
                fputs("Mesh index is outside the vertex array.\n", stderr);
                free(triangles);
                return 1;
            }
            triangles[i].p[corner] = vertices[vertexIndex];
        }

        Vec3 edge1 = vec3Subtract(triangles[i].p[1], triangles[i].p[0]);
        Vec3 edge2 = vec3Subtract(triangles[i].p[2], triangles[i].p[0]);
        triangles[i].normal = vec3Normalize(vec3Cross(edge1, edge2));
    }

    Camera camera = {
        .position = {0.0f, 1.0f, 5.0f},
        .direction = {0.0f, 0.0f, -1.0f},
        .up = {0.0f, 1.0f, 0.0f}
    };
    pointAt(&camera, camera.position, (Vec3){0.0f, 0.0f, 0.0f}, camera.up);
    PerspectiveMatrix perspectiveMatrix;
    Renderable mesh = {triangles, triangleCount};
    Renderable renderables[] = {mesh};

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
    Scene scene = {
        .renderables = renderables,
        .renderableCount = sizeof(renderables) / sizeof(renderables[0]),
        .lights = lights,
        .lightCount = sizeof(lights) / sizeof(lights[0])
    };

    if (beginFrame() != 0)
    {
        fputs("Failed to enable terminal ANSI processing.\n", stderr);
        free(triangles);
        return 1;
    }

    double frameTime = getMonotonicSeconds();
    int result = 0;
    bool recalculateFrame = true;
    int renderedWidth = 0;
    int renderedHeight = 0;
    while (true)
    {
        iVec2 terminal_size = getTerminalSize();
        if (terminal_size.x <= 0 || terminal_size.y <= 1)
        {
            fputs("Terminal is too small to render a frame and status line.\n", stderr);
            result = 1;
            break;
        }
        --terminal_size.y;
        if (terminal_size.x != renderedWidth || terminal_size.y != renderedHeight)
        {
            recalculateFrame = true;
        }

        if (recalculateFrame)
        {
            if (blankFrame(terminal_size) != 0)
            {
                fputs("Failed to clear terminal frame.\n", stderr);
                result = 1;
                break;
            }
            renderedWidth = terminal_size.x;
            renderedHeight = terminal_size.y;
            float aspectRatio = (float)terminal_size.x / (float)terminal_size.y;
            perspectiveMatrixSetPerspective(
                &perspectiveMatrix,
                &camera.clippingFrutsum,
                3.14159265358979323846f / 6.0f,
                aspectRatio,
                0.1f,
                100.0f);
            perspectiveRender(scene, perspectiveMatrix.base, camera);
        }
        frameTime = finishFrame(recalculateFrame, frameTime);
        char input = getInput();
        if (input != '\0')
        {
            recalculateFrame = true;
            if (input == 'W')
            {
                camera.position = (Vec3){camera.position.x + (camera.direction.x * 0.05f), camera.position.y + (camera.direction.y * 0.05f), camera.position.z + (camera.direction.z * 0.05f)};
            }
            else if (input == 'A')
            {
                Vec3 right = vec3Cross(camera.up, camera.direction);
                camera.position = (Vec3){camera.position.x + (right.x * 0.05f), camera.position.y + (right.y * 0.05f), camera.position.z + (right.z * 0.05f)};
            }
            else if (input == 'S')
            {
                camera.position = (Vec3){camera.position.x - (camera.direction.x * 0.05f), camera.position.y - (camera.direction.y * 0.05f), camera.position.z - (camera.direction.z * 0.05f)};
            }
            else if (input == 'D')
            {
                Vec3 left = vec3Negate(vec3Cross(camera.up, camera.direction));
                camera.position = (Vec3){camera.position.x + (left.x * 0.05f), camera.position.y + (left.y * 0.05f), camera.position.z + (left.z * 0.05f)};
            }
            else if (input == 'K') // pitch
            {
                Vec3 right = vec3Cross(camera.up, camera.direction);
                float angle = 3.14159265358979323846f * 0.05f;
                camera.up = rotateAroundAxis(right, angle, camera.up);
                camera.direction = rotateAroundAxis(right, angle, camera.direction);
            }
            else if (input == 'I')
            {
                Vec3 right = vec3Cross(camera.up, camera.direction);
                float angle = -3.14159265358979323846f * 0.05f;
                camera.up = rotateAroundAxis(right, angle, camera.up);
                camera.direction = rotateAroundAxis(right, angle, camera.direction);
            }
            else if (input == 'J') // yaw
            {
                float angle = 3.14159265358979323846f * 0.05f;
                camera.direction = rotateAroundAxis(camera.up, angle, camera.direction);
            }
            else if (input == 'L')
            {
                float angle = -3.14159265358979323846f * 0.05f;
                camera.direction = rotateAroundAxis(camera.up, angle, camera.direction);
            }
            else if (input == 'O') // roll
            {
                float angle = 3.14159265358979323846f * 0.05f;
                camera.up = rotateAroundAxis(camera.direction, angle, camera.up);
            }
            else if (input == 'U')
            {
                float angle = -3.14159265358979323846f * 0.05f;
                camera.up = rotateAroundAxis(camera.direction, angle, camera.up);
            }
            else if (input == 27) // esc as exit key
                break;
        }
        else recalculateFrame = false;
    }

    free(triangles);
    if (result == 0)
    {
        puts("Done.");
    }
    return result;
}
