#include "ln_profile.h"
#include "calibration/mania4k_calibration.h"

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

/* 
 * this profile only chooses player-facing LN descriptors and prominent traits.
 * it has no path back into LN Course stage or DP calculation. 
 */

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

    const Mania4KLnProfileCalibration *cal =
        &MANIA4K_CALIBRATION.ln_profile;

    LnProfileTraitValue values[LN_PROFILE_TRAIT_COUNT] =
    {
        {
            LN_PROFILE_TRAIT_OCCUPANCY,
            clamp01(
                (features->hold_occupancy - cal->trait_low[LN_PROFILE_TRAIT_OCCUPANCY]) /
                cal->trait_span[LN_PROFILE_TRAIT_OCCUPANCY]
            ),
            features->hold_occupancy
        },
        {
            LN_PROFILE_TRAIT_RELEASE_DENSITY,
            clamp01(
                (features->release_density - cal->trait_low[LN_PROFILE_TRAIT_RELEASE_DENSITY]) /
                cal->trait_span[LN_PROFILE_TRAIT_RELEASE_DENSITY]
            ),
            features->release_density
        },
        {
            LN_PROFILE_TRAIT_HOLD_OVERLAP,
            clamp01(
                (features->simultaneous_hold - cal->trait_low[LN_PROFILE_TRAIT_HOLD_OVERLAP]) /
                cal->trait_span[LN_PROFILE_TRAIT_HOLD_OVERLAP]
            ),
            features->simultaneous_hold
        },
        {
            LN_PROFILE_TRAIT_DURATION_VARIATION,
            clamp01(
                (features->ln_duration_cv - cal->trait_low[LN_PROFILE_TRAIT_DURATION_VARIATION]) /
                cal->trait_span[LN_PROFILE_TRAIT_DURATION_VARIATION]
            ),
            features->ln_duration_cv
        },
        {
            LN_PROFILE_TRAIT_HOLD_CHORDS,
            clamp01(
                (features->hold_chord_ratio - cal->trait_low[LN_PROFILE_TRAIT_HOLD_CHORDS]) /
                cal->trait_span[LN_PROFILE_TRAIT_HOLD_CHORDS]
            ),
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
    if (trait < 0 || trait >= LN_PROFILE_TRAIT_COUNT)
        return "LN";

    return MANIA4K_CALIBRATION.ln_profile.trait_names[trait];
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

    if (trait->trait < 0 || trait->trait >= LN_PROFILE_TRAIT_COUNT)
    {
        snprintf(output, output_size, "%s", name);
        return;
    }

    switch (MANIA4K_CALIBRATION.ln_profile.trait_formats[trait->trait])
    {
        case MANIA4K_LN_TRAIT_FORMAT_PERCENT:
            snprintf(
                output,
                output_size,
                "%s %.0f%%",
                name,
                trait->raw_value * 100.0
            );
            break;

        case MANIA4K_LN_TRAIT_FORMAT_RATE:
            snprintf(
                output,
                output_size,
                "%s %.2f/s",
                name,
                trait->raw_value
            );
            break;

        case MANIA4K_LN_TRAIT_FORMAT_DECIMAL:
        default:
            snprintf(
                output,
                output_size,
                "%s %.2f",
                name,
                trait->raw_value
            );
            break;
    }
}
