#ifndef DANOVERLAY_APP_VIEW_H
#define DANOVERLAY_APP_VIEW_H

#include <stdbool.h>

#include "beatmap/beatmap.h"
#include "engine/chart_features.h"
#include "engine/family_classifier.h"
#include "engine/minacalc_bridge.h"
#include "engine/ln_course.h"
#include "engine/ln_profile.h"
#include "engine/reform_rank.h"
#include "engine/rhythm_profile.h"
#include "engine/ruler_sanity.h"
#include "engine/sunny_sr.h"
#include "tosu/tosu.h"
#include "settings/app_settings.h"
#include "ui/theme.h"

typedef enum
{
    DENSITY_GRAPH_OVERVIEW = 0,
    DENSITY_GRAPH_FOCUS = 1
} DensityGraphMode;

typedef struct
{
    const TosuSnapshot *tosu;
    const Beatmap *beatmap;
    const ChartFeatures *features;
    const SunnySrResult *sunny;
    const MinaCalcScores *msd;
    const LnCourseResult *ln_course;
    const LnPlayerProfile *ln_profile;
    const ReformRankResult *rank;
    const FamilyClassification *classification;
    const RhythmProfileResult *rhythm;
    const RulerSanityResult *sanity;

    DanTheme theme;

    ChartFamily active_family;
    double active_family_confidence;
    ReformRuler selected_ruler;

    const char *beatmap_status;

    bool beatmap_ready;
    bool features_ready;
    bool rhythm_ready;
    bool sunny_ready;
    bool sunny_current;
    bool msd_ready;
    bool ln_route;
    bool ln_course_ready;
    bool ln_profile_ready;
    bool rank_ready;
    bool classification_ready;
    bool sanity_ready;
    bool debug_visible;
    bool settings_visible;

    AppSettings *settings;

    AppSettingsViewMode view_mode;
    DensityGraphMode graph_mode;
    int focus_span_seconds;
} AppViewModel;

void AppViewDraw(
    const AppViewModel *model
);

#endif
