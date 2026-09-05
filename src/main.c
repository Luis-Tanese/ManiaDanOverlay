#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"

#include "app/identity.h"

#include "beatmap/beatmap.h"
#include "beatmap/beatmap_fetch.h"

#include "engine/analysis_map.h"
#include "engine/chart_features.h"
#include "engine/family_classifier.h"
#include "engine/minacalc_bridge.h"
#include "engine/ln_course.h"
#include "engine/ln_profile.h"
#include "engine/reform_rank.h"
#include "engine/rhythm_profile.h"
#include "engine/ruler_sanity.h"
#include "calibration/mania4k_calibration.h"
#include "engine/sunny_sr.h"

#include "tosu/tosu.h"

#include "settings/app_settings.h"

#include "ui/app_view.h"
#include "ui/font.h"
#include "ui/theme.h"


static bool rate_changed(
    double a,
    double b
)
{
    return fabs(a - b) > 0.0001;
}


static void mark_settings_dirty(
    bool *dirty,
    double *dirty_since
)
{
    if (!dirty || !dirty_since)
        return;

    *dirty = true;
    *dirty_since = GetTime();
}


static size_t focus_span_index_for(
    const int *presets,
    size_t preset_count,
    int requested_seconds
)
{
    if (!presets || preset_count == 0)
        return 0;

    size_t best_index = 0;
    int best_distance = requested_seconds - presets[0];

    if (best_distance < 0)
        best_distance = -best_distance;

    for (size_t i = 1; i < preset_count; ++i)
    {
        int distance = requested_seconds - presets[i];

        if (distance < 0)
            distance = -distance;

        if (distance < best_distance)
        {
            best_distance = distance;
            best_index = i;
        }
    }

    return best_index;
}


static bool app_settings_equal(
    const AppSettings *a,
    const AppSettings *b
)
{
    if (!a || !b)
        return false;

    return
        a->version == b->version &&
        a->view_mode == b->view_mode &&
        a->graph_mode == b->graph_mode &&
        a->focus_span_seconds == b->focus_span_seconds &&
        a->always_on_top == b->always_on_top &&
        a->remember_window_position == b->remember_window_position &&
        a->remember_window_size == b->remember_window_size &&
        a->has_window_position == b->has_window_position &&
        a->window_x == b->window_x &&
        a->window_y == b->window_y &&
        a->hud_window_width == b->hud_window_width &&
        a->hud_window_height == b->hud_window_height &&
        a->extra_info_window_width == b->extra_info_window_width &&
        a->extra_info_window_height == b->extra_info_window_height;
}


/* 
 * a saved position only counts if enough of the window is still reachable.
 * this matters after monitors are unplugged or rearranged. 
 */

static bool saved_window_position_is_visible(
    const AppSettings *settings
)
{
    if (!settings || !settings->has_window_position)
        return false;

    const float left = (float)settings->window_x;
    const float top = (float)settings->window_y;
    const float right =
        left +
        (float)AppSettingsViewWidth(
            settings,
            settings->view_mode
        );

    const float bottom =
        top +
        (float)AppSettingsViewHeight(
            settings,
            settings->view_mode
        );

    const int monitor_count = GetMonitorCount();

    for (int monitor = 0; monitor < monitor_count; ++monitor)
    {
        const Vector2 position =
            GetMonitorPosition(monitor);

        const float monitor_left = position.x;
        const float monitor_top = position.y;
        const float monitor_right =
            monitor_left +
            (float)GetMonitorWidth(monitor);
        const float monitor_bottom =
            monitor_top +
            (float)GetMonitorHeight(monitor);

        const float intersection_left =
            fmax(left, monitor_left);
        const float intersection_top =
            fmax(top, monitor_top);
        const float intersection_right =
            fmin(right, monitor_right);
        const float intersection_bottom =
            fmin(bottom, monitor_bottom);

        if (
            intersection_right - intersection_left >= 64.0f &&
            intersection_bottom - intersection_top >= 64.0f
        )
        {
            return true;
        }
    }

    return false;
}


