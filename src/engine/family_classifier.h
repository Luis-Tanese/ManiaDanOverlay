#ifndef DANOVERLAY_FAMILY_CLASSIFIER_H
#define DANOVERLAY_FAMILY_CLASSIFIER_H

#include <stdbool.h>

#include "engine/chart_features.h"
#include "engine/reform_rank.h"
#include "engine/sunny_sr.h"

typedef enum
{
    CHART_FAMILY_STREAM = 0,
    CHART_FAMILY_JACK,
    CHART_FAMILY_TECH,
    CHART_FAMILY_SPEED,
    CHART_FAMILY_STAMINA,
    CHART_FAMILY_HYBRID,

    CHART_FAMILY_COUNT
} ChartFamily;

typedef enum
{
    TECH_SUBTYPE_GENERIC = 0,
    TECH_SUBTYPE_CHAOS,
    TECH_SUBTYPE_CONTROL,
    TECH_SUBTYPE_HYBRID
} TechSubtype;

typedef struct
{
    ChartFamily family;
    double confidence;

    double stream_score;
    double jack_score;
    double tech_score;
    double speed_score;
    double stamina_score;

    TechSubtype tech_subtype;
    double tech_subtype_confidence;
} FamilyClassification;

const char *ChartFamilyName(
    ChartFamily family
);

const char *TechSubtypeName(
    TechSubtype subtype
);

bool FamilyClassifierEvaluate(
    const SunnySrResult *sunny,
    const ChartFeatures *features,
    double bpm,
    FamilyClassification *out_result
);

ReformRuler FamilyClassifierRuler(
    const FamilyClassification *classification,
    bool *out_uses_skillset
);

#endif
