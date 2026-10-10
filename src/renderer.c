#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

static char *frameBuffer;
static size_t frameBufferSize;
static int frameWidth;
static int frameHeight;
static const float fontAspectRatio = 2.5f;

static void drawTrianglePerspective(triangle tri, Mat4x4 projectionMatrix, Mat4x4 viewMatrix);

void perspectiveRender(Renderable **renderables, size_t renderableNumber, Mat4x4 projectionMatrix, Mat4x4 viewMatrix)
{
    for (size_t renderableIndex = 0; renderableIndex < renderableNumber; ++renderableIndex)
    {
        Renderable *renderable = renderables[renderableIndex];

        for (size_t i = 0; i < renderable->triangleCount; ++i)
        {
            drawTrianglePerspective((renderable->triangles)[i], projectionMatrix, viewMatrix);
        }
    }
}

//void orthographicRender(triangle *triangles, int triangle_count, )
//{
//    (void)triangles;
//    (void)triangle_count;
//}

static void drawTrianglePerspective(triangle tri, Mat4x4 projectionMatrix, Mat4x4 viewMatrix)
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
        if (!isfinite(projected[i].x) || !isfinite(projected[i].y))
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
            float sampleX = (float)x + 0.5f;
            float sampleY = (float)y + 0.5f;
            float edge0 = (screenX[1] - screenX[0]) * (sampleY - screenY[0]) - (screenY[1] - screenY[0]) * (sampleX - screenX[0]);
            float edge1 = (screenX[2] - screenX[1]) * (sampleY - screenY[1]) - (screenY[2] - screenY[1]) * (sampleX - screenX[1]);
            float edge2 = (screenX[0] - screenX[2]) * (sampleY - screenY[2]) - (screenY[0] - screenY[2]) * (sampleX - screenX[2]);

            if ((area > 0.0f && edge0 >= 0.0f && edge1 >= 0.0f && edge2 >= 0.0f) || (area < 0.0f && edge0 <= 0.0f && edge1 <= 0.0f && edge2 <= 0.0f))
            {
                frameBuffer[(size_t)y * rowSize + (size_t)x] = '#';
            }
        }
    }
}

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

    frameBuffer = newFrameBuffer;
    frameBufferSize = requiredSize;
    frameWidth = terminalSize.x;
    frameHeight = terminalSize.y;

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
    if (frameBuffer != NULL)
    {
        fwrite(frameBuffer, 1, frameBufferSize, stdout);
    }
}

void freeFrameBuffer(void)
{
    free(frameBuffer);
    frameBuffer = NULL;
    frameBufferSize = 0;
    frameWidth = 0;
    frameHeight = 0;
}

void initializeRenderer(void)
{
    frameBuffer = NULL;
    frameBufferSize = 0;
    frameWidth = 0;
    frameHeight = 0;
}

int main(void)
{
    initializeRenderer();

    Camera camera = {
        .position = {0.0f, 0.0f, 5.0f},
        .direction = {0.0f, 0.0f, -1.0f},
        .up = {0.0f, 1.0f, 0.0f}
    };

    PerspectiveMatrix perspectiveMatrix;
    //OrthographicMatrix orthographicMatrix;

    Vec3 cubeVertices[8] = {
        {-1.0f, -1.0f, -1.0f},
        { 1.0f, -1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f},
        {-1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f},
        { 1.0f, -1.0f,  1.0f},
        { 1.0f,  1.0f,  1.0f},
        {-1.0f,  1.0f,  1.0f}
    };
    const size_t cubeFaceIndices[12][3] = {
        {0, 2, 1}, {0, 3, 2},
        {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4},
        {3, 7, 6}, {3, 6, 2},
        {0, 4, 7}, {0, 7, 3},
        {1, 2, 6}, {1, 6, 5}
    };
    triangle cubeTriangles[12];
    for (size_t i = 0; i < sizeof(cubeTriangles) / sizeof(cubeTriangles[0]); ++i)
    {
        cubeTriangles[i].p[0] = cubeVertices[cubeFaceIndices[i][0]];
        cubeTriangles[i].p[1] = cubeVertices[cubeFaceIndices[i][1]];
        cubeTriangles[i].p[2] = cubeVertices[cubeFaceIndices[i][2]];
    }
    Renderable cube = {cubeTriangles, sizeof(cubeTriangles) / sizeof(cubeTriangles[0])};

    Renderable *renderables[] = {&cube};
    size_t renderablesLength = 1;
    
    
    for (int i = 0; i < 200; ++i)
    {
        clearFrame();
        iVec2 terminal_size = getTerminalSize();
        if (blankFrame(terminal_size) != 0)
        {
            fputs("Failed to clear terminal frame.\n", stderr);
            return 1;
        }
        float aspectRatio = (float)terminal_size.x / (float)terminal_size.y;
        perspectiveMatrixSetPerspective(
            &perspectiveMatrix,
            3.14159265358979323846f / 2.0f,
            aspectRatio,
            0.1f,
            100.0f);

        perspectiveRender(renderables, renderablesLength, perspectiveMatrix.base, getPerspectiveCameraMatrix(&camera));

        finishFrame();
        sleepMS(32);
    }

    puts("Done.");
    return 0;
}
