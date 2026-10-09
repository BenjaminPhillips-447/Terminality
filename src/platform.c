#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdio.h>

#include "platform.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

const char *platformName(void)
{
#ifdef _WIN32
    return "Microsoft Windows";
#else
    return "Linux";
#endif
}
int clearFrame(void)
{
    printf("\033[2J\033[3J\033[H");
    return 0;
}

int sleepMS(unsigned int duration_ms)
{
#ifdef _WIN32
    Sleep((DWORD)duration_ms);
    return 0;
#endif

#ifndef _WIN32
    struct timespec remaining = {
        .tv_sec = (time_t)(duration_ms / 1000),
        .tv_nsec = (long)(duration_ms % 1000) * 1000000L
    };

    while (nanosleep(&remaining, &remaining) != 0)
    {
        if (errno != EINTR)
        {
            return -1;
        }
    }

    return 0;
#endif
}

iVec2 getTerminalSize(void)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns, rows;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    iVec2 size = {columns, rows};
    return size;
#endif

#ifndef _WIN32
    struct winsize max;
    ioctl(0, TIOCGWINSZ, &max);
    iVec2 size = {max.ws_col, max.ws_row};
    return size;
#endif
}
