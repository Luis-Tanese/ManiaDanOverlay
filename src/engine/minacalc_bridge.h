#ifndef DANOVERLAY_MINACALC_BRIDGE_H
#define DANOVERLAY_MINACALC_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#include "beatmap/beatmap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    MINACALC_SKILL_STREAM = 0,
    MINACALC_SKILL_JUMPSTREAM,
    MINACALC_SKILL_HANDSTREAM,
    MINACALC_SKILL_STAMINA,
    MINACALC_SKILL_JACKSPEED,
    MINACALC_SKILL_CHORDJACK,
    MINACALC_SKILL_TECHNICAL,

    MINACALC_SKILL_COUNT
} MinaCalcSkillset;

typedef struct
{
    double rate;

    double overall;
    double stream;
    double jumpstream;
    double handstream;
    double stamina;
    double jackspeed;
    double chordjack;
    double technical;

    size_t row_count;
    int calc_version;
} MinaCalcScores;

typedef struct
{
    MinaCalcSkillset primary;
    MinaCalcSkillset secondary;
    MinaCalcSkillset tertiary;

    double primary_score;
    double secondary_score;
    double tertiary_score;

    bool has_secondary;
    bool has_tertiary;
} MinaCalcPatternSummary;

bool MinaCalcInit(void);
void MinaCalcShutdown(void);

bool MinaCalcLoadBeatmap(
    const Beatmap *beatmap
);

bool MinaCalcSample(
    double rate,
    MinaCalcScores *out_scores
);

bool MinaCalcIsReady(void);
int MinaCalcVersion(void);

const char *MinaCalcSkillsetName(
    MinaCalcSkillset skillset
);

void MinaCalcGetPatternSummary(
    const MinaCalcScores *scores,
    MinaCalcPatternSummary *out_summary
);

void MinaCalcFormatPatterns(
    const MinaCalcScores *scores,
    char *output,
    size_t output_size
);

void MinaCalcFormatPatternScores(
    const MinaCalcScores *scores,
    char *output,
    size_t output_size
);

#ifdef __cplusplus
}
#endif

#endif
