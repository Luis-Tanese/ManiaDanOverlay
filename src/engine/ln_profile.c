#include "ln_profile.h"

#include <stdio.h>
#include <string.h>

static double clamp01(
    double value
)
{
    if (value < 0.0)
        return 0.0;

    if (value > 1.0)
        return 1.0;

    return value;
}

static void sort_traits(
    LnProfileTraitValue values[LN_PROFILE_TRAIT_COUNT]
)
{
    for (int i = 0; i < LN_PROFILE_TRAIT_COUNT - 1; ++i)
    {
        for (int j = i + 1; j < LN_PROFILE_TRAIT_COUNT; ++j)
        {
            if (values[j].prominence > values[i].prominence)
            {
                const LnProfileTraitValue temporary = values[i];
                values[i] = values[j];
                values[j] = temporary;
            }
        }
    }
}

bool LnPlayerProfileBuild(
    const ChartFeatures *features,
    LnPlayerProfile *out_profile
)
{
    if (!out_profile)
        return false;

    memset(
        out_profile,
        0,
        sizeof(*out_profile)
    );

    if (!features || features->hold_count == 0)
        return false;

    LnProfileTraitValue values[LN_PROFILE_TRAIT_COUNT] =
    {
        {
            LN_PROFILE_TRAIT_OCCUPANCY,
            clamp01((features->hold_occupancy - 0.45) / 0.50),
            features->hold_occupancy
        },
        {
            LN_PROFILE_TRAIT_RELEASE_DENSITY,
            clamp01((features->release_density - 1.5) / 7.0),
            features->release_density
        },
        {
            LN_PROFILE_TRAIT_HOLD_OVERLAP,
            clamp01((features->simultaneous_hold - 0.08) / 0.45),
            features->simultaneous_hold
        },
        {
            LN_PROFILE_TRAIT_DURATION_VARIATION,
            clamp01((features->ln_duration_cv - 0.20) / 0.80),
            features->ln_duration_cv
        },
        {
            LN_PROFILE_TRAIT_HOLD_CHORDS,
            clamp01((features->hold_chord_ratio - 0.08) / 0.50),
            features->hold_chord_ratio
        }
    };

    sort_traits(values);

    out_profile->valid = true;
    out_profile->family =
        LnCourseClassifyFamily(features);

    for (int i = 0; i < 3; ++i)
        out_profile->traits[i] = values[i];

    return true;
}

const char *LnPlayerProfileTraitName(
    LnProfileTrait trait
)
{
    switch (trait)
    {
        case LN_PROFILE_TRAIT_OCCUPANCY:
            return "Occupancy";

        case LN_PROFILE_TRAIT_RELEASE_DENSITY:
            return "Release";

        case LN_PROFILE_TRAIT_HOLD_OVERLAP:
            return "Overlap";

        case LN_PROFILE_TRAIT_DURATION_VARIATION:
            return "Duration CV";

        case LN_PROFILE_TRAIT_HOLD_CHORDS:
            return "Hold chords";

        default:
            return "LN";
    }
}

void LnPlayerProfileFormatTrait(
    const LnProfileTraitValue *trait,
    char *output,
    size_t output_size
)
{
    if (!output || output_size == 0)
        return;

    output[0] = '\0';

    if (!trait)
        return;

    const char *name =
        LnPlayerProfileTraitName(trait->trait);

    switch (trait->trait)
    {
        case LN_PROFILE_TRAIT_OCCUPANCY:
        case LN_PROFILE_TRAIT_HOLD_OVERLAP:
        case LN_PROFILE_TRAIT_HOLD_CHORDS:
            snprintf(
                output,
                output_size,
                "%s %.0f%%",
                name,
                trait->raw_value * 100.0
            );
            break;

        case LN_PROFILE_TRAIT_RELEASE_DENSITY:
            snprintf(
                output,
                output_size,
                "%s %.2f/s",
                name,
                trait->raw_value
            );
            break;

        case LN_PROFILE_TRAIT_DURATION_VARIATION:
            snprintf(
                output,
                output_size,
                "%s %.2f",
                name,
                trait->raw_value
            );
            break;

        default:
            snprintf(
                output,
                output_size,
                "%s",
                name
            );
            break;
    }
}
