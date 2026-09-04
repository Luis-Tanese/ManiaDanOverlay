#ifndef DANOVERLAY_BEATMAP_H
#define DANOVERLAY_BEATMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BEATMAP_TEXT_MAX 256

typedef enum
{
    MANIA_NOTE_TAP,
    MANIA_NOTE_HOLD
} ManiaNoteType;

typedef struct
{
    ManiaNoteType type;

    uint8_t column;

    double start_ms;
    double end_ms;
} ManiaNote;

typedef struct
{
    int keys;

    double bpm;

    char artist[BEATMAP_TEXT_MAX];
    char title[BEATMAP_TEXT_MAX];
    char creator[BEATMAP_TEXT_MAX];
    char difficulty[BEATMAP_TEXT_MAX];

    ManiaNote *notes;

    size_t note_count;
    size_t note_capacity;

    size_t tap_count;
    size_t hold_count;

    double first_note_ms;
    double last_note_ms;
} Beatmap;

void BeatmapInit(Beatmap *map);
void BeatmapFree(Beatmap *map);

bool BeatmapAppendNote(
    Beatmap *map,
    ManiaNote note
);

double BeatmapHoldRatio(
    const Beatmap *map
);

#endif
