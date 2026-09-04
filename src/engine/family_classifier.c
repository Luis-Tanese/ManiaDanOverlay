#include "family_classifier.h"

#include <math.h>
#include <stddef.h>
#include <string.h>


typedef struct
{
    ChartFamily family;
    double score;
} FamilyScore;


typedef struct
{
    TechSubtype subtype;
    double score;
} TechScore;


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


static double max_double(
    double a,
    double b
)
{
    return a > b ? a : b;
}


static double min_double(
    double a,
    double b
)
{
    return a < b ? a : b;
}


static void sort_family_scores(
    FamilyScore *scores,
    size_t count
)
{
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = i + 1; j < count; ++j)
        {
            if (scores[j].score > scores[i].score)
            {
                const FamilyScore temp = scores[i];
                scores[i] = scores[j];
                scores[j] = temp;
            }
        }
    }
}


static void sort_tech_scores(
    TechScore *scores,
    size_t count
)
{
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = i + 1; j < count; ++j)
        {
            if (scores[j].score > scores[i].score)
            {
                const TechScore temp = scores[i];
                scores[i] = scores[j];
                scores[j] = temp;
            }
        }
    }
}


static double score_for_family(
    const FamilyClassification *result,
    ChartFamily family
)
{
    if (!result)
        return 0.0;

    switch (family)
    {
        case CHART_FAMILY_STREAM:
            return result->stream_score;

        case CHART_FAMILY_JACK:
            return result->jack_score;

        case CHART_FAMILY_TECH:
            return result->tech_score;

        case CHART_FAMILY_SPEED:
            return result->speed_score;

        case CHART_FAMILY_STAMINA:
            return result->stamina_score;

        case CHART_FAMILY_HYBRID:
        default:
            return 0.0;
    }
}


static void classify_tech_subtype(
    const SunnySrResult *sunny,
    const ChartFeatures *features,
    TechSubtype *out_subtype,
    double *out_confidence
)
{
    if (!sunny || !features || !out_subtype || !out_confidence)
        return;

    const double transition_var = features->transition_var;
    const double density_cv = features->density_cv;
    const double jump_ratio = features->jump_ratio;
    const double hand_ratio = features->hand_ratio;
    const double anchor_ratio = features->anchor_ratio;
    const double chord_complexity = features->chord_complexity;
    const double nps_active_cv = features->nps_active_cv;
    const double stream_purity = features->stream_purity;

    const double jbar_max = sunny->jbar_max;
    const double pbar_max = sunny->pbar_max;
    const double xbar_max = sunny->xbar_max;

    const double bar_total =
        jbar_max +
        pbar_max +
        1.0;

    const double tech_dom =
        (xbar_max + bar_total) > 0.0
            ? xbar_max / (xbar_max + bar_total)
            : 0.0;

    const double chaos_score =
        transition_var * 42.0 +
        density_cv * 26.0 +
        nps_active_cv * 20.0 +
        tech_dom * 18.0 -
        anchor_ratio * 10.0;

    const double control_score =
        hand_ratio * 45.0 +
        jump_ratio * 10.0 +
        chord_complexity * 12.0 +
        tech_dom * 15.0 +
        max_double(0.0, 1.0 - density_cv) * 8.0 +
        anchor_ratio * 6.0;

    const double hybrid_score =
        tech_dom * 22.0 +
        jump_ratio * 14.0 +
        density_cv * 10.0 +
        transition_var * 10.0 +
        max_double(
            0.0,
            0.9 - fabs(stream_purity - 0.45)
        ) * 5.0;

    TechScore ranked[3] =
    {
        {TECH_SUBTYPE_CHAOS, chaos_score},
        {TECH_SUBTYPE_CONTROL, control_score},
        {TECH_SUBTYPE_HYBRID, hybrid_score}
    };

    sort_tech_scores(ranked, 3);

    const double top_score = ranked[0].score;
    const double second_score = ranked[1].score;
    const double total = max_double(
        ranked[0].score +
        ranked[1].score +
        ranked[2].score,
        1e-9
    );

    double confidence = clamp_double(
        (top_score - second_score) /
        max_double(total * 0.18, 1.0),
        0.15,
        1.0
    );

    TechSubtype subtype = ranked[0].subtype;

    const double chaos_transition_floor = 0.38;
    const double control_hand_floor = 0.10;
    const double control_chord_floor = 0.18;

    if (
        subtype == TECH_SUBTYPE_CHAOS &&
        transition_var < chaos_transition_floor
    )
    {
        subtype = TECH_SUBTYPE_HYBRID;
        confidence = min_double(confidence, 0.45);
    }
    else if (
        subtype == TECH_SUBTYPE_CONTROL &&
        hand_ratio < control_hand_floor &&
        chord_complexity < control_chord_floor
    )
    {
        subtype = TECH_SUBTYPE_HYBRID;
        confidence = min_double(confidence, 0.45);
    }

    *out_subtype = subtype;
    *out_confidence = confidence;
}


