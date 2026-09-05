#include "ln_course.h"
#include "calibration/mania4k_calibration.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

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
        MANIA4K_CALIBRATION.ln_course.stage_sr_means,
        sizeof(MANIA4K_CALIBRATION.ln_course.stage_sr_means)
    );

    for (size_t i = 1; i < LN_STAGE_COUNT; ++i)
    {
        if (output[i] <= output[i - 1])
        {
            output[i] =
                output[i - 1] +
                MANIA4K_CALIBRATION.ln_course.monotonic_epsilon;
        }
    }
}

static double sr_to_dp(
    double sr
)
{
    const Mania4KLnCourseCalibration *cal =
        &MANIA4K_CALIBRATION.ln_course;

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
        return cal->minimum_dp;

    if (sr >= upper[LN_STAGE_COUNT - 1])
        return cal->maximum_dp;

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

    return cal->minimum_dp;
}

static double regression_dp(
    double dp_sr,
    const ChartFeatures *features
)
{
    const Mania4KLnRegressionCalibration *cal =
        &MANIA4K_CALIBRATION.ln_course.regression;

    const double hold_occupancy =
        features ? features->hold_occupancy : 0.0;

    const double simultaneous_hold =
        features ? features->simultaneous_hold : 0.0;

    const double release_density =
        features ? features->release_density : 0.0;

    const double ln_duration_cv =
        features ? features->ln_duration_cv : 0.0;

    return
        cal->dp_sr * dp_sr +
        cal->hold_occupancy * hold_occupancy +
        cal->simultaneous_hold * simultaneous_hold +
        cal->release_density * release_density +
        cal->ln_duration_cv * ln_duration_cv +
        cal->dp_sr_x_hold *
            (dp_sr * hold_occupancy) +
        cal->bias;
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
    const double fraction = dp - floor(dp);
    const double *edges =
        MANIA4K_CALIBRATION.ln_course.sublevel_edges;

    if (fraction <= edges[0])
        return LN_SUBLEVEL_LOW;

    if (fraction <= edges[1])
        return LN_SUBLEVEL_MID_LOW;

    if (fraction <= edges[2])
        return LN_SUBLEVEL_MID;

    if (fraction <= edges[3])
        return LN_SUBLEVEL_MID_HIGH;

    return LN_SUBLEVEL_HIGH;
}

bool LnCourseShouldRoute(
    const ChartFeatures *features
)
{
    return
        features &&
        features->ln_ratio > MANIA4K_CALIBRATION.ln_course.route_ln_ratio;
}

static double normalize_signal(
    double value,
    Mania4KSignalRange range
)
{
    if (range.high <= range.low)
        return 0.0;

    return clampd(
        (value - range.low) / (range.high - range.low),
        0.0,
        1.0
    );
}

/* 
 * LN family is descriptive. 
 * it is kept separate from the stage regression so changing labels cannot silently change LN Course DP. 
 */

