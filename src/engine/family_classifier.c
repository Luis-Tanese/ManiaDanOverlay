#include "family_classifier.h"
#include "calibration/mania4k_calibration.h"

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

    const Mania4KTechSubtypeCalibration *cal =
        &MANIA4K_CALIBRATION.family.tech_subtype;

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
        transition_var * cal->chaos_transition +
        density_cv * cal->chaos_density_cv +
        nps_active_cv * cal->chaos_nps_active_cv +
        tech_dom * cal->chaos_tech_dom -
        anchor_ratio * cal->chaos_anchor;

    const double control_score =
        hand_ratio * cal->control_hand +
        jump_ratio * cal->control_jump +
        chord_complexity * cal->control_chord_complexity +
        tech_dom * cal->control_tech_dom +
        max_double(0.0, 1.0 - density_cv) * cal->control_density_regularity +
        anchor_ratio * cal->control_anchor;

    const double hybrid_score =
        tech_dom * cal->hybrid_tech_dom +
        jump_ratio * cal->hybrid_jump +
        density_cv * cal->hybrid_density_cv +
        transition_var * cal->hybrid_transition +
        max_double(
            0.0,
            cal->hybrid_stream_width -
            fabs(stream_purity - cal->hybrid_stream_center)
        ) * cal->hybrid_stream_weight;

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
        max_double(total * cal->confidence_scale, 1.0),
        cal->confidence_min,
        1.0
    );

    TechSubtype subtype = ranked[0].subtype;

    const double chaos_transition_floor = cal->chaos_transition_floor;
    const double control_hand_floor = cal->control_hand_floor;
    const double control_chord_floor = cal->control_chord_floor;

    if (
        subtype == TECH_SUBTYPE_CHAOS &&
        transition_var < chaos_transition_floor
    )
    {
        subtype = TECH_SUBTYPE_HYBRID;
        confidence = min_double(confidence, cal->confidence_fallback_cap);
    }
    else if (
        subtype == TECH_SUBTYPE_CONTROL &&
        hand_ratio < control_hand_floor &&
        chord_complexity < control_chord_floor
    )
    {
        subtype = TECH_SUBTYPE_HYBRID;
        confidence = min_double(confidence, cal->confidence_fallback_cap);
    }

    *out_subtype = subtype;
    *out_confidence = confidence;
}


const char *ChartFamilyName(
    ChartFamily family
)
{
    if (family < 0 || family >= CHART_FAMILY_COUNT)
        return "UNKNOWN";

    return MANIA4K_CALIBRATION.family.family_names[family];
}


