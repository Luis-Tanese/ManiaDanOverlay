#include "reform_rank.h"
#include "calibration/mania4k_calibration.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const double *get_ruler(
    ReformRuler ruler
)
{
    if (ruler < 0 || ruler >= REFORM_RULER_COUNT)
        ruler = REFORM_RULER_GENERAL;

    return MANIA4K_CALIBRATION.reform.means[ruler];
}


static double clamp_double(
    double value,
    double minimum,
    double maximum
)
{
    if (value < minimum)
        return minimum;

    if (value > maximum)
        return maximum;

    return value;
}


static double lower_boundary(
    const double *means,
    int tier_index
)
{
    if (tier_index <= 0)
    {
        double gap =
            means[1] -
            means[0];

        return
            means[0] -
            gap * 0.5;
    }


    return
        (
            means[tier_index - 1] +
            means[tier_index]
        ) *
        0.5;
}


static double upper_boundary(
    const double *means,
    int tier_index
)
{
    if (
        tier_index >=
        REFORM_TIER_COUNT - 1
    )
    {
        double gap =
            means[
                REFORM_TIER_COUNT - 1
            ] -
            means[
                REFORM_TIER_COUNT - 2
            ];

        return
            means[
                REFORM_TIER_COUNT - 1
            ] +
            gap * 0.5;
    }


    return
        (
            means[tier_index] +
            means[tier_index + 1]
        ) *
        0.5;
}


static ReformSublevel sublevel_from_fraction(
    double fraction
)
{
    fraction =
        clamp_double(
            fraction,
            0.0,
            1.0
        );


    const double *edges =
        MANIA4K_CALIBRATION.reform.sublevel_edges;

    if (fraction <= edges[0])
        return REFORM_SUBLEVEL_LOW;

    if (fraction <= edges[1])
        return REFORM_SUBLEVEL_MID_LOW;

    if (fraction <= edges[2])
        return REFORM_SUBLEVEL_MID;

    if (fraction <= edges[3])
        return REFORM_SUBLEVEL_MID_HIGH;


    return REFORM_SUBLEVEL_HIGH;
}


const char *ReformTierName(
    int tier_index
)
{
    if (
        tier_index < 0 ||
        tier_index >= REFORM_TIER_COUNT
    )
    {
        return "UNKNOWN";
    }


    return
        MANIA4K_CALIBRATION.reform.tier_names[
            tier_index
        ];
}


const char *ReformRulerName(
    ReformRuler ruler
)
{
    if (ruler < 0 || ruler >= REFORM_RULER_COUNT)
        return "UNKNOWN";

    return MANIA4K_CALIBRATION.reform.ruler_names[ruler];
}


const char *ReformSublevelName(
    ReformSublevel sublevel
)
{
    if (sublevel < 0 || sublevel >= MANIA4K_SUBLEVEL_COUNT)
        return "UNKNOWN";

    return MANIA4K_CALIBRATION.reform.sublevel_names[sublevel];
}


double ReformRankMean(
    ReformRuler ruler,
    int tier_index
)
{
    if (
        tier_index < 0 ||
        tier_index >= REFORM_TIER_COUNT
    )
    {
        return 0.0;
    }


    const double *means =
        get_ruler(
            ruler
        );


    return
        means[
            tier_index
        ];
}


bool ReformRankEvaluate(
    double sr,
    ReformRuler ruler,
    ReformRankResult *out_result
)
{
    if (
        !out_result ||
        !isfinite(sr)
    )
    {
        return false;
    }


    if (
        ruler < 0 ||
        ruler >= REFORM_RULER_COUNT
    )
    {
        ruler =
            REFORM_RULER_GENERAL;
    }


    const double *means =
        get_ruler(
            ruler
        );


    memset(
        out_result,
        0,
        sizeof(*out_result)
    );


    out_result->input_sr =
        sr;

    out_result->ruler =
        ruler;


    double absolute_lower =
        lower_boundary(
            means,
            0
        );


    double absolute_upper =
        upper_boundary(
            means,
            REFORM_TIER_COUNT - 1
        );


    out_result->below_range =
        sr <
        absolute_lower;


    out_result->above_range =
        sr >
        absolute_upper;


    int selected_tier =
        REFORM_TIER_COUNT - 1;


    if (out_result->below_range)
    {
        selected_tier =
            0;
    }
    else
    {
        for (
            int i = 0;
            i < REFORM_TIER_COUNT;
            i++
        )
        {
            double upper =
                upper_boundary(
                    means,
                    i
                );


            if (
                sr < upper ||
                i ==
                REFORM_TIER_COUNT - 1
            )
            {
                selected_tier =
                    i;

                break;
            }
        }
    }


    double lower =
        lower_boundary(
            means,
            selected_tier
        );


    double upper =
        upper_boundary(
            means,
            selected_tier
        );


    double width =
        upper -
        lower;


    double fraction =
        0.0;


    if (width > 0.0)
    {
        fraction =
            (
                sr -
                lower
            ) /
            width;
    }

    fraction =
        clamp_double(
            fraction,
            0.0,
            0.999999
        );


    out_result->tier_index =
        selected_tier;


    out_result->mean_sr =
        means[
            selected_tier
        ];


    out_result->lower_boundary =
        lower;


    out_result->upper_boundary =
        upper;


    out_result->zone_fraction =
        fraction;


    out_result->dp =
        (double)(
            selected_tier + 1
        ) +
        fraction;


    out_result->sublevel =
        sublevel_from_fraction(
            fraction
        );


    return true;
}