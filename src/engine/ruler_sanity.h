#ifndef DANOVERLAY_RULER_SANITY_H
#define DANOVERLAY_RULER_SANITY_H

#include <stdbool.h>

#include "engine/family_classifier.h"
#include "engine/minacalc_bridge.h"
#include "engine/reform_rank.h"

typedef enum
{
    RULER_SANITY_KEEP = 0,
    RULER_SANITY_PROMOTE,
    RULER_SANITY_VETO
} RulerSanityAction;

typedef struct
{
    ReformRuler requested_ruler;
    ReformRuler final_ruler;
    ReformRuler msd_top_ruler;

    ChartFamily family;
    double family_confidence;

    double jack_support;
    double speed_support;
    double stamina_support;
    double tech_support;

    double requested_support;
    double top_support;
    double second_support;
    double top_to_second_ratio;
    double top_to_requested_ratio;

    RulerSanityAction action;
    bool applied;
} RulerSanityResult;

bool RulerSanityEvaluate(
    ReformRuler requested_ruler,
    ChartFamily family,
    double family_confidence,
    const MinaCalcScores *base_msd,
    RulerSanityResult *out_result
);

/* 
 * TanMDO chooses the classified skill ruler for a supported rice family, retaining GENERAL if MSD strongly contradicts the classification.
 * this changes ruler selection, never Sunny SR or Reform calibration values.
 */
ReformRuler TanMdoRulerSelect(
    ReformRuler baseline_ruler,
    ChartFamily family,
    double family_confidence,
    const RulerSanityResult *sanity
);

const char *RulerSanityActionName(
    RulerSanityAction action
);

#endif
