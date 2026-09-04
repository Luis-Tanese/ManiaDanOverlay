#ifndef DANOVERLAY_REFORM_RANK_H
#define DANOVERLAY_REFORM_RANK_H

#include <stdbool.h>


#define REFORM_TIER_COUNT 20


typedef enum
{
    REFORM_RULER_GENERAL = 0,
    REFORM_RULER_JACK,
    REFORM_RULER_SPEED,
    REFORM_RULER_STAMINA,
    REFORM_RULER_TECH,

    REFORM_RULER_COUNT
} ReformRuler;


typedef enum
{
    REFORM_SUBLEVEL_LOW = 0,
    REFORM_SUBLEVEL_MID_LOW,
    REFORM_SUBLEVEL_MID,
    REFORM_SUBLEVEL_MID_HIGH,
    REFORM_SUBLEVEL_HIGH
} ReformSublevel;


typedef struct
{
    double input_sr;

    double dp;

    double zone_fraction;

    double mean_sr;
    double lower_boundary;
    double upper_boundary;

    int tier_index;

    ReformRuler ruler;
    ReformSublevel sublevel;

    bool below_range;
    bool above_range;
} ReformRankResult;


const char *ReformTierName(
    int tier_index
);


const char *ReformRulerName(
    ReformRuler ruler
);


const char *ReformSublevelName(
    ReformSublevel sublevel
);


double ReformRankMean(
    ReformRuler ruler,
    int tier_index
);


bool ReformRankEvaluate(
    double sr,
    ReformRuler ruler,
    ReformRankResult *out_result
);


#endif