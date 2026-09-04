#ifndef DANOVERLAY_LN_COURSE_H
#define DANOVERLAY_LN_COURSE_H

#include <stdbool.h>

#include "engine/chart_features.h"
#include "engine/minacalc_bridge.h"

typedef enum
{
    LN_FAMILY_ALLROUND = 0,
    LN_FAMILY_JACK_TECHNICAL,
    LN_FAMILY_INVERSE,
    LN_FAMILY_SPEED_DENSITY
} LnFamily;

typedef enum
{
    LN_STAGE_1ST = 0,
    LN_STAGE_2ND,
    LN_STAGE_3RD,
    LN_STAGE_4TH,
    LN_STAGE_5TH,
    LN_STAGE_6TH,
    LN_STAGE_7TH,
    LN_STAGE_8TH,
    LN_STAGE_9TH,
    LN_STAGE_10TH,
    LN_STAGE_YOAKE,
    LN_STAGE_YUUGURE,
    LN_STAGE_YORU,
    LN_STAGE_YAMI,
    LN_STAGE_YUME,
    LN_STAGE_YOKAZE,

    LN_STAGE_COUNT
} LnStage;

typedef enum
{
    LN_SUBLEVEL_LOW = 0,
    LN_SUBLEVEL_MID_LOW,
    LN_SUBLEVEL_MID,
    LN_SUBLEVEL_MID_HIGH,
    LN_SUBLEVEL_HIGH
} LnSublevel;

typedef struct
{
    bool valid;
    bool beyond;

    LnStage stage;
    LnFamily family;
    LnSublevel sublevel;

    double dp;
    double confidence;
    double base_sr_dp;
    double corrected_dp;
} LnCourseResult;

/*
 * <= 0.30 LN ratio : rice
 * <= 0.45          : hybrid/gray, still rice
 * >  0.45          : LN Course
 */
bool LnCourseShouldRoute(
    const ChartFeatures *features
);

LnFamily LnCourseClassifyFamily(
    const ChartFeatures *features
);

bool LnCourseEvaluate(
    double sunny_sr,
    const ChartFeatures *features,
    const MinaCalcScores *msd,
    LnCourseResult *out_result
);

const char *LnCourseStageName(
    LnStage stage
);

const char *LnCourseFamilyName(
    LnFamily family
);

const char *LnCourseSublevelName(
    LnSublevel sublevel
);

#endif
