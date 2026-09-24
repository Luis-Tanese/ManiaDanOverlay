#include "focus_density.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FOCUS_DENSITY_WINDOW_MS 500.0

static int compare_double(
    const void *a,
    const void *b
)
{
    const double left = *(const double *)a;
    const double right = *(const double *)b;

    if (left < right)
        return -1;
    if (left > right)
        return 1;
    return 0;
}

void FocusDensityCurveInit(
    FocusDensityCurve *curve
)
{
    if (!curve)
        return;

    memset(curve, 0, sizeof(*curve));
}

void FocusDensityCurveFree(
    FocusDensityCurve *curve
)
{
    if (!curve)
        return;

    free(curve->points);
    FocusDensityCurveInit(curve);
}

bool FocusDensityCurveBuild(
    const Beatmap *beatmap,
    int interval_ms,
    FocusDensityCurve *out_curve
)
{
    if (
        !beatmap ||
        !beatmap->notes ||
        beatmap->note_count == 0 ||
        interval_ms <= 0 ||
        !out_curve
    )
    {
        return false;
    }

    double *times =
        malloc(beatmap->note_count * sizeof(double));

    if (!times)
        return false;

    for (size_t i = 0; i < beatmap->note_count; ++i)
        times[i] = beatmap->notes[i].start_ms;

    qsort(
        times,
        beatmap->note_count,
        sizeof(double),
        compare_double
    );

    const double first_time = times[0];
    const double last_time = times[beatmap->note_count - 1];
    const double stride = (double)interval_ms;

    size_t capacity =
        (size_t)floor((last_time - first_time) / stride) + 2;

    if (capacity < 1)
        capacity = 1;

    NpsPoint *points =
        malloc(capacity * sizeof(NpsPoint));

    if (!points)
    {
        free(times);
        return false;
    }

    size_t count = 0;
    size_t left = 0;
    size_t right = 0;

    for (
        double time_ms = first_time;
        time_ms <= last_time + 0.001;
        time_ms += stride
    )
    {
        const double lo =
            time_ms - FOCUS_DENSITY_WINDOW_MS * 0.5;
        const double hi =
            time_ms + FOCUS_DENSITY_WINDOW_MS * 0.5;

        while (
            left < beatmap->note_count &&
            times[left] < lo
        )
        {
            ++left;
        }

        if (right < left)
            right = left;

        while (
            right < beatmap->note_count &&
            times[right] <= hi
        )
        {
            ++right;
        }

        if (count >= capacity)
        {
            capacity *= 2;

            NpsPoint *grown =
                realloc(points, capacity * sizeof(NpsPoint));

            if (!grown)
            {
                free(points);
                free(times);
                return false;
            }

            points = grown;
        }

        points[count].time_ms = time_ms;
        points[count].nps =
            (double)(right - left) /
            (FOCUS_DENSITY_WINDOW_MS / 1000.0);

        ++count;
    }

    if (
        count > 0 &&
        points[count - 1].time_ms < last_time - 0.001
    )
    {
        if (count >= capacity)
        {
            ++capacity;

            NpsPoint *grown =
                realloc(points, capacity * sizeof(NpsPoint));

            if (!grown)
            {
                free(points);
                free(times);
                return false;
            }

            points = grown;
        }

        const double lo =
            last_time - FOCUS_DENSITY_WINDOW_MS * 0.5;
        const double hi =
            last_time + FOCUS_DENSITY_WINDOW_MS * 0.5;

        left = 0;
        right = 0;

        while (
            left < beatmap->note_count &&
            times[left] < lo
        )
        {
            ++left;
        }

        right = left;

        while (
            right < beatmap->note_count &&
            times[right] <= hi
        )
        {
            ++right;
        }

        points[count].time_ms = last_time;
        points[count].nps =
            (double)(right - left) /
            (FOCUS_DENSITY_WINDOW_MS / 1000.0);

        ++count;
    }

    free(times);

    FocusDensityCurve candidate =
    {
        .points = points,
        .count = count,
        .interval_ms = interval_ms
    };

    FocusDensityCurveFree(out_curve);
    *out_curve = candidate;
    return true;
}
