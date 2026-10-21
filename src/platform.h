#ifndef BASE_C_TEMPLATE_PLATFORM_H
#define BASE_C_TEMPLATE_PLATFORM_H
#include "units.h"

const char *platformName(void);
int beginFrame(void);
int clearFrame(void);
int endFrame(void);
int sleepMS(unsigned int duration_ms);
double getMonotonicSeconds(void);
char getInput(void);
iVec2 getTerminalSize(void);


#endif