static void apply_window_size_for_view(
    AppSettings *settings,
    AppSettingsViewMode view_mode
)
{
    if (!settings)
        return;

    const int width =
        settings->remember_window_size
            ? AppSettingsViewWidth(settings, view_mode)
            : AppSettingsViewDefaultWidth(view_mode);

    const int height =
        settings->remember_window_size
            ? AppSettingsViewHeight(settings, view_mode)
            : AppSettingsViewDefaultHeight(view_mode);

    const int monitor = GetCurrentMonitor();
    const Vector2 old_position = GetWindowPosition();
    const Vector2 monitor_position = GetMonitorPosition(monitor);
    const int monitor_width = GetMonitorWidth(monitor);
    const int monitor_height = GetMonitorHeight(monitor);

    SetWindowMinSize(
        AppSettingsViewMinWidth(view_mode),
        AppSettingsViewMinHeight(view_mode)
    );

    SetWindowSize(width, height);

    const float min_visible = 64.0f;
    const float min_x =
        monitor_position.x - (float)width + min_visible;
    const float max_x =
        monitor_position.x + (float)monitor_width - min_visible;
    const float min_y =
        monitor_position.y - (float)height + min_visible;
    const float max_y =
        monitor_position.y + (float)monitor_height - min_visible;

    const int x =
        (int)fminf(fmaxf(old_position.x, min_x), max_x);
    const int y =
        (int)fminf(fmaxf(old_position.y, min_y), max_y);

    if (x != (int)old_position.x || y != (int)old_position.y)
    {
        SetWindowPosition(x, y);

        if (settings->remember_window_position)
        {
            settings->window_x = x;
            settings->window_y = y;
            settings->has_window_position = true;
        }
    }
}


