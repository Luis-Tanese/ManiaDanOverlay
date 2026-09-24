#ifndef MANIADANOVERLAY_PAUSE_TRACKER_H
#define MANIADANOVERLAY_PAUSE_TRACKER_H

#include <stdbool.h>
#include <stddef.h>

#define PAUSE_TRACKER_MAX_MARKERS 64

typedef struct
{
    double markers_ms[PAUSE_TRACKER_MAX_MARKERS];
    size_t marker_count;

    bool gameplay_active;
    bool has_time;
    bool saw_advancing;
    bool pause_candidate;
    bool paused;

    double last_time_ms;
    double candidate_time_ms;
    double candidate_since_s;
} PauseTracker;

void PauseTrackerInit(PauseTracker *tracker);
void PauseTrackerReset(PauseTracker *tracker);

void PauseTrackerUpdate(
    PauseTracker *tracker,
    bool gameplay_active,
    double song_time_ms,
    double chart_end_ms,
    double wall_time_s
);

#endif
