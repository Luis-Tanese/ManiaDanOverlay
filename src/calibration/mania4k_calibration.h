#ifndef MANIADANOVERLAY_MANIA4K_CALIBRATION_H
#define MANIADANOVERLAY_MANIA4K_CALIBRATION_H

#include "engine/family_classifier.h"
#include "engine/ln_course.h"
#include "engine/ln_profile.h"
#include "engine/reform_rank.h"
#include "engine/ruler_sanity.h"

#define MANIA4K_SUBLEVEL_COUNT 5
#define MANIA4K_LN_FAMILY_COUNT 4
#define MANIA4K_TECH_SUBTYPE_COUNT 4
#define MANIA4K_RULER_SANITY_ACTION_COUNT 3
#define MANIA4K_RHYTHM_KIND_COUNT 6

typedef enum
{
    MANIA4K_LN_TRAIT_FORMAT_PERCENT = 0,
    MANIA4K_LN_TRAIT_FORMAT_RATE,
    MANIA4K_LN_TRAIT_FORMAT_DECIMAL
} Mania4KLnTraitFormat;

typedef struct
{
    double low;
    double high;
} Mania4KSignalRange;

typedef struct
{
    double means[REFORM_RULER_COUNT][REFORM_TIER_COUNT];
    const char *tier_names[REFORM_TIER_COUNT];
    const char *ruler_names[REFORM_RULER_COUNT];
    const char *sublevel_names[MANIA4K_SUBLEVEL_COUNT];
    double sublevel_edges[MANIA4K_SUBLEVEL_COUNT - 1];
} Mania4KReformCalibration;

typedef struct
{
    double chaos_transition;
    double chaos_density_cv;
    double chaos_nps_active_cv;
    double chaos_tech_dom;
    double chaos_anchor;

    double control_hand;
    double control_jump;
    double control_chord_complexity;
    double control_tech_dom;
    double control_density_regularity;
    double control_anchor;

    double hybrid_tech_dom;
    double hybrid_jump;
    double hybrid_density_cv;
    double hybrid_transition;
    double hybrid_stream_center;
    double hybrid_stream_width;
    double hybrid_stream_weight;

    double confidence_scale;
    double confidence_min;
    double confidence_fallback_cap;
    double chaos_transition_floor;
    double control_hand_floor;
    double control_chord_floor;
} Mania4KTechSubtypeCalibration;

typedef struct
{
    const char *family_names[CHART_FAMILY_COUNT];
    const char *tech_subtype_names[MANIA4K_TECH_SUBTYPE_COUNT];

    Mania4KTechSubtypeCalibration tech_subtype;

    double density_cv_cap;
    double repetitive_transition_center;
    double repetitive_scale;

    double jack_baseline_default;
    double jack_baseline_max;
    double jack_quarter_ms_scale;

    double bpm_signal_start;
    double bpm_signal_span;
    double bpm_signal_cap;

    double jack_weight_repetitive;
    double jack_weight_density;
    double jack_weight_chords;
    double jack_weight_excess;
    double jack_weight_vibro;
    double jack_weight_anchor;
    double jack_weight_sunny;

    double stream_weight_purity;
    double stream_chord_suppression_scale;
    double stream_weight_chord_regularity;
    double stream_weight_chord_penalty;
    double stream_weight_sunny;
    double stream_weight_density_regularity;
    double stream_jack_floor;
    double stream_weight_low_jack;

    double tech_weight_density_cv;
    double tech_weight_pattern_irregularity;
    double tech_weight_nps_active_cv;
    double tech_weight_sunny;
    double tech_weight_transition;
    double tech_weight_chords;

    double speed_weight_bpm;
    double speed_weight_purity;
    double speed_weight_density_regularity;
    double speed_peak_floor;
    double speed_weight_peak;
    double speed_chord_gate_min;
    double speed_chord_gate_scale;
    double speed_regularity_min;
    double speed_regularity_density_start;
    double speed_regularity_scale;

    double stamina_drain_start;
    double stamina_drain_span;
    double stamina_short_duration;
    double stamina_chord_req_start;
    double stamina_chord_req_span;
    double stamina_weight_drain;
    double stamina_weight_chords;
    double stamina_weight_stream_penalty;
    double stamina_weight_active_ratio;
    double stamina_weight_density_regularity;
    double stamina_weight_sustained_nps;
    double stamina_duration_scale;
    double stamina_weight_duration;
    double stamina_min_duration_gate;
    double stamina_short_penalty_scale;
    double stamina_repetitive_trigger;
    double stamina_repetitive_floor;
    double stamina_repetitive_scale;

    double confidence_scale;
    double hybrid_confidence_floor;
    double hybrid_confidence_min;

    double tech_rescue_density_cv;
    double tech_rescue_ratio;
    double tech_rescue_confidence;

    double speed_rescue_bpm;
    double speed_rescue_purity;
    double speed_rescue_chord_max;
    double speed_rescue_ratio;
    double speed_rescue_confidence;

    double long_jack_note_count;
    double long_jack_confidence_cap;

    double stamina_rescue_duration;
    double stamina_rescue_chord_min;
    double stamina_rescue_ratio;
    double stamina_rescue_confidence;

    double irregular_tech_timing_min;
    double irregular_tech_jack_min;
    double irregular_tech_chord_min;
    double irregular_tech_chord_max;
    double irregular_tech_purity_min;
    double irregular_tech_purity_max;
    double irregular_tech_ratio;
    double irregular_tech_confidence;

    double hybrid_tech_subtype_ratio;
    double ruler_confidence_floor;

    double rhythm_profile_sr_ceiling;
    double rhythm_profile_confidence_floor;
} Mania4KFamilyCalibration;

