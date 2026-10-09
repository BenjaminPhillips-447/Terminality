#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

static char *frameBuffer;
static size_t frameBufferSize;

triangle *deconstructComplexObjects(void)
{
    return NULL;
}

void renderObjects(triangle *triangles, int triangle_count)
{
    (void)triangles;
    (void)triangle_count;
}

void drawTriangle(triangle tri)
{
    (void)tri;
}

int blankFrame(iVec2 terminalSize)
{
    if (terminalSize.x <= 0 || terminalSize.y <= 0 || (size_t)terminalSize.y > SIZE_MAX / (size_t)terminalSize.x)
    {
        return -1;
    }
    size_t requiredSize = (size_t)terminalSize.x * (size_t)(terminalSize.y - 1);
    frameBuffer = realloc(frameBuffer, requiredSize + 1);
    
    char *debugMessage = malloc(terminalSize.x);
    int debugLength = printf("Blanking frame of size %dx%d operating on %s\n", terminalSize.x, terminalSize.y, platformName());
    
    free(debugMessage);

    if (debugLength < 0)
    {
        return -1;
    }
    return 0;
}

void finishFrame(void)
{
    printf(frameBuffer);
}

void freeFrameBuffer(void)
{
    free(frameBuffer);
    frameBuffer = NULL;
    frameBufferSize = 0;
}

void initializeRenderer(void)
{
    //allocate dynamic sized memory for the framebuffer so we can use realloc
    frameBuffer = malloc(20);
    frameBufferSize = 20;
}

int main(void)
{
    initializeRenderer();

    Vec3 cameraPosition = {0.0f, 0.0f, 0.0f};
    Vec3 cameraDirection = {1.0f, 0.0f, 0.0f};


    for (int i = 0; i < 200; ++i)
    {
        clearFrame();
        iVec2 terminal_size = getTerminalSize();









        if (blankFrame(terminal_size) != 0)
        {
            fputs("Failed to clear terminal frame.\n", stderr);
            return 1;
        }
        finishFrame();
        sleepMS(32);
    }

    puts("Done.");
    return 0;
}
