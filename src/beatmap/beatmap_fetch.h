#ifndef DANOVERLAY_BEATMAP_FETCH_H
#define DANOVERLAY_BEATMAP_FETCH_H

#include <stdbool.h>
#include <stddef.h>

#include "beatmap.h"

bool BeatmapFetchInit(void);

void BeatmapFetchShutdown(void);

bool BeatmapFetchCurrent(
    Beatmap *out_map,
    char *error,
    size_t error_size
);

#endif