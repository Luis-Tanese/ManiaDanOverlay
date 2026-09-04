#ifndef DANOVERLAY_OSU_PARSER_H
#define DANOVERLAY_OSU_PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "beatmap.h"

bool OsuParseBeatmapText(
    const char *text,
    Beatmap *out_map,
    char *error,
    size_t error_size
);

#endif