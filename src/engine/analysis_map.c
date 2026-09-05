#include "analysis_map.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>


void AnalysisMapInit(
    AnalysisMap *map
)
{
    if (!map)
        return;

    memset(
        map,
        0,
        sizeof(*map)
    );

    map->rate = 1.0;
}


void AnalysisMapFree(
    AnalysisMap *map
)
{
    if (!map)
        return;

    free(
        map->notes
    );

    AnalysisMapInit(
        map
    );
}


bool AnalysisMapBuild(
    const Beatmap *source,
    double rate,
    AnalysisMap *out_map
)
{
    if (
        !source ||
        !out_map ||
        source->note_count == 0 ||
        !source->notes
    )
    {
        return false;
    }


    if (
        !isfinite(rate) ||
        rate <= 0.0
    )
    {
        return false;
    }


    AnalysisMap candidate;

    AnalysisMapInit(
        &candidate
    );


    candidate.notes =
        calloc(
            source->note_count,
            sizeof(ManiaNote)
        );


    if (!candidate.notes)
        return false;


    candidate.note_count =
        source->note_count;

    candidate.rate =
        rate;


    /* 
     * clock rate is represented by time compression. 
     * apply it to LN releases as well as note heads so hold duration stays physically correct. 
     */

    for (
        size_t i = 0;
        i < source->note_count;
        i++
    )
    {
        const ManiaNote *original =
            &source->notes[i];

        ManiaNote *adjusted =
            &candidate.notes[i];


        adjusted->type =
            original->type;

        adjusted->column =
            original->column;

        adjusted->start_ms =
            original->start_ms /
            rate;

        adjusted->end_ms =
            original->end_ms /
            rate;


        if (i == 0)
        {
            candidate.first_note_ms =
                adjusted->start_ms;

            candidate.last_note_ms =
                adjusted->end_ms;
        }
        else
        {
            if (
                adjusted->start_ms <
                candidate.first_note_ms
            )
            {
                candidate.first_note_ms =
                    adjusted->start_ms;
            }


            if (
                adjusted->end_ms >
                candidate.last_note_ms
            )
            {
                candidate.last_note_ms =
                    adjusted->end_ms;
            }
        }
    }


    candidate.duration_ms =
        candidate.last_note_ms -
        candidate.first_note_ms;


    if (
        candidate.duration_ms <
        0.0
    )
    {
        candidate.duration_ms =
            0.0;
    }


    AnalysisMapFree(
        out_map
    );


    *out_map =
        candidate;


    return true;
}