const char *TechSubtypeName(
    TechSubtype subtype
)
{
    if (subtype < 0 || subtype >= MANIA4K_TECH_SUBTYPE_COUNT)
        return "GENERIC";

    return MANIA4K_CALIBRATION.family.tech_subtype_names[subtype];
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

    const Mania4KFamilyCalibration *cal =
        &MANIA4K_CALIBRATION.family;

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
    const double density_cv = min_double(cal->density_cv_cap, features->density_cv);
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
        cal->repetitive_transition_center - transition_var
    );

    const double is_repetitive = min_double(
        1.0,
        rep_margin * cal->repetitive_scale
    );

    double jack_baseline = cal->jack_baseline_default;

    if (bpm > 0.0)
    {
        const double quarter_ms = 60000.0 / bpm;

        jack_baseline = min_double(
            cal->jack_baseline_max,
            max_double(
                0.0,
                1.0 - quarter_ms / cal->jack_quarter_ms_scale
            )
        );
    }

    const double jack_excess = max_double(
        0.0,
        jack_ratio_broad - jack_baseline
    );

    const double bpm_signal = min_double(
        cal->bpm_signal_cap,
        max_double(0.0, bpm - cal->bpm_signal_start) / cal->bpm_signal_span
    );

    const double jack_score =
        is_repetitive * cal->jack_weight_repetitive +
        jack_density * cal->jack_weight_density +
        chord_fraction * cal->jack_weight_chords +
        jack_excess * cal->jack_weight_excess +
        vibro_density * cal->jack_weight_vibro +
        anchor_ratio * cal->jack_weight_anchor +
        jack_dom * cal->jack_weight_sunny;

    const double stream_score =
        stream_purity * cal->stream_weight_purity +
        max_double(0.0, 1.0 - chord_fraction * cal->stream_chord_suppression_scale) *
            cal->stream_weight_chord_regularity -
        chord_fraction * cal->stream_weight_chord_penalty +
        stream_dom * cal->stream_weight_sunny +
        max_double(0.0, 1.0 - density_cv) * cal->stream_weight_density_regularity +
        max_double(0.0, cal->stream_jack_floor - jack_density) *
            cal->stream_weight_low_jack;

    const double tech_score =
        density_cv * cal->tech_weight_density_cv +
        pattern_irregularity * cal->tech_weight_pattern_irregularity +
        nps_active_cv * cal->tech_weight_nps_active_cv +
        tech_dom * cal->tech_weight_sunny +
        transition_var * cal->tech_weight_transition +
        chord_fraction * cal->tech_weight_chords;

    const double speed_raw =
        bpm_signal * cal->speed_weight_bpm +
        stream_purity * cal->speed_weight_purity +
        max_double(0.0, 1.0 - density_cv) * cal->speed_weight_density_regularity +
        max_double(0.0, peak_ratio - cal->speed_peak_floor) * cal->speed_weight_peak;

    const double speed_chord_gate = max_double(
        cal->speed_chord_gate_min,
        1.0 - chord_fraction * cal->speed_chord_gate_scale
    );

    const double speed_regularity = max_double(
        cal->speed_regularity_min,
        1.0 - max_double(0.0, density_cv - cal->speed_regularity_density_start) *
            cal->speed_regularity_scale
    );

    const double speed_score =
        speed_raw *
        speed_chord_gate *
        speed_regularity;

    const double drain_gate =
        max_double(0.0, drain_s - cal->stamina_drain_start) /
        cal->stamina_drain_span;

    const double short_penalty = max_double(
        0.0,
        1.0 - drain_s / cal->stamina_short_duration
    );

    const double chord_req = min_double(
        1.0,
        max_double(0.0, chord_fraction - cal->stamina_chord_req_start) /
        cal->stamina_chord_req_span
    );

    const double stamina_raw =
        drain_gate * cal->stamina_weight_drain +
        chord_fraction * cal->stamina_weight_chords -
        stream_purity * cal->stamina_weight_stream_penalty +
        nps_active_ratio * cal->stamina_weight_active_ratio +
        max_double(0.0, 1.0 - density_cv) * cal->stamina_weight_density_regularity +
        nps_sustained * cal->stamina_weight_sustained_nps +
        min_double(1.0, drain_s / cal->stamina_duration_scale) *
            cal->stamina_weight_duration;

    double stamina_score =
        stamina_raw *
        chord_req *
        max_double(
            cal->stamina_min_duration_gate,
            1.0 - short_penalty * cal->stamina_short_penalty_scale
        );

    if (is_repetitive > cal->stamina_repetitive_trigger)
    {
        stamina_score *= max_double(
            cal->stamina_repetitive_floor,
            1.0 - is_repetitive * cal->stamina_repetitive_scale
        );
    }

    out_result->stream_score = stream_score;
    out_result->jack_score = jack_score;
    out_result->tech_score = tech_score;
    out_result->speed_score = speed_score;
    out_result->stamina_score = stamina_score;

    /* 
     * family scores describe structure. 
     * low separation is intentionally treated as HYBRID instead of forcing a skill ruler from a weak winner. 
     */

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
        gap / max_double(total * cal->confidence_scale, 1.0)
    );

    ChartFamily family;

    if (confidence < cal->hybrid_confidence_floor)
    {
        family = CHART_FAMILY_HYBRID;
        confidence = max_double(cal->hybrid_confidence_min, confidence);
    }
    else
    {
        family = top_family;
    }

    if (
        (family == CHART_FAMILY_HYBRID ||
         family == CHART_FAMILY_STREAM) &&
        density_cv > cal->tech_rescue_density_cv
    )
    {
        if (
            tech_score > 0.0 &&
            stream_score > 0.0 &&
            tech_score / stream_score > cal->tech_rescue_ratio
        )
        {
            family = CHART_FAMILY_TECH;
            confidence = max_double(confidence, cal->tech_rescue_confidence);
        }
    }

    if (
        family == CHART_FAMILY_STREAM &&
        bpm >= cal->speed_rescue_bpm &&
        stream_purity > cal->speed_rescue_purity &&
        chord_fraction < cal->speed_rescue_chord_max
    )
    {
        if (
            speed_score > 0.0 &&
            stream_score > 0.0 &&
            speed_score / stream_score > cal->speed_rescue_ratio
        )
        {
            family = CHART_FAMILY_SPEED;
            confidence = max_double(confidence, cal->speed_rescue_confidence);
        }
    }

    if (
        family == CHART_FAMILY_JACK &&
        sunny->total_notes_eff > cal->long_jack_note_count
    )
    {
        family =
            stamina_score > tech_score
                ? CHART_FAMILY_STAMINA
                : CHART_FAMILY_HYBRID;

        confidence = min_double(confidence, cal->long_jack_confidence_cap);
    }

    if (
        (family == CHART_FAMILY_STREAM ||
         family == CHART_FAMILY_TECH) &&
        drain_s > cal->stamina_rescue_duration &&
        chord_fraction > cal->stamina_rescue_chord_min
    )
    {
        if (
            stamina_score > 0.0 &&
            stamina_score >
                max_double(stream_score, tech_score) * cal->stamina_rescue_ratio
        )
        {
            family = CHART_FAMILY_STAMINA;
            confidence = max_double(confidence, cal->stamina_rescue_confidence);
        }
    }

    if (
        family != CHART_FAMILY_TECH &&
        family != CHART_FAMILY_JACK &&
        timing_irregularity > cal->irregular_tech_timing_min
    )
    {
        if (
            jack_density > cal->irregular_tech_jack_min &&
            chord_fraction > cal->irregular_tech_chord_min &&
            chord_fraction < cal->irregular_tech_chord_max &&
            stream_purity > cal->irregular_tech_purity_min &&
            stream_purity < cal->irregular_tech_purity_max
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
                tech_score > winner_score * cal->irregular_tech_ratio
            )
            {
                family = CHART_FAMILY_TECH;
                confidence = max_double(confidence, cal->irregular_tech_confidence);
            }
        }
    }

    out_result->family = family;
    out_result->confidence = clamp_double(confidence, 0.0, 1.0);

    if (
        family == CHART_FAMILY_TECH ||
        (
            family == CHART_FAMILY_HYBRID &&
            tech_score >= top_score * cal->hybrid_tech_subtype_ratio
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


/* 
 * a family name is not automatically a ruler decision. 
 * weak classifications fall back to GENERAL so the specialized calibration stays conservative. 
 */

ReformRuler FamilyClassifierRuler(
    const FamilyClassification *classification,
    bool *out_uses_skillset
)
{
    if (out_uses_skillset)
        *out_uses_skillset = false;

    if (
        !classification ||
        classification->confidence < MANIA4K_CALIBRATION.family.ruler_confidence_floor
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
