#ifndef DANOVERLAY_TOSU_H
#define DANOVERLAY_TOSU_H

#include <stdbool.h>

#define TOSU_TEXT_MAX 256
#define TOSU_MOD_MAX 32

typedef struct
{
    bool connected;

    char client[32];
    char state[32];

    char artist[TOSU_TEXT_MAX];
    char title[TOSU_TEXT_MAX];
    char difficulty[TOSU_TEXT_MAX];
    char creator[TOSU_TEXT_MAX];

    char checksum[64];

    char mod[TOSU_MOD_MAX];

    double rate;

    double time_ms;
    double length_ms;

    int mode;
} TosuSnapshot;

bool TosuInit(void);
void TosuShutdown(void);

void TosuUpdate(void);

const TosuSnapshot *TosuGetSnapshot(void);

#endif