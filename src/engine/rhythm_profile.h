#ifndef DANOVERLAY_RHYTHM_PROFILE_H
#define DANOVERLAY_RHYTHM_PROFILE_H

#include <stdbool.h>
#include <stddef.h>

#include "beatmap/beatmap.h"
#include "engine/family_classifier.h"
#include "engine/reform_rank.h"

#define RHYTHM_PROFILE_NAME_MAX 32

typedef struct
{
    ChartFamily family;
    double confidence;

    double stream_score;
    double jack_score;
    double tech_score;
    double speed_score;
    double stamina_score;

    char primary_rhythm[RHYTHM_PROFILE_NAME_MAX];
    char subtype[RHYTHM_PROFILE_NAME_MAX];

    double primary_bpm;
    bool primary_volatile;

    size_t texture_count;
    size_t profile_count;
} RhythmProfileResult;

bool RhythmProfileEvaluate(
    const Beatmap *beatmap,
    RhythmProfileResult *out_result
);

ReformRuler RhythmProfileRuler(
    const RhythmProfileResult *profile,
    bool *out_uses_skillset
);

#endif
