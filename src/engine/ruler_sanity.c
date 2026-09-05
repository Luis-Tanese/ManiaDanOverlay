#include "engine/ruler_sanity.h"

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
    switch (ruler)
    {
        case REFORM_RULER_JACK:
            return 0.28;

        case REFORM_RULER_SPEED:
            return 0.32;

        case REFORM_RULER_STAMINA:
            return 0.34;

        case REFORM_RULER_TECH:
            return 0.35;

        case REFORM_RULER_GENERAL:
        default:
            return 1.0;
    }
}


static double promotion_ratio_floor(
    ReformRuler ruler
)
{
    switch (ruler)
    {
        case REFORM_RULER_JACK:
            return 1.08;

        case REFORM_RULER_SPEED:
            return 1.07;

        case REFORM_RULER_STAMINA:
            return 1.08;

        case REFORM_RULER_TECH:
            return 1.10;

        case REFORM_RULER_GENERAL:
        default:
            return INFINITY;
    }
}


static double veto_ratio_floor(
    double family_confidence
)
{
    if (family_confidence < 0.60)
        return 1.18;

    if (family_confidence < 0.70)
        return 1.24;

    if (family_confidence < 0.80)
        return 1.32;

    return INFINITY;
}


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
            1.06
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
    switch (action)
    {
        case RULER_SANITY_PROMOTE:
            return "PROMOTE";

        case RULER_SANITY_VETO:
            return "VETO";

        case RULER_SANITY_KEEP:
        default:
            return "KEEP";
    }
}
