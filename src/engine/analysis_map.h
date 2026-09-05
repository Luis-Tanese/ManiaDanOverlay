#ifndef DANOVERLAY_ANALYSIS_MAP_H
#define DANOVERLAY_ANALYSIS_MAP_H

#include <stdbool.h>
#include <stddef.h>

#include "beatmap/beatmap.h"

typedef struct
{
    ManiaNote *notes;
    size_t note_count;

    double rate;

    double first_note_ms;
    double last_note_ms;
    double duration_ms;
} AnalysisMap;

void AnalysisMapInit(
    AnalysisMap *map
);

void AnalysisMapFree(
    AnalysisMap *map
);

bool AnalysisMapBuild(
    const Beatmap *source,
    double rate,
    AnalysisMap *out_map
);

#endif