#include "ln_course.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const double LN_GLOBAL_SR_MEANS[LN_STAGE_COUNT] =
{
    1.3050,
    2.1515,
    2.8504,
    2.8504,
    3.2971,
    3.2971,
    3.8084,
    3.9410,
    4.5798,
    5.0721,
    5.3570,
    5.7562,
    6.4753,
    6.8382,
    7.1861,
    7.5488
};

static const double W_DP_SR = 0.768445;
static const double W_HOLD_OCCUPANCY = 0.579175;
static const double W_SIMULTANEOUS_HOLD = 5.109795;
static const double W_RELEASE_DENSITY = 0.324758;
static const double W_LN_DURATION_CV = 0.224097;
static const double W_DP_SR_X_HOLD = -0.265177;
static const double W_BIAS = -3.189884;

static double clampd(
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

static void build_monotonic_means(
    double output[LN_STAGE_COUNT]
)
{
    memcpy(
        output,
        LN_GLOBAL_SR_MEANS,
        sizeof(LN_GLOBAL_SR_MEANS)
    );

    for (size_t i = 1; i < LN_STAGE_COUNT; ++i)
    {
        if (output[i] <= output[i - 1])
            output[i] = output[i - 1] + 0.001;
    }
}

static double sr_to_dp(
    double sr
)
{
    double means[LN_STAGE_COUNT];
    build_monotonic_means(means);

    double lower[LN_STAGE_COUNT];
    double upper[LN_STAGE_COUNT];

    for (size_t i = 0; i < LN_STAGE_COUNT; ++i)
    {
        if (i > 0)
        {
            lower[i] =
                (means[i - 1] + means[i]) * 0.5;
        }
        else
        {
            lower[i] =
                means[0] -
                (means[1] - means[0]) * 0.5;
        }

        if (i + 1 < LN_STAGE_COUNT)
        {
            upper[i] =
                (means[i] + means[i + 1]) * 0.5;
        }
        else
        {
            upper[i] =
                means[LN_STAGE_COUNT - 1] +
                (means[LN_STAGE_COUNT - 1] -
                 means[LN_STAGE_COUNT - 2]) * 0.5;
        }
    }

    if (sr < lower[0])
        return 1.0;

    if (sr >= upper[LN_STAGE_COUNT - 1])
        return 16.99;

    for (size_t i = 0; i < LN_STAGE_COUNT; ++i)
    {
        if (
            sr >= lower[i] &&
            sr < upper[i]
        )
        {
            const double width =
                fmax(upper[i] - lower[i], 0.000001);

            const double t =
                (sr - lower[i]) / width;

            return
                (double)(i + 1) + t;
        }
    }

    return 1.0;
}

static double regression_dp(
    double dp_sr,
    const ChartFeatures *features
)
{
    const double hold_occupancy =
        features ? features->hold_occupancy : 0.0;

    const double simultaneous_hold =
        features ? features->simultaneous_hold : 0.0;

    const double release_density =
        features ? features->release_density : 0.0;

    const double ln_duration_cv =
        features ? features->ln_duration_cv : 0.0;

    return
        W_DP_SR * dp_sr +
        W_HOLD_OCCUPANCY * hold_occupancy +
        W_SIMULTANEOUS_HOLD * simultaneous_hold +
        W_RELEASE_DENSITY * release_density +
        W_LN_DURATION_CV * ln_duration_cv +
        W_DP_SR_X_HOLD *
            (dp_sr * hold_occupancy) +
        W_BIAS;
}

static double confidence_from_dp(
    double dp
)
{
    const double integer = floor(dp);
    const double fraction = dp - integer;
    const double distance = fabs(fraction - 0.5);

    const double confidence =
        0.5 *
        (1.0 + cos(3.14159265358979323846 * distance / 0.5));

    return clampd(confidence, 0.0, 1.0);
}

static LnStage stage_from_dp(
    double dp
)
{
    int index = (int)floor(dp) - 1;

    if (index < 0)
        index = 0;

    if (index >= LN_STAGE_COUNT)
        index = LN_STAGE_COUNT - 1;

    return (LnStage)index;
}

static LnSublevel sublevel_from_dp(
    double dp
)
{
    double fraction = dp - floor(dp);

    if (fraction <= 0.20)
        return LN_SUBLEVEL_LOW;

    if (fraction <= 0.40)
        return LN_SUBLEVEL_MID_LOW;

    if (fraction <= 0.60)
        return LN_SUBLEVEL_MID;

    if (fraction <= 0.80)
        return LN_SUBLEVEL_MID_HIGH;

    return LN_SUBLEVEL_HIGH;
}

bool LnCourseShouldRoute(
    const ChartFeatures *features
)
{
    return
        features &&
        features->ln_ratio > 0.45;
}

static double normalize_signal(
    double value,
    double low,
    double high
)
{
    if (high <= low)
        return 0.0;

    return clampd(
        (value - low) / (high - low),
        0.0,
        1.0
    );
}

LnFamily LnCourseClassifyFamily(
    const ChartFeatures *features
)
{
    if (!features)
        return LN_FAMILY_ALLROUND;

    const double release =
        normalize_signal(
            features->release_density,
            2.0,
            10.0
        );

    const double density =
        normalize_signal(
            features->nps_p90,
            10.0,
            32.0
        );

    const double stamina =
        normalize_signal(
            features->stamina_index,
            0.35,
            0.92
        );

    const double overlap =
        normalize_signal(
            features->simultaneous_hold,
            0.10,
            0.58
        );

    const double occupancy =
        normalize_signal(
            features->hold_occupancy,
            0.50,
            0.97
        );

    const double hold_chords =
        normalize_signal(
            features->hold_chord_ratio,
            0.10,
            0.55
        );

    const double duration_variation =
        normalize_signal(
            features->ln_duration_cv,
            0.22,
            1.05
        );

    const double timing_irregularity =
        normalize_signal(
            features->timing_irregularity,
            0.25,
            1.45
        );

    const double transition_complexity =
        normalize_signal(
            features->transition_var,
            0.30,
            0.98
        );

    const double pattern_irregularity =
        normalize_signal(
            features->pattern_irregularity,
            0.03,
            0.35
        );

    const double jack_structure =
        normalize_signal(
            features->jack_density,
            0.06,
            0.48
        );

    const double minijack_structure =
        normalize_signal(
            features->minijack_ratio,
            0.03,
            0.28
        );

    double speed_score =
        0.38 * sqrt(release * density) +
        0.24 * density +
        0.20 * stamina +
        0.12 * release +
        0.06 * (1.0 - overlap);

    speed_score *=
        1.0 -
        0.16 * overlap -
        0.10 * hold_chords;

    const double wall_partner =
        fmax(
            hold_chords,
            occupancy * 0.72
        );

    const double wall_core =
        sqrt(
            overlap * wall_partner
        );

    double inverse_score =
        0.56 * wall_core +
        0.22 * overlap +
        0.14 * hold_chords +
        0.08 * occupancy;

    double technical_score =
        0.24 * duration_variation +
        0.20 * timing_irregularity +
        0.18 * transition_complexity +
        0.14 * jack_structure +
        0.10 * minijack_structure +
        0.08 * pattern_irregularity +
        0.06 * (1.0 - overlap);

    if (
        release < 0.30 ||
        density < 0.28
    )
    {
        speed_score *= 0.58;
    }

    const bool inverse_has_overlap =
        features->simultaneous_hold >= 0.22;

    const bool inverse_has_partner =
        features->hold_chord_ratio >= 0.18 ||
        features->hold_occupancy >= 0.84;

    if (
        !inverse_has_overlap ||
        !inverse_has_partner
    )
    {
        inverse_score *= 0.42;
    }

    if (
        features->hold_occupancy >= 0.82 &&
        features->simultaneous_hold < 0.24 &&
        features->hold_chord_ratio < 0.20
    )
    {
        inverse_score *= 0.55;
    }

    if (
        fmax(
            duration_variation,
            fmax(
                timing_irregularity,
                fmax(
                    jack_structure,
                    transition_complexity
                )
            )
        ) < 0.30
    )
    {
        technical_score *= 0.60;
    }

    if (
        duration_variation >= 0.45 &&
        (jack_structure >= 0.35 || transition_complexity >= 0.42)
    )
    {
        technical_score += 0.08;
    }

    LnFamily best_family =
        LN_FAMILY_SPEED_DENSITY;

    double best_score =
        speed_score;

    double second_score =
        fmax(
            inverse_score,
            technical_score
        );

    if (inverse_score > best_score)
    {
        best_family =
            LN_FAMILY_INVERSE;

        second_score =
            fmax(
                speed_score,
                technical_score
            );

        best_score =
            inverse_score;
    }

    if (technical_score > best_score)
    {
        best_family =
            LN_FAMILY_JACK_TECHNICAL;

        second_score =
            fmax(
                speed_score,
                inverse_score
            );

        best_score =
            technical_score;
    }

    if (
        best_score < 0.55 ||
        (best_score - second_score) < 0.075
    )
    {
        return LN_FAMILY_ALLROUND;
    }

    return best_family;
}

bool LnCourseEvaluate(
    double sunny_sr,
    const ChartFeatures *features,
    const MinaCalcScores *msd,
    LnCourseResult *out_result
)
{
    if (!out_result)
        return false;

    memset(
        out_result,
        0,
        sizeof(*out_result)
    );

    (void)msd;

    if (
        sunny_sr <= 0.0 ||
        !features
    )
    {
        return false;
    }

    const double base_dp =
        sr_to_dp(sunny_sr);

    double corrected =
        regression_dp(
            base_dp,
            features
        );

    corrected =
        clampd(
            corrected,
            1.0,
            16.99
        );

    out_result->valid = true;
    out_result->beyond =
        corrected > 16.99;

    out_result->stage =
        stage_from_dp(corrected);

    out_result->family =
        LnCourseClassifyFamily(features);

    out_result->sublevel =
        sublevel_from_dp(corrected);

    out_result->dp = corrected;
    out_result->confidence =
        confidence_from_dp(corrected);

    out_result->base_sr_dp = base_dp;
    out_result->corrected_dp = corrected;

    return true;
}

const char *LnCourseStageName(
    LnStage stage
)
{
    static const char *NAMES[LN_STAGE_COUNT] =
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
        "YOAKE",
        "YUUGURE",
        "YORU",
        "YAMI",
        "YUME",
        "YOKAZE"
    };

    if (
        stage < 0 ||
        stage >= LN_STAGE_COUNT
    )
    {
        return "LN";
    }

    return NAMES[stage];
}

const char *LnCourseFamilyName(
    LnFamily family
)
{
    switch (family)
    {
        case LN_FAMILY_ALLROUND:
            return "All-round LN";

        case LN_FAMILY_JACK_TECHNICAL:
            return "Jack / Technical LN";

        case LN_FAMILY_INVERSE:
            return "Inverse / Wall LN";

        case LN_FAMILY_SPEED_DENSITY:
            return "Speed / Density LN";

        default:
            return "LN";
    }
}

const char *LnCourseSublevelName(
    LnSublevel sublevel
)
{
    switch (sublevel)
    {
        case LN_SUBLEVEL_LOW:
            return "LOW";

        case LN_SUBLEVEL_MID_LOW:
            return "MID-LOW";

        case LN_SUBLEVEL_MID:
            return "MID";

        case LN_SUBLEVEL_MID_HIGH:
            return "MID-HIGH";

        case LN_SUBLEVEL_HIGH:
            return "HIGH";

        default:
            return "MID";
    }
}
