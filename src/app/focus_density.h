#ifndef MANIADANOVERLAY_FOCUS_DENSITY_H
#define MANIADANOVERLAY_FOCUS_DENSITY_H

#include <stdbool.h>
#include <stddef.h>

#include "beatmap/beatmap.h"
#include "engine/chart_features.h"

typedef struct
{
    NpsPoint *points;
    size_t count;
    int interval_ms;
} FocusDensityCurve;

void FocusDensityCurveInit(FocusDensityCurve *curve);
void FocusDensityCurveFree(FocusDensityCurve *curve);

bool FocusDensityCurveBuild(
    const Beatmap *beatmap,
    int interval_ms,
    FocusDensityCurve *out_curve
);

#endif