typedef struct
{
    double promotion_confidence[REFORM_RULER_COUNT];
    double promotion_ratio[REFORM_RULER_COUNT];
    double veto_confidence_breaks[3];
    double veto_ratio_floors[3];
    double veto_top_to_second_floor;
    const char *action_names[MANIA4K_RULER_SANITY_ACTION_COUNT];
} Mania4KRulerSanityCalibration;

typedef struct
{
    double dp_sr;
    double hold_occupancy;
    double simultaneous_hold;
    double release_density;
    double ln_duration_cv;
    double dp_sr_x_hold;
    double bias;
} Mania4KLnRegressionCalibration;

typedef struct
{
    double speed_release_density_interaction;
    double speed_density;
    double speed_stamina;
    double speed_release;
    double speed_no_overlap;
    double speed_overlap_penalty;
    double speed_hold_chord_penalty;

    double wall_occupancy_partner;
    double inverse_wall_core;
    double inverse_overlap;
    double inverse_hold_chords;
    double inverse_occupancy;

    double technical_duration_variation;
    double technical_timing_irregularity;
    double technical_transition_complexity;
    double technical_jack_structure;
    double technical_minijack_structure;
    double technical_pattern_irregularity;
    double technical_no_overlap;

    double speed_release_floor;
    double speed_density_floor;
    double speed_weak_multiplier;

    double inverse_overlap_floor;
    double inverse_hold_chord_floor;
    double inverse_occupancy_partner_floor;
    double inverse_missing_structure_multiplier;

    double occupancy_only_floor;
    double occupancy_only_overlap_ceiling;
    double occupancy_only_chord_ceiling;
    double occupancy_only_multiplier;

    double technical_signal_floor;
    double technical_weak_multiplier;
    double technical_duration_bonus_floor;
    double technical_jack_bonus_floor;
    double technical_transition_bonus_floor;
    double technical_bonus;

    double family_score_floor;
    double family_margin_floor;
} Mania4KLnFamilyCalibration;

typedef struct
{
    double stage_sr_means[LN_STAGE_COUNT];
    const char *stage_names[LN_STAGE_COUNT];
    const char *family_names[MANIA4K_LN_FAMILY_COUNT];
    const char *sublevel_names[MANIA4K_SUBLEVEL_COUNT];
    double sublevel_edges[MANIA4K_SUBLEVEL_COUNT - 1];

    double route_ln_ratio;
    double monotonic_epsilon;
    double minimum_dp;
    double maximum_dp;

    Mania4KLnRegressionCalibration regression;

    Mania4KSignalRange release_density;
    Mania4KSignalRange density;
    Mania4KSignalRange stamina;
    Mania4KSignalRange overlap;
    Mania4KSignalRange occupancy;
    Mania4KSignalRange hold_chords;
    Mania4KSignalRange duration_variation;
    Mania4KSignalRange timing_irregularity;
    Mania4KSignalRange transition_complexity;
    Mania4KSignalRange pattern_irregularity;
    Mania4KSignalRange jack_structure;
    Mania4KSignalRange minijack_structure;

    Mania4KLnFamilyCalibration family;
} Mania4KLnCourseCalibration;

typedef struct
{
    double trait_low[LN_PROFILE_TRAIT_COUNT];
    double trait_span[LN_PROFILE_TRAIT_COUNT];
    const char *trait_names[LN_PROFILE_TRAIT_COUNT];
    Mania4KLnTraitFormat trait_formats[LN_PROFILE_TRAIT_COUNT];
} Mania4KLnProfileCalibration;

typedef struct
{
    double rhythm_weights[MANIA4K_RHYTHM_KIND_COUNT];
    double stability_threshold_ms;
    double bpm_cluster_tolerance_ms;
    double important_cluster_ratio;
    double default_beat_length_ms;
    double hybrid_tech_bpm_ceiling;
    double stamina_bpm_floor;
    double stamina_duration_floor;
    double ruler_confidence_floor;
} Mania4KRhythmProfileCalibration;

typedef struct
{
    const char *revision;
    Mania4KReformCalibration reform;
    Mania4KFamilyCalibration family;
    Mania4KRulerSanityCalibration ruler_sanity;
    Mania4KLnCourseCalibration ln_course;
    Mania4KLnProfileCalibration ln_profile;
    Mania4KRhythmProfileCalibration rhythm_profile;
} Mania4KCalibration;

extern const Mania4KCalibration MANIA4K_CALIBRATION;

#endif
