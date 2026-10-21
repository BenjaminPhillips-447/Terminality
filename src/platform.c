#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdio.h>
#include <ctype.h>

#include "platform.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
static struct termios originalTerminalSettings;
static int terminalModeEnabled;
#endif

const char *platformName(void)
{
#ifdef _WIN32
    return "Microsoft Windows";
#else
    return "Linux";
#endif
}

double getMonotonicSeconds(void)
{
#ifdef _WIN32
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&frequency);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        return 0.0;
    }
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
#endif
}

int beginFrame(void)
{
#ifdef _WIN32
    static int virtualTerminalEnabled;
    if (!virtualTerminalEnabled)
    {
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        if (output != INVALID_HANDLE_VALUE && output != NULL && GetConsoleMode(output, &mode))
        {
            if (!SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                return -1;
            }
        }
        virtualTerminalEnabled = 1;
    }
#else
    if (terminalModeEnabled)
    {
        return 0;
    }
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &originalTerminalSettings) != 0)
    {
        return -1;
    }

    struct termios terminalSettings = originalTerminalSettings;
    terminalSettings.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    terminalSettings.c_cc[VMIN] = 0;
    terminalSettings.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &terminalSettings) != 0)
    {
        return -1;
    }
    terminalModeEnabled = 1;
#endif
    return 0;
}

int endFrame(void)
{
#ifdef _WIN32
    return 0;
#else
    if (!terminalModeEnabled)
    {
        return 0;
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, &originalTerminalSettings) != 0)
    {
        return -1;
    }
    terminalModeEnabled = 0;
    return 0;
#endif
}

char getInput(void)
{
#ifdef _WIN32
    if (_kbhit())
    {
        return (char)toupper(_getch());
    }
#else
    unsigned char input;
    if (read(STDIN_FILENO, &input, sizeof(input)) == 1)
    {
        return (char)toupper(input);
    }
#endif
    return '\0';
}

int clearFrame(void)
{
    if (beginFrame() != 0)
    {
        return -1;
    }
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