LnFamily LnCourseClassifyFamily(
    const ChartFeatures *features
)
{
    if (!features)
        return LN_FAMILY_ALLROUND;

    const Mania4KLnCourseCalibration *cal =
        &MANIA4K_CALIBRATION.ln_course;

    const Mania4KLnFamilyCalibration *family =
        &cal->family;

    const double release =
        normalize_signal(
            features->release_density,
            cal->release_density
        );

    const double density =
        normalize_signal(
            features->nps_p90,
            cal->density
        );

    const double stamina =
        normalize_signal(
            features->stamina_index,
            cal->stamina
        );

    const double overlap =
        normalize_signal(
            features->simultaneous_hold,
            cal->overlap
        );

    const double occupancy =
        normalize_signal(
            features->hold_occupancy,
            cal->occupancy
        );

    const double hold_chords =
        normalize_signal(
            features->hold_chord_ratio,
            cal->hold_chords
        );

    const double duration_variation =
        normalize_signal(
            features->ln_duration_cv,
            cal->duration_variation
        );

    const double timing_irregularity =
        normalize_signal(
            features->timing_irregularity,
            cal->timing_irregularity
        );

    const double transition_complexity =
        normalize_signal(
            features->transition_var,
            cal->transition_complexity
        );

    const double pattern_irregularity =
        normalize_signal(
            features->pattern_irregularity,
            cal->pattern_irregularity
        );

    const double jack_structure =
        normalize_signal(
            features->jack_density,
            cal->jack_structure
        );

    const double minijack_structure =
        normalize_signal(
            features->minijack_ratio,
            cal->minijack_structure
        );

    double speed_score =
        family->speed_release_density_interaction * sqrt(release * density) +
        family->speed_density * density +
        family->speed_stamina * stamina +
        family->speed_release * release +
        family->speed_no_overlap * (1.0 - overlap);

    speed_score *=
        1.0 -
        family->speed_overlap_penalty * overlap -
        family->speed_hold_chord_penalty * hold_chords;

    const double wall_partner =
        fmax(
            hold_chords,
            occupancy * family->wall_occupancy_partner
        );

    const double wall_core =
        sqrt(overlap * wall_partner);

    double inverse_score =
        family->inverse_wall_core * wall_core +
        family->inverse_overlap * overlap +
        family->inverse_hold_chords * hold_chords +
        family->inverse_occupancy * occupancy;

    double technical_score =
        family->technical_duration_variation * duration_variation +
        family->technical_timing_irregularity * timing_irregularity +
        family->technical_transition_complexity * transition_complexity +
        family->technical_jack_structure * jack_structure +
        family->technical_minijack_structure * minijack_structure +
        family->technical_pattern_irregularity * pattern_irregularity +
        family->technical_no_overlap * (1.0 - overlap);

    if (
        release < family->speed_release_floor ||
        density < family->speed_density_floor
    )
    {
        speed_score *= family->speed_weak_multiplier;
    }

    const bool inverse_has_overlap =
        features->simultaneous_hold >= family->inverse_overlap_floor;

    const bool inverse_has_partner =
        features->hold_chord_ratio >= family->inverse_hold_chord_floor ||
        features->hold_occupancy >= family->inverse_occupancy_partner_floor;

    if (
        !inverse_has_overlap ||
        !inverse_has_partner
    )
    {
        inverse_score *= family->inverse_missing_structure_multiplier;
    }

    if (
        features->hold_occupancy >= family->occupancy_only_floor &&
        features->simultaneous_hold < family->occupancy_only_overlap_ceiling &&
        features->hold_chord_ratio < family->occupancy_only_chord_ceiling
    )
    {
        inverse_score *= family->occupancy_only_multiplier;
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
        ) < family->technical_signal_floor
    )
    {
        technical_score *= family->technical_weak_multiplier;
    }

    if (
        duration_variation >= family->technical_duration_bonus_floor &&
        (
            jack_structure >= family->technical_jack_bonus_floor ||
            transition_complexity >= family->technical_transition_bonus_floor
        )
    )
    {
        technical_score += family->technical_bonus;
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
        best_score < family->family_score_floor ||
        (best_score - second_score) < family->family_margin_floor
    )
    {
        return LN_FAMILY_ALLROUND;
    }

    return best_family;
}

/* 
 * the rating path is Sunny-derived DP plus the LN regression. 
 * MinaCalc remains display evidence and does not feed the course score. 
 */

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

    const Mania4KLnCourseCalibration *cal =
        &MANIA4K_CALIBRATION.ln_course;

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
            cal->minimum_dp,
            cal->maximum_dp
        );

    out_result->valid = true;
    out_result->beyond =
        corrected > cal->maximum_dp;

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
    if (
        stage < 0 ||
        stage >= LN_STAGE_COUNT
    )
    {
        return "LN";
    }

    return MANIA4K_CALIBRATION.ln_course.stage_names[stage];
}

const char *LnCourseFamilyName(
    LnFamily family
)
{
    if (family < 0 || family >= MANIA4K_LN_FAMILY_COUNT)
        return "LN";

    return MANIA4K_CALIBRATION.ln_course.family_names[family];
}

const char *LnCourseSublevelName(
    LnSublevel sublevel
)
{
    if (sublevel < 0 || sublevel >= MANIA4K_SUBLEVEL_COUNT)
        return "MID";

    return MANIA4K_CALIBRATION.ln_course.sublevel_names[sublevel];
}
