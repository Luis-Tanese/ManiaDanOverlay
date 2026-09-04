#include "beatmap.h"

#include <stdlib.h>
#include <string.h>

void BeatmapInit(
    Beatmap *map
)
{
    if (!map)
        return;

    memset(
        map,
        0,
        sizeof(*map)
    );
}


void BeatmapFree(
    Beatmap *map
)
{
    if (!map)
        return;

    free(map->notes);

    BeatmapInit(map);
}


bool BeatmapAppendNote(
    Beatmap *map,
    ManiaNote note
)
{
    if (!map)
        return false;

    if (
        map->note_count >=
        map->note_capacity
    )
    {
        size_t new_capacity =
            map->note_capacity == 0
            ? 1024
            : map->note_capacity * 2;

        ManiaNote *new_notes =
            realloc(
                map->notes,
                new_capacity *
                sizeof(ManiaNote)
            );

        if (!new_notes)
            return false;

        map->notes =
            new_notes;

        map->note_capacity =
            new_capacity;
    }

    map->notes[
        map->note_count
    ] = note;

    map->note_count++;

    if (
        note.type ==
        MANIA_NOTE_HOLD
    )
    {
        map->hold_count++;
    }
    else
    {
        map->tap_count++;
    }

    if (
        map->note_count == 1 ||
        note.start_ms <
        map->first_note_ms
    )
    {
        map->first_note_ms =
            note.start_ms;
    }

    if (
        note.end_ms >
        map->last_note_ms
    )
    {
        map->last_note_ms =
            note.end_ms;
    }

    return true;
}


double BeatmapHoldRatio(
    const Beatmap *map
)
{
    if (
        !map ||
        map->note_count == 0
    )
    {
        return 0.0;
    }

    return
        (double)map->hold_count /
        (double)map->note_count;
}