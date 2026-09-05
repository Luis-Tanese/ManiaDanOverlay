#include "calibration/mania4k_calibration.h"

#include <math.h>

/* 
* tuned 4K data lives here; the engine files keep the calculation itself.
* changing this table is a calibration change, not a cleanup refactor. 
*/

const Mania4KCalibration MANIA4K_CALIBRATION =
{
    .revision = "4K-R1",

    /* 
     * reform means are ordered GENERAL, JACK, SPEED, STAMINA, TECH. 
     * each row is 1st through Kappa; reform_rank.c derives boundaries from adjacent midpoints. 
     */

    .reform =
    {
        .means =
        {
            {
                2.94, 3.23, 3.51, 4.16, 4.71,
                5.12, 5.36, 5.83, 6.15, 6.55,
                6.56, 6.94, 7.41, 7.91, 9.03,
                9.40, 10.13, 10.74, 11.68, 12.25
            },
            {
                2.34, 2.35, 3.17, 3.48, 3.98,
                4.75, 4.97, 5.84, 5.85, 6.50,
                6.66, 6.90, 7.28, 7.91, 9.13,
                9.35, 10.38, 10.96, 12.27, 13.13
            },
            {
                2.94, 3.50, 3.78, 4.16, 4.86,
                5.29, 5.36, 5.71, 5.98, 6.22,
                6.58, 6.92, 7.23, 7.91, 9.25,
                9.55, 10.05, 10.67, 11.16, 12.05
            },
            {
                3.38, 3.48, 3.79, 4.69, 5.23,
                5.65, 5.75, 6.15, 6.26, 6.41,
                6.70, 7.04, 7.37, 8.04, 9.32,
                9.60, 9.96, 10.81, 11.66, 12.41
            },
            {
                2.84, 3.08, 3.09, 3.90, 4.18,
                4.50, 5.43, 5.69, 6.31, 6.46,
                6.63, 7.00, 7.31, 7.99, 9.22,
                9.60, 10.25, 10.64, 11.69, 12.10
            }
        },
        .tier_names =
        {
            "1ST", "2ND", "3RD", "4TH", "5TH",
            "6TH", "7TH", "8TH", "9TH", "10TH",
            "ALPHA", "BETA", "GAMMA", "DELTA", "EPSILON",
            "ZETA", "ETA", "THETA", "IOTA", "KAPPA"
        },
        .ruler_names =
        {
            "GENERAL", "JACK", "SPEED", "STAMINA", "TECH"
        },
        .sublevel_names =
        {
            "LOW", "MID-LOW", "MID", "MID-HIGH", "HIGH"
        },
        .sublevel_edges = {0.20, 0.40, 0.60, 0.80}
    },

    .family =
    {
        .family_names =
        {
            "STREAM", "JACK", "TECH", "SPEED", "STAMINA", "HYBRID"
        },
        .tech_subtype_names =
        {
            "GENERIC", "CHAOS TECH", "CONTROL TECH", "HYBRID TECH"
        },
        .tech_subtype =
        {
            .chaos_transition = 42.0,
            .chaos_density_cv = 26.0,
            .chaos_nps_active_cv = 20.0,
            .chaos_tech_dom = 18.0,
            .chaos_anchor = 10.0,

            .control_hand = 45.0,
            .control_jump = 10.0,
            .control_chord_complexity = 12.0,
            .control_tech_dom = 15.0,
            .control_density_regularity = 8.0,
            .control_anchor = 6.0,

            .hybrid_tech_dom = 22.0,
            .hybrid_jump = 14.0,
            .hybrid_density_cv = 10.0,
            .hybrid_transition = 10.0,
            .hybrid_stream_center = 0.45,
            .hybrid_stream_width = 0.90,
            .hybrid_stream_weight = 5.0,

            .confidence_scale = 0.18,
            .confidence_min = 0.15,
            .confidence_fallback_cap = 0.45,
            .chaos_transition_floor = 0.38,
            .control_hand_floor = 0.10,
            .control_chord_floor = 0.18
        },

        .density_cv_cap = 2.0,
        .repetitive_transition_center = 0.83,
        .repetitive_scale = 12.0,

        .jack_baseline_default = 0.30,
        .jack_baseline_max = 0.90,
        .jack_quarter_ms_scale = 250.0,

        .bpm_signal_start = 140.0,
        .bpm_signal_span = 80.0,
        .bpm_signal_cap = 1.50,

        .jack_weight_repetitive = 50.0,
        .jack_weight_density = 45.0,
        .jack_weight_chords = 20.0,
        .jack_weight_excess = 15.0,
        .jack_weight_vibro = 20.0,
        .jack_weight_anchor = 15.0,
        .jack_weight_sunny = 10.0,

        .stream_weight_purity = 45.0,
        .stream_chord_suppression_scale = 3.0,
        .stream_weight_chord_regularity = 15.0,
        .stream_weight_chord_penalty = 12.0,
        .stream_weight_sunny = 20.0,
        .stream_weight_density_regularity = 15.0,
        .stream_jack_floor = 0.50,
        .stream_weight_low_jack = 10.0,

        .tech_weight_density_cv = 55.0,
        .tech_weight_pattern_irregularity = 25.0,
        .tech_weight_nps_active_cv = 20.0,
        .tech_weight_sunny = 20.0,
        .tech_weight_transition = 12.0,
        .tech_weight_chords = 8.0,

        .speed_weight_bpm = 42.0,
        .speed_weight_purity = 22.0,
        .speed_weight_density_regularity = 10.0,
        .speed_peak_floor = 1.02,
        .speed_weight_peak = 12.0,
        .speed_chord_gate_min = 0.25,
        .speed_chord_gate_scale = 1.80,
        .speed_regularity_min = 0.50,
        .speed_regularity_density_start = 0.30,
        .speed_regularity_scale = 2.50,

        .stamina_drain_start = 60.0,
        .stamina_drain_span = 120.0,
        .stamina_short_duration = 90.0,
        .stamina_chord_req_start = 0.15,
        .stamina_chord_req_span = 0.20,
        .stamina_weight_drain = 28.0,
        .stamina_weight_chords = 25.0,
        .stamina_weight_stream_penalty = 20.0,
        .stamina_weight_active_ratio = 12.0,
        .stamina_weight_density_regularity = 8.0,
        .stamina_weight_sustained_nps = 0.30,
        .stamina_duration_scale = 150.0,
        .stamina_weight_duration = 12.0,
        .stamina_min_duration_gate = 0.40,
        .stamina_short_penalty_scale = 0.50,
        .stamina_repetitive_trigger = 0.15,
        .stamina_repetitive_floor = 0.30,
        .stamina_repetitive_scale = 0.70,

        .confidence_scale = 0.10,
        .hybrid_confidence_floor = 0.15,
        .hybrid_confidence_min = 0.10,

        .tech_rescue_density_cv = 0.35,
        .tech_rescue_ratio = 0.75,
        .tech_rescue_confidence = 0.30,

        .speed_rescue_bpm = 155.0,
        .speed_rescue_purity = 0.70,
        .speed_rescue_chord_max = 0.30,
        .speed_rescue_ratio = 0.35,
        .speed_rescue_confidence = 0.25,

        .long_jack_note_count = 15000.0,
        .long_jack_confidence_cap = 0.50,

        .stamina_rescue_duration = 120.0,
        .stamina_rescue_chord_min = 0.25,
        .stamina_rescue_ratio = 0.85,
        .stamina_rescue_confidence = 0.35,

        .irregular_tech_timing_min = 0.40,
        .irregular_tech_jack_min = 0.08,
        .irregular_tech_chord_min = 0.15,
        .irregular_tech_chord_max = 0.55,
        .irregular_tech_purity_min = 0.40,
        .irregular_tech_purity_max = 0.85,
        .irregular_tech_ratio = 0.75,
        .irregular_tech_confidence = 0.35,

        .hybrid_tech_subtype_ratio = 0.85,
        .ruler_confidence_floor = 0.50,

        .rhythm_profile_sr_ceiling = 7.0,
        .rhythm_profile_confidence_floor = 0.10
    },

    .ruler_sanity =
    {
        .promotion_confidence = {1.0, 0.28, 0.32, 0.34, 0.35},
        .promotion_ratio = {INFINITY, 1.08, 1.07, 1.08, 1.10},
        .veto_confidence_breaks = {0.60, 0.70, 0.80},
        .veto_ratio_floors = {1.18, 1.24, 1.32},
        .veto_top_to_second_floor = 1.06,
        .action_names = {"KEEP", "PROMOTE", "VETO"}
    },

    /* 
     * LN Course rating data and LN family description data share one section, but the engine keeps those two paths independent. 
     */

    .ln_course =
    {
        .stage_sr_means =
        {
            1.3050, 2.1515, 2.8504, 2.8504,
            3.2971, 3.2971, 3.8084, 3.9410,
            4.5798, 5.0721, 5.3570, 5.7562,
            6.4753, 6.8382, 7.1861, 7.5488
        },
        .stage_names =
        {
            "1ST", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "8TH",
            "9TH", "10TH", "YOAKE", "YUUGURE", "YORU", "YAMI", "YUME", "YOKAZE"
        },
        .family_names =
        {
            "All-round LN",
            "Jack / Technical LN",
            "Inverse / Wall LN",
            "Speed / Density LN"
        },
        .sublevel_names =
        {
            "LOW", "MID-LOW", "MID", "MID-HIGH", "HIGH"
        },
        .sublevel_edges = {0.20, 0.40, 0.60, 0.80},

        .route_ln_ratio = 0.45,
        .monotonic_epsilon = 0.001,
        .minimum_dp = 1.0,
        .maximum_dp = 16.99,

        .regression =
        {
            .dp_sr = 0.768445,
            .hold_occupancy = 0.579175,
            .simultaneous_hold = 5.109795,
            .release_density = 0.324758,
            .ln_duration_cv = 0.224097,
            .dp_sr_x_hold = -0.265177,
            .bias = -3.189884
        },

        .release_density = {2.0, 10.0},
        .density = {10.0, 32.0},
        .stamina = {0.35, 0.92},
        .overlap = {0.10, 0.58},
        .occupancy = {0.50, 0.97},
        .hold_chords = {0.10, 0.55},
        .duration_variation = {0.22, 1.05},
        .timing_irregularity = {0.25, 1.45},
        .transition_complexity = {0.30, 0.98},
        .pattern_irregularity = {0.03, 0.35},
        .jack_structure = {0.06, 0.48},
        .minijack_structure = {0.03, 0.28},

        .family =
        {
            .speed_release_density_interaction = 0.38,
            .speed_density = 0.24,
            .speed_stamina = 0.20,
            .speed_release = 0.12,
            .speed_no_overlap = 0.06,
            .speed_overlap_penalty = 0.16,
            .speed_hold_chord_penalty = 0.10,

            .wall_occupancy_partner = 0.72,
            .inverse_wall_core = 0.56,
            .inverse_overlap = 0.22,
            .inverse_hold_chords = 0.14,
            .inverse_occupancy = 0.08,

            .technical_duration_variation = 0.24,
            .technical_timing_irregularity = 0.20,
            .technical_transition_complexity = 0.18,
            .technical_jack_structure = 0.14,
            .technical_minijack_structure = 0.10,
            .technical_pattern_irregularity = 0.08,
            .technical_no_overlap = 0.06,

            .speed_release_floor = 0.30,
            .speed_density_floor = 0.28,
            .speed_weak_multiplier = 0.58,

            .inverse_overlap_floor = 0.22,
            .inverse_hold_chord_floor = 0.18,
            .inverse_occupancy_partner_floor = 0.84,
            .inverse_missing_structure_multiplier = 0.42,

            .occupancy_only_floor = 0.82,
            .occupancy_only_overlap_ceiling = 0.24,
            .occupancy_only_chord_ceiling = 0.20,
            .occupancy_only_multiplier = 0.55,

            .technical_signal_floor = 0.30,
            .technical_weak_multiplier = 0.60,
            .technical_duration_bonus_floor = 0.45,
            .technical_jack_bonus_floor = 0.35,
            .technical_transition_bonus_floor = 0.42,
            .technical_bonus = 0.08,

            .family_score_floor = 0.55,
            .family_margin_floor = 0.075
        }
    },

    /* 
     * LN profile ranges and labels only affect the information shown to players. 
     */

    .ln_profile =
    {
        .trait_low = {0.45, 1.50, 0.08, 0.20, 0.08},
        .trait_span = {0.50, 7.00, 0.45, 0.80, 0.50},
        .trait_names =
        {
            "Occupancy",
            "Release",
            "Overlap",
            "Duration CV",
            "Hold chords"
        },
        .trait_formats =
        {
            MANIA4K_LN_TRAIT_FORMAT_PERCENT,
            MANIA4K_LN_TRAIT_FORMAT_RATE,
            MANIA4K_LN_TRAIT_FORMAT_PERCENT,
            MANIA4K_LN_TRAIT_FORMAT_DECIMAL,
            MANIA4K_LN_TRAIT_FORMAT_PERCENT
        }
    },

    .rhythm_profile =
    {
        .rhythm_weights =
        {
            1.0 / 3.0,
            0.65,
            0.90,
            0.75,
            0.90,
            1.0
        },
        .stability_threshold_ms = 5.0,
        .bpm_cluster_tolerance_ms = 5.0,
        .important_cluster_ratio = 0.50,
        .default_beat_length_ms = 500.0,
        .hybrid_tech_bpm_ceiling = 150.0,
        .stamina_bpm_floor = 200.0,
        .stamina_duration_floor = 100.0,
        .ruler_confidence_floor = 0.50
    }
};
