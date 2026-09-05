#ifndef DANOVERLAY_CHART_FEATURES_H
#define DANOVERLAY_CHART_FEATURES_H

#include <stdbool.h>
#include <stddef.h>

#include "engine/analysis_map.h"

typedef struct
{
    double time_ms;
    double nps;
} NpsPoint;

typedef struct
{
    double stream_purity;
    double jump_ratio;
    double hand_ratio;
    double quad_ratio;

    double jack_ratio;
    double jack_density;
    double vibro_density;
    double anchor_ratio;
    double minijack_ratio;

    double density_cv;
    double transition_var;

    double nps_p50;
    double nps_p90;
    double nps_p95;
    double nps_sustained_top30;
    double nps_active_ratio;
    double nps_active_cv;

    double duration_s;
    double stamina_index;
    double burst_ratio;
    double pattern_irregularity;
    double timing_irregularity;
    double chord_complexity;

    double ln_ratio;
    double hold_occupancy;
    double ln_duration_mean_ms;
    double ln_duration_cv;
    double simultaneous_hold;
    double release_density;
    double hold_chord_ratio;

    size_t note_count;
    size_t row_count;
    size_t hold_count;

    NpsPoint *nps_curve;
    size_t nps_curve_count;
} ChartFeatures;

void ChartFeaturesInit(
    ChartFeatures *features
);

void ChartFeaturesFree(
    ChartFeatures *features
);

bool ChartFeaturesBuild(
    const AnalysisMap *map,
    ChartFeatures *out_features
);

#endif
