#include "pause_tracker.h"

#include <math.h>
#include <string.h>

#define PAUSE_FREEZE_CONFIRM_SECONDS 0.18
#define PAUSE_TIME_EPSILON_MS 0.5
#define PAUSE_JUMP_THRESHOLD_MS 2000.0
#define PAUSE_END_GUARD_MS 500.0

static void reset_transient_state(
    PauseTracker *tracker
)
{
    if (!tracker)
        return;

    tracker->has_time = false;
    tracker->saw_advancing = false;
    tracker->pause_candidate = false;
    tracker->paused = false;
    tracker->last_time_ms = 0.0;
    tracker->candidate_time_ms = 0.0;
    tracker->candidate_since_s = 0.0;
}

void PauseTrackerInit(
    PauseTracker *tracker
)
{
    PauseTrackerReset(tracker);
}

void PauseTrackerReset(
    PauseTracker *tracker
)
{
    if (!tracker)
        return;

    memset(tracker, 0, sizeof(*tracker));
}

static void append_marker(
    PauseTracker *tracker,
    double time_ms
)
{
    if (!tracker || !isfinite(time_ms))
        return;

    if (tracker->marker_count >= PAUSE_TRACKER_MAX_MARKERS)
        return;

    tracker->markers_ms[tracker->marker_count++] = time_ms;
}

void PauseTrackerUpdate(
    PauseTracker *tracker,
    bool gameplay_active,
    double song_time_ms,
    double chart_end_ms,
    double wall_time_s
)
{
    if (
        !tracker ||
        !isfinite(song_time_ms) ||
        !isfinite(wall_time_s)
    )
    {
        return;
    }

    if (!gameplay_active)
    {
        if (tracker->gameplay_active)
            tracker->marker_count = 0;

        tracker->gameplay_active = false;
        reset_transient_state(tracker);
        return;
    }

    if (!tracker->gameplay_active)
    {
        tracker->marker_count = 0;
        reset_transient_state(tracker);
        tracker->gameplay_active = true;
    }

    if (!tracker->has_time)
    {
        tracker->has_time = true;
        tracker->last_time_ms = song_time_ms;
        return;
    }

    const double delta =
        song_time_ms - tracker->last_time_ms;

    const bool near_chart_end =
        isfinite(chart_end_ms) &&
        chart_end_ms > 0.0 &&
        song_time_ms >= chart_end_ms - PAUSE_END_GUARD_MS;

    if (
        fabs(delta) > PAUSE_JUMP_THRESHOLD_MS &&
        !near_chart_end
    )
    {
        PauseTrackerReset(tracker);
        tracker->has_time = true;
        tracker->last_time_ms = song_time_ms;
        return;
    }

    if (delta > PAUSE_TIME_EPSILON_MS)
        tracker->saw_advancing = true;

    if (
        tracker->saw_advancing &&
        fabs(delta) <= PAUSE_TIME_EPSILON_MS &&
        !near_chart_end
    )
    {
        if (!tracker->paused)
        {
            if (!tracker->pause_candidate)
            {
                tracker->pause_candidate = true;
                tracker->candidate_time_ms = song_time_ms;
                tracker->candidate_since_s = wall_time_s;
            }
            else if (
                wall_time_s - tracker->candidate_since_s >=
                PAUSE_FREEZE_CONFIRM_SECONDS
            )
            {
                tracker->paused = true;
                tracker->pause_candidate = false;
                append_marker(
                    tracker,
                    tracker->candidate_time_ms
                );
            }
        }
    }
    else
    {
        tracker->pause_candidate = false;
        tracker->paused = false;
    }

    tracker->last_time_ms = song_time_ms;
}
