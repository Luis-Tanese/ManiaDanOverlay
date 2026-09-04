#ifndef DANOVERLAY_LN_PROFILE_H
#define DANOVERLAY_LN_PROFILE_H

#include <stdbool.h>
#include <stddef.h>

#include "engine/chart_features.h"
#include "engine/ln_course.h"

typedef enum
{
    LN_PROFILE_TRAIT_OCCUPANCY = 0,
    LN_PROFILE_TRAIT_RELEASE_DENSITY,
    LN_PROFILE_TRAIT_HOLD_OVERLAP,
    LN_PROFILE_TRAIT_DURATION_VARIATION,
    LN_PROFILE_TRAIT_HOLD_CHORDS,

    LN_PROFILE_TRAIT_COUNT
} LnProfileTrait;

typedef struct
{
    LnProfileTrait trait;
    double prominence;
    double raw_value;
} LnProfileTraitValue;

typedef struct
{
    bool valid;
    LnFamily family;
    LnProfileTraitValue traits[3];
} LnPlayerProfile;

bool LnPlayerProfileBuild(
    const ChartFeatures *features,
    LnPlayerProfile *out_profile
);

const char *LnPlayerProfileTraitName(
    LnProfileTrait trait
);

void LnPlayerProfileFormatTrait(
    const LnProfileTraitValue *trait,
    char *output,
    size_t output_size
);

#endif
