#include "reform_rank.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const double GENERAL_MEANS[
    REFORM_TIER_COUNT
] =
{
    2.94,
    3.23,
    3.51,
    4.16,
    4.71,
    5.12,
    5.36,
    5.83,
    6.15,
    6.55,

    6.56,
    6.94,
    7.41,
    7.91,
    9.03,
    9.40,
    10.13,
    10.74,
    11.68,
    12.25
};


static const double JACK_MEANS[
    REFORM_TIER_COUNT
] =
{
    2.34,
    2.35,
    3.17,
    3.48,
    3.98,
    4.75,
    4.97,
    5.84,
    5.85,
    6.50,

    6.66,
    6.90,
    7.28,
    7.91,
    9.13,
    9.35,
    10.38,
    10.96,
    12.27,
    13.13
};


static const double SPEED_MEANS[
    REFORM_TIER_COUNT
] =
{
    2.94,
    3.50,
    3.78,
    4.16,
    4.86,
    5.29,
    5.36,
    5.71,
    5.98,
    6.22,

    6.58,
    6.92,
    7.23,
    7.91,
    9.25,
    9.55,
    10.05,
    10.67,
    11.16,
    12.05
};


static const double STAMINA_MEANS[
    REFORM_TIER_COUNT
] =
{
    3.38,
    3.48,
    3.79,
    4.69,
    5.23,
    5.65,
    5.75,
    6.15,
    6.26,
    6.41,

    6.70,
    7.04,
    7.37,
    8.04,
    9.32,
    9.60,
    9.96,
    10.81,
    11.66,
    12.41
};


static const double TECH_MEANS[
    REFORM_TIER_COUNT
] =
{
    2.84,
    3.08,
    3.09,
    3.90,
    4.18,
    4.50,
    5.43,
    5.69,
    6.31,
    6.46,

    6.63,
    7.00,
    7.31,
    7.99,
    9.22,
    9.60,
    10.25,
    10.64,
    11.69,
    12.10
};


static const char *TIER_NAMES[
    REFORM_TIER_COUNT
] =
{
    "1ST",
    "2ND",
    "3RD",
    "4TH",
    "5TH",
    "6TH",
    "7TH",
    "8TH",
    "9TH",
    "10TH",

    "ALPHA",
    "BETA",
    "GAMMA",
    "DELTA",
    "EPSILON",
    "ZETA",
    "ETA",
    "THETA",
    "IOTA",
    "KAPPA"
};


static const double *get_ruler(
    ReformRuler ruler
)
{
    switch (ruler)
    {
        case REFORM_RULER_JACK:
            return JACK_MEANS;

        case REFORM_RULER_SPEED:
            return SPEED_MEANS;

        case REFORM_RULER_STAMINA:
            return STAMINA_MEANS;

        case REFORM_RULER_TECH:
            return TECH_MEANS;

        case REFORM_RULER_GENERAL:
        default:
            return GENERAL_MEANS;
    }
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


    if (fraction <= 0.20)
        return REFORM_SUBLEVEL_LOW;

    if (fraction <= 0.40)
        return REFORM_SUBLEVEL_MID_LOW;

    if (fraction <= 0.60)
        return REFORM_SUBLEVEL_MID;

    if (fraction <= 0.80)
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
        TIER_NAMES[
            tier_index
        ];
}


const char *ReformRulerName(
    ReformRuler ruler
)
{
    switch (ruler)
    {
        case REFORM_RULER_GENERAL:
            return "GENERAL";

        case REFORM_RULER_JACK:
            return "JACK";

        case REFORM_RULER_SPEED:
            return "SPEED";

        case REFORM_RULER_STAMINA:
            return "STAMINA";

        case REFORM_RULER_TECH:
            return "TECH";

        default:
            return "UNKNOWN";
    }
}


const char *ReformSublevelName(
    ReformSublevel sublevel
)
{
    switch (sublevel)
    {
        case REFORM_SUBLEVEL_LOW:
            return "LOW";

        case REFORM_SUBLEVEL_MID_LOW:
            return "MID-LOW";

        case REFORM_SUBLEVEL_MID:
            return "MID";

        case REFORM_SUBLEVEL_MID_HIGH:
            return "MID-HIGH";

        case REFORM_SUBLEVEL_HIGH:
            return "HIGH";

        default:
            return "UNKNOWN";
    }
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