const char *ChartFamilyName(
    ChartFamily family
)
{
    switch (family)
    {
        case CHART_FAMILY_STREAM:
            return "STREAM";

        case CHART_FAMILY_JACK:
            return "JACK";

        case CHART_FAMILY_TECH:
            return "TECH";

        case CHART_FAMILY_SPEED:
            return "SPEED";

        case CHART_FAMILY_STAMINA:
            return "STAMINA";

        case CHART_FAMILY_HYBRID:
            return "HYBRID";

        default:
            return "UNKNOWN";
    }
}


const char *TechSubtypeName(
    TechSubtype subtype
)
{
    switch (subtype)
    {
        case TECH_SUBTYPE_CHAOS:
            return "CHAOS TECH";

        case TECH_SUBTYPE_CONTROL:
            return "CONTROL TECH";

        case TECH_SUBTYPE_HYBRID:
            return "HYBRID TECH";

        case TECH_SUBTYPE_GENERIC:
        default:
            return "GENERIC";
    }
}


bool FamilyClassifierEvaluate(
    const SunnySrResult *sunny,
    const ChartFeatures *features,
    double bpm,
    FamilyClassification *out_result
)
{
    if (!sunny || !features || !out_result)
        return false;

    memset(out_result, 0, sizeof(*out_result));

    out_result->family = CHART_FAMILY_HYBRID;
    out_result->tech_subtype = TECH_SUBTYPE_GENERIC;

    const double jbar_max = sunny->jbar_max;
    const double pbar_max = sunny->pbar_max;
    const double xbar_max = sunny->xbar_max;

    const double bar_total =
        jbar_max +
        pbar_max +
        1.0;

    const double jack_dom =
        jbar_max / bar_total;

    const double stream_dom =
        pbar_max / bar_total;

    const double tech_dom =
        (xbar_max + bar_total) > 0.0
            ? xbar_max / (xbar_max + bar_total)
            : 0.0;

    const double peak_ratio = 1.0;

    const double stream_purity = features->stream_purity;
    const double jack_ratio_broad = features->jack_ratio;
    const double jack_density = features->jack_density;
    const double vibro_density = features->vibro_density;
    const double density_cv = min_double(2.0, features->density_cv);
    const double transition_var = features->transition_var;
    const double jump_ratio = features->jump_ratio;
    const double hand_ratio = features->hand_ratio;
    const double quad_ratio = features->quad_ratio;

    const double nps_p90 = features->nps_p90;
    const double nps_sustained = features->nps_sustained_top30;
    const double nps_active_ratio = features->nps_active_ratio;
    const double nps_active_cv = features->nps_active_cv;

    (void)nps_p90;

    const double chord_fraction =
        jump_ratio +
        hand_ratio +
        quad_ratio;

    const double anchor_ratio = features->anchor_ratio;
    const double pattern_irregularity = features->pattern_irregularity;
    const double timing_irregularity = features->timing_irregularity;
    const double drain_s = features->duration_s;

    const double rep_margin = max_double(
        0.0,
        0.83 - transition_var
    );

    const double is_repetitive = min_double(
        1.0,
        rep_margin * 12.0
    );

    double jack_baseline = 0.3;

    if (bpm > 0.0)
    {
        const double quarter_ms = 60000.0 / bpm;

        jack_baseline = min_double(
            0.9,
            max_double(
                0.0,
                1.0 - quarter_ms / 250.0
            )
        );
    }

    const double jack_excess = max_double(
        0.0,
        jack_ratio_broad - jack_baseline
    );

    const double bpm_signal = min_double(
        1.5,
        max_double(0.0, bpm - 140.0) / 80.0
    );

    const double jack_score =
        is_repetitive * 50.0 +
        jack_density * 45.0 +
        chord_fraction * 20.0 +
        jack_excess * 15.0 +
        vibro_density * 20.0 +
        anchor_ratio * 15.0 +
        jack_dom * 10.0;

    const double stream_score =
        stream_purity * 45.0 +
        max_double(0.0, 1.0 - chord_fraction * 3.0) * 15.0 -
        chord_fraction * 12.0 +
        stream_dom * 20.0 +
        max_double(0.0, 1.0 - density_cv) * 15.0 +
        max_double(0.0, 0.5 - jack_density) * 10.0;

    const double tech_score =
        density_cv * 55.0 +
        pattern_irregularity * 25.0 +
        nps_active_cv * 20.0 +
        tech_dom * 20.0 +
        transition_var * 12.0 +
        chord_fraction * 8.0;

    const double speed_raw =
        bpm_signal * 42.0 +
        stream_purity * 22.0 +
        max_double(0.0, 1.0 - density_cv) * 10.0 +
        max_double(0.0, peak_ratio - 1.02) * 12.0;

    const double speed_chord_gate = max_double(
        0.25,
        1.0 - chord_fraction * 1.8
    );

    const double speed_regularity = max_double(
        0.5,
        1.0 - max_double(0.0, density_cv - 0.30) * 2.5
    );

    const double speed_score =
        speed_raw *
        speed_chord_gate *
        speed_regularity;

    const double drain_gate =
        max_double(0.0, drain_s - 60.0) /
        120.0;

    const double short_penalty = max_double(
        0.0,
        1.0 - drain_s / 90.0
    );

    const double chord_req = min_double(
        1.0,
        max_double(0.0, chord_fraction - 0.15) /
        0.20
    );

    const double stamina_raw =
        drain_gate * 28.0 +
        chord_fraction * 25.0 -
        stream_purity * 20.0 +
        nps_active_ratio * 12.0 +
        max_double(0.0, 1.0 - density_cv) * 8.0 +
        nps_sustained * 0.3 +
        min_double(1.0, drain_s / 150.0) * 12.0;

    double stamina_score =
        stamina_raw *
        chord_req *
        max_double(
            0.4,
            1.0 - short_penalty * 0.5
        );

    if (is_repetitive > 0.15)
    {
        stamina_score *= max_double(
            0.3,
            1.0 - is_repetitive * 0.7
        );
    }

    out_result->stream_score = stream_score;
    out_result->jack_score = jack_score;
    out_result->tech_score = tech_score;
    out_result->speed_score = speed_score;
    out_result->stamina_score = stamina_score;

    FamilyScore ranked[5] =
    {
        {CHART_FAMILY_STREAM, stream_score},
        {CHART_FAMILY_JACK, jack_score},
        {CHART_FAMILY_TECH, tech_score},
        {CHART_FAMILY_SPEED, speed_score},
        {CHART_FAMILY_STAMINA, stamina_score}
    };

    sort_family_scores(ranked, 5);

    const ChartFamily top_family = ranked[0].family;
    const double top_score = ranked[0].score;
    const double second_score = ranked[1].score;
    const double gap = top_score - second_score;

    const double total =
        ranked[0].score +
        ranked[1].score +
        ranked[2].score +
        ranked[3].score +
        ranked[4].score;

    double confidence = min_double(
        1.0,
        gap / max_double(total * 0.10, 1.0)
    );

    ChartFamily family;

    if (confidence < 0.15)
    {
        family = CHART_FAMILY_HYBRID;
        confidence = max_double(0.1, confidence);
    }
    else
    {
        family = top_family;
    }

    if (
        (family == CHART_FAMILY_HYBRID ||
         family == CHART_FAMILY_STREAM) &&
        density_cv > 0.35
    )
    {
        if (
            tech_score > 0.0 &&
            stream_score > 0.0 &&
            tech_score / stream_score > 0.75
        )
        {
            family = CHART_FAMILY_TECH;
            confidence = max_double(confidence, 0.3);
        }
    }

    if (
        family == CHART_FAMILY_STREAM &&
        bpm >= 155.0 &&
        stream_purity > 0.70 &&
        chord_fraction < 0.30
    )
    {
        if (
            speed_score > 0.0 &&
            stream_score > 0.0 &&
            speed_score / stream_score > 0.35
        )
        {
            family = CHART_FAMILY_SPEED;
            confidence = max_double(confidence, 0.25);
        }
    }

    if (
        family == CHART_FAMILY_JACK &&
        sunny->total_notes_eff > 15000
    )
    {
        family =
            stamina_score > tech_score
                ? CHART_FAMILY_STAMINA
                : CHART_FAMILY_HYBRID;

        confidence = min_double(confidence, 0.5);
    }

    if (
        (family == CHART_FAMILY_STREAM ||
         family == CHART_FAMILY_TECH) &&
        drain_s > 120.0 &&
        chord_fraction > 0.25
    )
    {
        if (
            stamina_score > 0.0 &&
            stamina_score >
                max_double(stream_score, tech_score) * 0.85
        )
        {
            family = CHART_FAMILY_STAMINA;
            confidence = max_double(confidence, 0.35);
        }
    }

    if (
        family != CHART_FAMILY_TECH &&
        family != CHART_FAMILY_JACK &&
        timing_irregularity > 0.4
    )
    {
        if (
            jack_density > 0.08 &&
            chord_fraction > 0.15 &&
            chord_fraction < 0.55 &&
            stream_purity > 0.40 &&
            stream_purity < 0.85
        )
        {
            const double winner_score =
                score_for_family(
                    out_result,
                    family
                );

            if (
                tech_score > 0.0 &&
                winner_score > 0.0 &&
                tech_score > winner_score * 0.75
            )
            {
                family = CHART_FAMILY_TECH;
                confidence = max_double(confidence, 0.35);
            }
        }
    }

    out_result->family = family;
    out_result->confidence = clamp_double(confidence, 0.0, 1.0);

    if (
        family == CHART_FAMILY_TECH ||
        (
            family == CHART_FAMILY_HYBRID &&
            tech_score >= top_score * 0.85
        )
    )
    {
        classify_tech_subtype(
            sunny,
            features,
            &out_result->tech_subtype,
            &out_result->tech_subtype_confidence
        );
    }

    return true;
}


ReformRuler FamilyClassifierRuler(
    const FamilyClassification *classification,
    bool *out_uses_skillset
)
{
    if (out_uses_skillset)
        *out_uses_skillset = false;

    if (
        !classification ||
        classification->confidence < 0.50
    )
    {
        return REFORM_RULER_GENERAL;
    }

    switch (classification->family)
    {
        case CHART_FAMILY_JACK:
            if (out_uses_skillset)
                *out_uses_skillset = true;
            return REFORM_RULER_JACK;

        case CHART_FAMILY_SPEED:
        case CHART_FAMILY_STREAM:
            if (out_uses_skillset)
                *out_uses_skillset = true;
            return REFORM_RULER_SPEED;

        case CHART_FAMILY_STAMINA:
            if (out_uses_skillset)
                *out_uses_skillset = true;
            return REFORM_RULER_STAMINA;

        case CHART_FAMILY_TECH:
            if (out_uses_skillset)
                *out_uses_skillset = true;
            return REFORM_RULER_TECH;

        case CHART_FAMILY_HYBRID:
        default:
            return REFORM_RULER_GENERAL;
    }
}