int main(void)
{
    AppSettings settings;
    char settings_error[256] = "";

    const bool settings_loaded =
        AppSettingsLoad(
            &settings,
            settings_error,
            sizeof(settings_error)
        );

    if (!settings_loaded)
    {
        fprintf(
            stderr,
            "%s: settings load warning: %s\n",
            MANIADANOVERLAY_NAME,
            settings_error[0]
                ? settings_error
                : "unknown error"
        );
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);

    const int launch_width =
        settings.remember_window_size
            ? AppSettingsViewWidth(
                &settings,
                settings.view_mode
            )
            : AppSettingsViewDefaultWidth(
                settings.view_mode
            );

    const int launch_height =
        settings.remember_window_size
            ? AppSettingsViewHeight(
                &settings,
                settings.view_mode
            )
            : AppSettingsViewDefaultHeight(
                settings.view_mode
            );

    InitWindow(
        launch_width,
        launch_height,
        MANIADANOVERLAY_TITLE
    );

    SetExitKey(KEY_NULL);

    if (settings.always_on_top)
        SetWindowState(FLAG_WINDOW_TOPMOST);
    else
        ClearWindowState(FLAG_WINDOW_TOPMOST);

    SetWindowMinSize(
        AppSettingsViewMinWidth(settings.view_mode),
        AppSettingsViewMinHeight(settings.view_mode)
    );

    if (
        settings.remember_window_position &&
        settings.has_window_position &&
        saved_window_position_is_visible(&settings)
    )
    {
        SetWindowPosition(
            settings.window_x,
            settings.window_y
        );
    }

    SetTargetFPS(60);

    if (!UiFontsLoad())
    {
        TraceLog(LOG_ERROR, "Failed to load Torus fonts");
        CloseWindow();
        return 1;
    }

    if (!TosuInit())
    {
        TraceLog(LOG_ERROR, "Failed to initialize Tosu client");
        UiFontsUnload();
        CloseWindow();
        return 1;
    }

    if (!BeatmapFetchInit())
    {
        TraceLog(LOG_ERROR, "Failed to initialize beatmap fetcher");
        TosuShutdown();
        UiFontsUnload();
        CloseWindow();
        return 1;
    }

    if (!MinaCalcInit())
    {
        TraceLog(LOG_ERROR, "Failed to initialize MinaCalc");
        BeatmapFetchShutdown();
        TosuShutdown();
        UiFontsUnload();
        CloseWindow();
        return 1;
    }

    TraceLog(
        LOG_INFO,
        "MinaCalc initialized | calc version %d",
        MinaCalcVersion()
    );

    Beatmap beatmap;
    BeatmapInit(&beatmap);

    AnalysisMap analysis;
    AnalysisMapInit(&analysis);

    AnalysisMap classification_map;
    AnalysisMapInit(&classification_map);

    ChartFeatures features;
    ChartFeaturesInit(&features);

    SunnySrResult sunny = {0};
    FamilyClassification classification = {0};
    RhythmProfileResult rhythm_profile = {0};
    MinaCalcScores msd = {0};

    bool features_ready = false;
    bool rhythm_ready = false;
    bool msd_table_ready = false;
    bool msd_sample_ready = false;
    bool sunny_has_result = false;
    bool sunny_pending = false;

    double sunny_pending_since = 0.0;
    double sunny_result_rate = 1.0;

    char wanted_checksum[64] = "";
    char loaded_checksum[64] = "";
    char analysis_checksum[64] = "";
    char features_checksum[64] = "";
    char rhythm_checksum[64] = "";
    char msd_checksum[64] = "";
    char sunny_result_checksum[64] = "";

    char beatmap_status[256] = "Waiting for beatmap...";

    double next_fetch_time = 0.0;

    bool debug_visible = false;
    bool settings_visible = false;
    bool applied_always_on_top =
        settings.always_on_top;

    bool obs_hide_mode = false;
    Vector2 obs_restore_position = {0.0f, 0.0f};

    const int focus_span_presets[] = {15, 30, 45, 60, 90, 120, 180};
    const size_t focus_span_preset_count =
        sizeof(focus_span_presets) /
        sizeof(focus_span_presets[0]);

    DensityGraphMode graph_mode =
        settings.graph_mode == APP_SETTINGS_GRAPH_FOCUS
            ? DENSITY_GRAPH_FOCUS
            : DENSITY_GRAPH_OVERVIEW;

    size_t focus_span_index =
        focus_span_index_for(
            focus_span_presets,
            focus_span_preset_count,
            settings.focus_span_seconds
        );

    bool settings_dirty = false;
    double settings_dirty_since = 0.0;

    char sanity_log_checksum[64] = "";
    RulerSanityAction last_sanity_action = RULER_SANITY_KEEP;
    ReformRuler last_sanity_requested = REFORM_RULER_GENERAL;
    ReformRuler last_sanity_final = REFORM_RULER_GENERAL;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F2))
        {
            settings_visible =
                !settings_visible;
        }

        if (
            settings_visible &&
            IsKeyPressed(KEY_ESCAPE)
        )
        {
            settings_visible = false;
        }

        if (
            !settings_visible &&
            IsKeyPressed(KEY_F3)
        )
        {
            debug_visible = !debug_visible;
        }

        /* 
         * OBS Hide keeps the window mapped and rendering. 
         * save the real position before moving it almost completely off-screen. 
         */

        if (
            !settings_visible &&
            IsKeyPressed(KEY_F10)
        )
        {
            if (!obs_hide_mode)
            {
                obs_restore_position =
                    GetWindowPosition();

                if (settings.remember_window_position)
                {
                    settings.window_x =
                        (int)obs_restore_position.x;
                    settings.window_y =
                        (int)obs_restore_position.y;
                    settings.has_window_position = true;

                    mark_settings_dirty(
                        &settings_dirty,
                        &settings_dirty_since
                    );
                }

                const int monitor =
                    GetCurrentMonitor();

                const Vector2 monitor_position =
                    GetMonitorPosition(monitor);

                SetWindowPosition(
                    (int)monitor_position.x -
                        GetScreenWidth() +
                        2,
                    (int)monitor_position.y
                );

                obs_hide_mode = true;
            }
            else
            {
                SetWindowPosition(
                    (int)obs_restore_position.x,
                    (int)obs_restore_position.y
                );

                obs_hide_mode = false;
            }
        }

        if (
            !settings_visible &&
            IsKeyPressed(KEY_G)
        )
        {
            graph_mode =
                graph_mode == DENSITY_GRAPH_OVERVIEW
                    ? DENSITY_GRAPH_FOCUS
                    : DENSITY_GRAPH_OVERVIEW;

            settings.graph_mode =
                graph_mode == DENSITY_GRAPH_FOCUS
                    ? APP_SETTINGS_GRAPH_FOCUS
                    : APP_SETTINGS_GRAPH_OVERVIEW;

            mark_settings_dirty(
                &settings_dirty,
                &settings_dirty_since
            );
        }

        const bool smaller_focus_span =
            !settings_visible &&
            (
                IsKeyPressed(KEY_MINUS) ||
                IsKeyPressed(KEY_KP_SUBTRACT) ||
                IsKeyPressed(KEY_LEFT_BRACKET)
            );

        const bool larger_focus_span =
            !settings_visible &&
            (
                IsKeyPressed(KEY_EQUAL) ||
                IsKeyPressed(KEY_KP_ADD) ||
                IsKeyPressed(KEY_RIGHT_BRACKET)
            );

        if (
            smaller_focus_span &&
            focus_span_index > 0
        )
        {
            --focus_span_index;

            settings.focus_span_seconds =
                focus_span_presets[focus_span_index];

            mark_settings_dirty(
                &settings_dirty,
                &settings_dirty_since
            );
        }

        if (
            larger_focus_span &&
            focus_span_index + 1 <
                focus_span_preset_count
        )
        {
            ++focus_span_index;

            settings.focus_span_seconds =
                focus_span_presets[focus_span_index];

            mark_settings_dirty(
                &settings_dirty,
                &settings_dirty_since
            );
        }

        /* 
         * do not learn the temporary F10 position as the normal window position. 
         */

        if (
            !obs_hide_mode &&
            !IsWindowMinimized()
        )
        {
            if (settings.remember_window_size)
            {
                const int width = GetScreenWidth();
                const int height = GetScreenHeight();
                const int saved_width =
                    AppSettingsViewWidth(
                        &settings,
                        settings.view_mode
                    );
                const int saved_height =
                    AppSettingsViewHeight(
                        &settings,
                        settings.view_mode
                    );

                if (
                    width != saved_width ||
                    height != saved_height
                )
                {
                    AppSettingsSetViewSize(
                        &settings,
                        settings.view_mode,
                        width,
                        height
                    );

                    mark_settings_dirty(
                        &settings_dirty,
                        &settings_dirty_since
                    );
                }
            }

            if (settings.remember_window_position)
            {
                const Vector2 position = GetWindowPosition();
                const int x = (int)position.x;
                const int y = (int)position.y;

                if (
                    !settings.has_window_position ||
                    x != settings.window_x ||
                    y != settings.window_y
                )
                {
                    settings.window_x = x;
                    settings.window_y = y;
                    settings.has_window_position = true;

                    mark_settings_dirty(
                        &settings_dirty,
                        &settings_dirty_since
                    );
                }
            }
        }

        if (
            settings_dirty &&
            GetTime() - settings_dirty_since >= 0.60
        )
        {
            if (
                AppSettingsSave(
                    &settings,
                    settings_error,
                    sizeof(settings_error)
                )
            )
            {
                settings_dirty = false;
            }
            else
            {
                TraceLog(
                    LOG_WARNING,
                    "Settings save failed: %s",
                    settings_error[0]
                        ? settings_error
                        : "unknown error"
                );

                settings_dirty_since = GetTime();
            }
        }

        TosuUpdate();

        const TosuSnapshot *tosu = TosuGetSnapshot();


        if (
            tosu->connected &&
            tosu->mode == 3 &&
            tosu->checksum[0] != '\0'
        )
        {
            /* 
             * the checksum is our map generation. 
             * anything derived from the previous checksum is stale as soon as this changes. 
             */

            if (strcmp(wanted_checksum, tosu->checksum) != 0)
            {
                snprintf(
                    wanted_checksum,
                    sizeof(wanted_checksum),
                    "%s",
                    tosu->checksum
                );

                snprintf(
                    beatmap_status,
                    sizeof(beatmap_status),
                    "Loading beatmap..."
                );

                next_fetch_time = 0.0;

                sunny_has_result = false;
                features_ready = false;
                rhythm_ready = false;
                msd_table_ready = false;
                msd_sample_ready = false;

                sunny_result_checksum[0] = '\0';
                features_checksum[0] = '\0';
                rhythm_checksum[0] = '\0';
                msd_checksum[0] = '\0';
                analysis_checksum[0] = '\0';
            }

            const bool map_is_loaded =
                strcmp(
                    loaded_checksum,
                    wanted_checksum
                ) == 0;

            if (
                !map_is_loaded &&
                GetTime() >= next_fetch_time
            )
            {
                Beatmap candidate;
                BeatmapInit(&candidate);

                char error[256];

                if (
                    BeatmapFetchCurrent(
                        &candidate,
                        error,
                        sizeof(error)
                    )
                )
                {
                    BeatmapFree(&beatmap);
                    beatmap = candidate;

                    snprintf(
                        loaded_checksum,
                        sizeof(loaded_checksum),
                        "%s",
                        wanted_checksum
                    );

                    snprintf(
                        beatmap_status,
                        sizeof(beatmap_status),
                        "Beatmap parsed successfully"
                    );

                    TraceLog(
                        LOG_INFO,
                        "Beatmap parsed: %.2f BPM",
                        beatmap.bpm
                    );
                }
                else
                {
                    BeatmapFree(&candidate);

                    snprintf(
                        beatmap_status,
                        sizeof(beatmap_status),
                        "%s",
                        error
                    );

                    next_fetch_time = GetTime() + 0.5;
                }
            }
        }
        else if (
            tosu->connected &&
            tosu->mode != 3
        )
        {
            snprintf(
                beatmap_status,
                sizeof(beatmap_status),
                "Select a 4K osu!mania map"
            );
        }

        const bool beatmap_ready =
            loaded_checksum[0] != '\0' &&
            strcmp(loaded_checksum, tosu->checksum) == 0 &&
            beatmap.note_count > 0;

        if (
            beatmap_ready &&
            (
                !features_ready ||
                strcmp(
                    features_checksum,
                    loaded_checksum
                ) != 0
            )
        )
        {
            /* 
             * structural family features stay at base timing. 
             * changing clock rate should change difficulty, not what kind of chart it is. 
             */

            if (
                AnalysisMapBuild(
                    &beatmap,
                    1.0,
                    &classification_map
                )
            )
            {
                const double feature_start = GetTime();

                if (
                    ChartFeaturesBuild(
                        &classification_map,
                        &features
                    )
                )
                {
                    features_ready = true;

                    snprintf(
                        features_checksum,
                        sizeof(features_checksum),
                        "%s",
                        loaded_checksum
                    );

                    TraceLog(
                        LOG_INFO,
                        "Structural features built in %.2fms",
                        (GetTime() - feature_start) * 1000.0
                    );
                }
                else
                {
                    features_ready = false;

                    TraceLog(
                        LOG_ERROR,
                        "Structural feature extraction failed"
                    );
                }
            }
            else
            {
                features_ready = false;

                TraceLog(
                    LOG_ERROR,
                    "Failed to build base classification map"
                );
            }
        }

        const bool features_match_current =
            features_ready &&
            beatmap_ready &&
            strcmp(
                features_checksum,
                loaded_checksum
            ) == 0;

        if (
            beatmap_ready &&
            (
                !rhythm_ready ||
                strcmp(
                    rhythm_checksum,
                    loaded_checksum
                ) != 0
            )
        )
        {
            const double rhythm_start = GetTime();

            if (
                RhythmProfileEvaluate(
                    &beatmap,
                    &rhythm_profile
                )
            )
            {
                rhythm_ready = true;

                snprintf(
                    rhythm_checksum,
                    sizeof(rhythm_checksum),
                    "%s",
                    loaded_checksum
                );

                TraceLog(
                    LOG_INFO,
                    "Rhythm profile %s %.0f%% | %s/%s | %.2fms",
                    ChartFamilyName(rhythm_profile.family),
                    rhythm_profile.confidence * 100.0,
                    rhythm_profile.primary_rhythm,
                    rhythm_profile.subtype,
                    (GetTime() - rhythm_start) * 1000.0
                );
            }
            else
            {
                rhythm_ready = false;

                TraceLog(
                    LOG_ERROR,
                    "Rhythm profile classification failed"
                );
            }
        }

        const bool rhythm_matches_current =
            rhythm_ready &&
            beatmap_ready &&
            strcmp(
                rhythm_checksum,
                loaded_checksum
            ) == 0;

        if (
            beatmap_ready &&
            (
                !msd_table_ready ||
                strcmp(
                    msd_checksum,
                    loaded_checksum
                ) != 0
            )
        )
        {
            const double msd_start = GetTime();

            if (MinaCalcLoadBeatmap(&beatmap))
            {
                msd_table_ready = true;
                msd_sample_ready = false;

                snprintf(
                    msd_checksum,
                    sizeof(msd_checksum),
                    "%s",
                    loaded_checksum
                );

                TraceLog(
                    LOG_INFO,
                    "MinaCalc MSD table built in %.2fms | %zu rows | v%d",
                    (GetTime() - msd_start) * 1000.0,
                    beatmap.note_count,
                    MinaCalcVersion()
                );
            }
            else
            {
                msd_table_ready = false;
                msd_sample_ready = false;

                TraceLog(
                    LOG_ERROR,
                    "MinaCalc failed to analyze current beatmap"
                );
            }
        }

        const bool msd_table_matches_current =
            msd_table_ready &&
            beatmap_ready &&
            strcmp(
                msd_checksum,
                loaded_checksum
            ) == 0;

        if (
            msd_table_matches_current &&
            (
                !msd_sample_ready ||
                rate_changed(
                    msd.rate,
                    tosu->rate
                )
            )
        )
        {
            MinaCalcScores candidate = {0};

            if (
                MinaCalcSample(
                    tosu->rate,
                    &candidate
                )
            )
            {
                msd = candidate;
                msd_sample_ready = true;

                char patterns[128];

                MinaCalcFormatPatterns(
                    &msd,
                    patterns,
                    sizeof(patterns)
                );

                TraceLog(
                    LOG_INFO,
                    "MinaCalc %.2fx | %s | O %.2f S %.2f JS %.2f HS %.2f St %.2f Jk %.2f CJ %.2f T %.2f",
                    msd.rate,
                    patterns,
                    msd.overall,
                    msd.stream,
                    msd.jumpstream,
                    msd.handstream,
                    msd.stamina,
                    msd.jackspeed,
                    msd.chordjack,
                    msd.technical
                );
            }
            else
            {
                msd_sample_ready = false;
            }
        }

        const bool msd_matches_current =
            msd_sample_ready &&
            msd_table_matches_current &&
            !rate_changed(
                msd.rate,
                tosu->rate
            );

        if (beatmap_ready)
        {
            const bool wrong_map =
                strcmp(
                    analysis_checksum,
                    loaded_checksum
                ) != 0;

            const bool wrong_rate =
                rate_changed(
                    analysis.rate,
                    tosu->rate
                );

            if (
                wrong_map ||
                wrong_rate ||
                analysis.note_count == 0
            )
            {
                if (
                    AnalysisMapBuild(
                        &beatmap,
                        tosu->rate,
                        &analysis
                    )
                )
                {
                    snprintf(
                        analysis_checksum,
                        sizeof(analysis_checksum),
                        "%s",
                        loaded_checksum
                    );

                    TraceLog(
                        LOG_INFO,
                        "Analysis map rebuilt at %.3fx",
                        tosu->rate
                    );

                    sunny_pending = true;
                    sunny_pending_since = GetTime();
                }
                else
                {
                    TraceLog(
                        LOG_ERROR,
                        "Failed to build analysis map"
                    );
                }
            }
        }

        const bool analysis_ready =
            beatmap_ready &&
            analysis.note_count == beatmap.note_count &&
            strcmp(
                analysis_checksum,
                loaded_checksum
            ) == 0;

        /* 
         * rate sliders can update several times in a few frames. 
         * the analysis map follows immediately, but Sunny waits briefly for the rate to settle. 
         */

        if (
            analysis_ready &&
            sunny_pending &&
            (GetTime() - sunny_pending_since) >= 0.15
        )
        {
            SunnySrResult candidate;

            const double start_time = GetTime();

            if (
                SunnySrCalculate(
                    &analysis,
                    &candidate
                )
            )
            {
                sunny = candidate;
                sunny_has_result = true;
                sunny_result_rate = analysis.rate;

                snprintf(
                    sunny_result_checksum,
                    sizeof(sunny_result_checksum),
                    "%s",
                    analysis_checksum
                );

                TraceLog(
                    LOG_INFO,
                    "Sunny SR %.6f calculated in %.2fms | J %.2f P %.2f X %.2f",
                    sunny.star_rating,
                    (GetTime() - start_time) * 1000.0,
                    sunny.jbar_max,
                    sunny.pbar_max,
                    sunny.xbar_max
                );
            }
            else
            {
                TraceLog(
                    LOG_ERROR,
                    "Sunny SR calculation failed"
                );
            }

            sunny_pending = false;
        }

        const bool sunny_matches_current =
            sunny_has_result &&
            analysis_ready &&
            strcmp(
                sunny_result_checksum,
                analysis_checksum
            ) == 0 &&
            !rate_changed(
                sunny_result_rate,
                analysis.rate
            );

        bool classification_ready = false;

        ChartFamily active_family =
            CHART_FAMILY_HYBRID;

        double active_family_confidence =
            0.0;

        ReformRuler selected_ruler =
            REFORM_RULER_GENERAL;

        bool uses_skillset_ruler =
            false;

        if (sunny_has_result)
        {
            if (
                sunny.star_rating < MANIA4K_CALIBRATION.family.rhythm_profile_sr_ceiling &&
                rhythm_matches_current
            )
            {
                if (rhythm_profile.confidence >= MANIA4K_CALIBRATION.family.rhythm_profile_confidence_floor)
                {
                    classification_ready = true;

                    active_family =
                        rhythm_profile.family;

                    active_family_confidence =
                        rhythm_profile.confidence;

                    selected_ruler =
                        RhythmProfileRuler(
                            &rhythm_profile,
                            &uses_skillset_ruler
                        );
                }
                else if (features_match_current)
                {
                    classification_ready =
                        FamilyClassifierEvaluate(
                            &sunny,
                            &features,
                            beatmap.bpm,
                            &classification
                        );

                    if (classification_ready)
                    {
                        active_family =
                            classification.family;

                        active_family_confidence =
                            classification.confidence;

                        selected_ruler =
                            FamilyClassifierRuler(
                                &classification,
                                &uses_skillset_ruler
                            );
                    }
                }
            }
            else if (
                sunny.star_rating >= MANIA4K_CALIBRATION.family.rhythm_profile_sr_ceiling &&
                features_match_current
            )
            {
                classification_ready =
                    FamilyClassifierEvaluate(
                        &sunny,
                        &features,
                        beatmap.bpm,
                        &classification
                    );

                if (classification_ready)
                {
                    active_family =
                        classification.family;

                    active_family_confidence =
                        classification.confidence;

                    selected_ruler =
                        FamilyClassifierRuler(
                            &classification,
                            &uses_skillset_ruler
                        );
                }
            }
        }

        RulerSanityResult sanity = {0};

        if (
            classification_ready &&
            msd_matches_current &&
            sunny_matches_current
        )
        {
            const ReformRuler requested_ruler =
                selected_ruler;

            if (
                RulerSanityEvaluate(
                    requested_ruler,
                    active_family,
                    active_family_confidence,
                    &msd,
                    &sanity
                )
            )
            {
                selected_ruler =
                    sanity.final_ruler;

                uses_skillset_ruler =
                    selected_ruler !=
                    REFORM_RULER_GENERAL;

                const bool sanity_log_changed =
                    strcmp(
                        sanity_log_checksum,
                        loaded_checksum
                    ) != 0 ||
                    last_sanity_action !=
                        sanity.action ||
                    last_sanity_requested !=
                        sanity.requested_ruler ||
                    last_sanity_final !=
                        sanity.final_ruler;

                if (
                    sanity.applied &&
                    sanity_log_changed
                )
                {
                    TraceLog(
                        LOG_INFO,
                        "Ruler sanity %s | %s -> %s | family %s %.0f%% | MSD top %s %.2f vs %.2f",
                        RulerSanityActionName(
                            sanity.action
                        ),
                        ReformRulerName(
                            sanity.requested_ruler
                        ),
                        ReformRulerName(
                            sanity.final_ruler
                        ),
                        ChartFamilyName(
                            sanity.family
                        ),
                        sanity.family_confidence * 100.0,
                        ReformRulerName(
                            sanity.msd_top_ruler
                        ),
                        sanity.top_support,
                        sanity.second_support
                    );
                }

                snprintf(
                    sanity_log_checksum,
                    sizeof(sanity_log_checksum),
                    "%s",
                    loaded_checksum
                );

                last_sanity_action =
                    sanity.action;

                last_sanity_requested =
                    sanity.requested_ruler;

                last_sanity_final =
                    sanity.final_ruler;
            }
        }

        ReformRankResult rank_result = {0};

        const bool rank_ready =
            sunny_has_result &&
            ReformRankEvaluate(
                sunny.star_rating,
                selected_ruler,
                &rank_result
            );

        const bool ln_route =
            features_match_current &&
            LnCourseShouldRoute(&features);

        LnCourseResult ln_course = {0};
        LnPlayerProfile ln_profile = {0};

        const bool ln_course_ready =
            ln_route &&
            sunny_matches_current &&
            LnCourseEvaluate(
                sunny.star_rating,
                &features,
                msd_matches_current
                    ? &msd
                    : NULL,
                &ln_course
            );

        const bool ln_profile_ready =
            ln_route &&
            features_match_current &&
            LnPlayerProfileBuild(
                &features,
                &ln_profile
            );

        DanRank theme_rank =
            rank_ready
                ? (DanRank)rank_result.tier_index
                : DAN_RANK_1ST;

        if (ln_course_ready)
        {
            theme_rank =
                (DanRank)ln_course.stage;
        }

        const DanTheme theme =
            DanThemeForRank(theme_rank);

        AppViewModel view =
        {
            .tosu = tosu,
            .beatmap = &beatmap,
            .features = &features,
            .sunny = &sunny,
            .msd = &msd,
            .ln_course = &ln_course,
            .ln_profile = &ln_profile,
            .rank = &rank_result,
            .classification = &classification,
            .rhythm = &rhythm_profile,
            .sanity = &sanity,

            .theme = theme,

            .active_family = active_family,
            .active_family_confidence =
                active_family_confidence,
            .selected_ruler = selected_ruler,

            .beatmap_status = beatmap_status,

            .beatmap_ready = beatmap_ready,
            .features_ready = features_match_current,
            .rhythm_ready = rhythm_matches_current,
            .sunny_ready = sunny_has_result,
            .sunny_current = sunny_matches_current,
            .msd_ready = msd_matches_current,
            .ln_route = ln_route,
            .ln_course_ready = ln_course_ready,
            .ln_profile_ready = ln_profile_ready,
            .rank_ready = rank_ready,
            .classification_ready =
                classification_ready,
            .sanity_ready =
                classification_ready &&
                msd_matches_current &&
                sunny_matches_current,
            .debug_visible = debug_visible,
            .settings_visible = settings_visible,
            .settings = &settings,
            .view_mode = settings.view_mode,
            .graph_mode = graph_mode,
            .focus_span_seconds = focus_span_presets[focus_span_index]
        };

        const AppSettings settings_before_ui =
            settings;

        AppViewDraw(
            &view
        );

        if (
            !app_settings_equal(
                &settings_before_ui,
                &settings
            )
        )
        {
            AppSettingsSanitize(&settings);

            graph_mode =
                settings.graph_mode ==
                    APP_SETTINGS_GRAPH_FOCUS
                    ? DENSITY_GRAPH_FOCUS
                    : DENSITY_GRAPH_OVERVIEW;

            focus_span_index =
                focus_span_index_for(
                    focus_span_presets,
                    focus_span_preset_count,
                    settings.focus_span_seconds
                );

            if (
                settings.always_on_top !=
                applied_always_on_top
            )
            {
                if (settings.always_on_top)
                    SetWindowState(FLAG_WINDOW_TOPMOST);
                else
                    ClearWindowState(FLAG_WINDOW_TOPMOST);

                applied_always_on_top =
                    settings.always_on_top;
            }

            const bool view_changed =
                settings.view_mode !=
                    settings_before_ui.view_mode;

            const bool view_size_changed =
                settings.hud_window_width !=
                    settings_before_ui.hud_window_width ||
                settings.hud_window_height !=
                    settings_before_ui.hud_window_height ||
                settings.extra_info_window_width !=
                    settings_before_ui.extra_info_window_width ||
                settings.extra_info_window_height !=
                    settings_before_ui.extra_info_window_height ||
                settings.remember_window_size !=
                    settings_before_ui.remember_window_size;

            if (
                !obs_hide_mode &&
                (view_changed || view_size_changed)
            )
            {
                apply_window_size_for_view(
                    &settings,
                    settings.view_mode
                );
            }

            mark_settings_dirty(
                &settings_dirty,
                &settings_dirty_since
            );
        }
    }

    if (
        !AppSettingsSave(
            &settings,
            settings_error,
            sizeof(settings_error)
        )
    )
    {
        TraceLog(
            LOG_WARNING,
            "Final settings save failed: %s",
            settings_error[0]
                ? settings_error
                : "unknown error"
        );
    }

    ChartFeaturesFree(&features);
    AnalysisMapFree(&classification_map);
    AnalysisMapFree(&analysis);
    BeatmapFree(&beatmap);

    MinaCalcShutdown();
    BeatmapFetchShutdown();
    TosuShutdown();
    UiFontsUnload();
    CloseWindow();

    return 0;
}
