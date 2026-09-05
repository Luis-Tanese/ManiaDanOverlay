#include "engine/ruler_sanity.h"
#include "calibration/mania4k_calibration.h"

#include <math.h>
#include <string.h>


typedef struct
{
    ReformRuler ruler;
    double support;
} RulerEvidence;


static double max_double(
    double a,
    double b
)
{
    return a > b ? a : b;
}


static double sanitize_score(
    double value
)
{
    if (!isfinite(value) || value < 0.0)
        return 0.0;

    return value;
}


static ReformRuler family_ruler(
    ChartFamily family
)
{
    switch (family)
    {
        case CHART_FAMILY_JACK:
            return REFORM_RULER_JACK;

        case CHART_FAMILY_STREAM:
        case CHART_FAMILY_SPEED:
            return REFORM_RULER_SPEED;

        case CHART_FAMILY_STAMINA:
            return REFORM_RULER_STAMINA;

        case CHART_FAMILY_TECH:
            return REFORM_RULER_TECH;

        case CHART_FAMILY_HYBRID:
        default:
            return REFORM_RULER_GENERAL;
    }
}


static double support_for_ruler(
    const RulerSanityResult *result,
    ReformRuler ruler
)
{
    if (!result)
        return 0.0;

    switch (ruler)
    {
        case REFORM_RULER_JACK:
            return result->jack_support;

        case REFORM_RULER_SPEED:
            return result->speed_support;

        case REFORM_RULER_STAMINA:
            return result->stamina_support;

        case REFORM_RULER_TECH:
            return result->tech_support;

        case REFORM_RULER_GENERAL:
        default:
            return 0.0;
    }
}


static void sort_evidence(
    RulerEvidence *values,
    size_t count
)
{
    if (!values)
        return;

    for (size_t i = 1; i < count; ++i)
    {
        const RulerEvidence value = values[i];
        size_t j = i;

        while (
            j > 0 &&
            values[j - 1].support <
            value.support
        )
        {
            values[j] =
                values[j - 1];

            --j;
        }

        values[j] =
            value;
    }
}


static double promotion_confidence_floor(
    ReformRuler ruler
)
{
    if (ruler < 0 || ruler >= REFORM_RULER_COUNT)
        return 1.0;

    return MANIA4K_CALIBRATION.ruler_sanity.promotion_confidence[ruler];
}


static double promotion_ratio_floor(
    ReformRuler ruler
)
{
    if (ruler < 0 || ruler >= REFORM_RULER_COUNT)
        return INFINITY;

    return MANIA4K_CALIBRATION.ruler_sanity.promotion_ratio[ruler];
}


static double veto_ratio_floor(
    double family_confidence
)
{
    const Mania4KRulerSanityCalibration *cal =
        &MANIA4K_CALIBRATION.ruler_sanity;

    if (family_confidence < cal->veto_confidence_breaks[0])
        return cal->veto_ratio_floors[0];

    if (family_confidence < cal->veto_confidence_breaks[1])
        return cal->veto_ratio_floors[1];

    if (family_confidence < cal->veto_confidence_breaks[2])
        return cal->veto_ratio_floors[2];

    return INFINITY;
}


/* 
 * MinaCalc is a second opinion here. 
 * it may promote GENERAL when both systems agree, or veto a weak ruler back to GENERAL, but it never swaps skill rulers. 
 */

bool RulerSanityEvaluate(
    ReformRuler requested_ruler,
    ChartFamily family,
    double family_confidence,
    const MinaCalcScores *base_msd,
    RulerSanityResult *out_result
)
{
    if (!out_result)
        return false;

    memset(
        out_result,
        0,
        sizeof(*out_result)
    );

    out_result->requested_ruler =
        requested_ruler;

    out_result->final_ruler =
        requested_ruler;

    out_result->family =
        family;

    if (!isfinite(family_confidence))
        family_confidence = 0.0;

    if (family_confidence < 0.0)
        family_confidence = 0.0;

    if (family_confidence > 1.0)
        family_confidence = 1.0;

    out_result->family_confidence =
        family_confidence;

    out_result->action =
        RULER_SANITY_KEEP;

    if (!base_msd)
        return false;

    out_result->jack_support =
        max_double(
            sanitize_score(base_msd->jackspeed),
            sanitize_score(base_msd->chordjack)
        );

    out_result->speed_support =
        max_double(
            sanitize_score(base_msd->stream),
            max_double(
                sanitize_score(base_msd->jumpstream),
                sanitize_score(base_msd->handstream)
            )
        );

    out_result->stamina_support =
        sanitize_score(
            base_msd->stamina
        );

    out_result->tech_support =
        sanitize_score(
            base_msd->technical
        );

    RulerEvidence evidence[4] =
    {
        {
            REFORM_RULER_JACK,
            out_result->jack_support
        },
        {
            REFORM_RULER_SPEED,
            out_result->speed_support
        },
        {
            REFORM_RULER_STAMINA,
            out_result->stamina_support
        },
        {
            REFORM_RULER_TECH,
            out_result->tech_support
        }
    };

    sort_evidence(
        evidence,
        sizeof(evidence) /
        sizeof(evidence[0])
    );

    out_result->msd_top_ruler =
        evidence[0].ruler;

    out_result->top_support =
        evidence[0].support;

    out_result->second_support =
        evidence[1].support;

    if (out_result->second_support > 0.0)
    {
        out_result->top_to_second_ratio =
            out_result->top_support /
            out_result->second_support;
    }
    else if (out_result->top_support > 0.0)
    {
        out_result->top_to_second_ratio =
            INFINITY;
    }

    out_result->requested_support =
        support_for_ruler(
            out_result,
            requested_ruler
        );

    if (out_result->requested_support > 0.0)
    {
        out_result->top_to_requested_ratio =
            out_result->top_support /
            out_result->requested_support;
    }
    else if (out_result->top_support > 0.0)
    {
        out_result->top_to_requested_ratio =
            INFINITY;
    }

    if (out_result->top_support <= 0.0)
        return false;

    const ReformRuler classifier_family_ruler =
        family_ruler(
            family
        );

    if (
        requested_ruler ==
            REFORM_RULER_GENERAL &&
        classifier_family_ruler !=
            REFORM_RULER_GENERAL &&
        out_result->msd_top_ruler ==
            classifier_family_ruler &&
        family_confidence >=
            promotion_confidence_floor(
                classifier_family_ruler
            ) &&
        out_result->top_to_second_ratio >=
            promotion_ratio_floor(
                classifier_family_ruler
            )
    )
    {
        out_result->final_ruler =
            classifier_family_ruler;

        out_result->action =
            RULER_SANITY_PROMOTE;

        out_result->applied =
            true;

        return true;
    }

    if (
        requested_ruler !=
            REFORM_RULER_GENERAL &&
        out_result->msd_top_ruler !=
            requested_ruler &&
        out_result->top_to_requested_ratio >=
            veto_ratio_floor(
                family_confidence
            ) &&
        out_result->top_to_second_ratio >=
            MANIA4K_CALIBRATION.ruler_sanity.veto_top_to_second_floor
    )
    {
        out_result->final_ruler =
            REFORM_RULER_GENERAL;

        out_result->action =
            RULER_SANITY_VETO;

        out_result->applied =
            true;

        return true;
    }

    return true;
}


const char *RulerSanityActionName(
    RulerSanityAction action
)
{
    if (action < 0 || action >= MANIA4K_RULER_SANITY_ACTION_COUNT)
        return "KEEP";

    return MANIA4K_CALIBRATION.ruler_sanity.action_names[action];
}
