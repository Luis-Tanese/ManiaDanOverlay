#ifndef DANOVERLAY_SUNNY_SR_H
#define DANOVERLAY_SUNNY_SR_H

#include <stdbool.h>
#include <stddef.h>

#include "engine/analysis_map.h"

typedef struct
{
    double star_rating;
    double percentile_93;
    double percentile_83;
    double weighted_mean;

    double jbar_max;
    double pbar_max;
    double xbar_max;
    double abar_mean;

    double jack_ratio;
    double jbar_share;
    double pbar_share;
    double xbar_share;

    size_t note_count;
    size_t corner_count;
    size_t total_notes_eff;
} SunnySrResult;

bool SunnySrCalculate(
    const AnalysisMap *map,
    SunnySrResult *out_result
);

#endif
