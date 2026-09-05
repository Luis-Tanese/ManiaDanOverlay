#include "app_view.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "ui/font.h"
#include "ui/settings_panel.h"

static const Color BG = {21, 23, 25, 255};
static const Color PANEL = {29, 32, 34, 255};
static const Color PANEL_INSET = {25, 28, 30, 255};
static const Color TEXT = {232, 229, 223, 255};
static const Color MUTED = {140, 140, 135, 255};
static const Color RULE = {52, 54, 56, 255};
static const Color GOOD = {120, 210, 140, 255};
static const Color BAD = {220, 100, 100, 255};

static int g_canvas_width = APP_SETTINGS_DESIGN_WIDTH;
static int g_canvas_height = APP_SETTINGS_DESIGN_HEIGHT;
static float g_ui_scale = 1.0f;

static float minimum_physical_font_size(float nominal_size)
{
    if (nominal_size >= 30.0f)
        return 17.0f;
    if (nominal_size >= 22.0f)
        return 13.0f;
    if (nominal_size >= 18.0f)
        return 11.5f;
    if (nominal_size >= 15.0f)
        return 10.0f;
    if (nominal_size >= 13.0f)
        return 9.0f;
    return 8.0f;
}

static float readable_font_size(float nominal_size)
{
    const float scale =
        g_ui_scale > 0.001f
            ? g_ui_scale
            : 1.0f;

    const float physical_size = nominal_size * scale;
    const float minimum_size = minimum_physical_font_size(nominal_size);

    if (physical_size >= minimum_size)
        return nominal_size;

    return minimum_size / scale;
}

static Vector2 ui_measure_text(
    Font font,
    const char *text,
    float nominal_size,
    float spacing)
{
    return MeasureTextEx(
        font,
        text,
        readable_font_size(nominal_size),
        spacing
    );
}

static void ui_draw_text(
    Font font,
    const char *text,
    Vector2 position,
    float nominal_size,
    float spacing,
    Color color)
{
    DrawTextEx(
        font,
        text,
        position,
        readable_font_size(nominal_size),
        spacing,
        color
    );
}

static float hud_font_size(
    float nominal_size,
    float minimum_physical_size)
{
    const float scale =
        g_ui_scale > 0.001f
            ? g_ui_scale
            : 1.0f;

    const float physical_size = nominal_size * scale;

    if (physical_size >= minimum_physical_size)
        return nominal_size;

    return minimum_physical_size / scale;
}

static Vector2 hud_measure_text(
    Font font,
    const char *text,
    float nominal_size,
    float minimum_physical_size)
{
    return MeasureTextEx(
        font,
        text,
        hud_font_size(nominal_size, minimum_physical_size),
        0.0f
    );
}

static void hud_draw_text(
    Font font,
    const char *text,
    Vector2 position,
    float nominal_size,
    float minimum_physical_size,
    Color color)
{
    DrawTextEx(
        font,
        text,
        position,
        hud_font_size(nominal_size, minimum_physical_size),
        0.0f,
        color
    );
}

static float hud_fit_font_size(
    Font font,
    const char *text,
    float nominal_size,
    float minimum_physical_size,
    float maximum_width)
{
    const float scale =
        g_ui_scale > 0.001f
            ? g_ui_scale
            : 1.0f;

    float size =
        hud_font_size(
            nominal_size,
            minimum_physical_size
        );

    const float hard_minimum =
        16.0f / scale;

    while (
        size > hard_minimum &&
        MeasureTextEx(font, text, size, 0.0f).x > maximum_width
    )
    {
        size -= 1.0f / scale;
    }

    if (size < hard_minimum)
        size = hard_minimum;

    return size;
}

static Camera2D make_ui_camera(bool debug_visible)
{
    const float window_width = fmaxf((float)GetScreenWidth(), 1.0f);
    const float window_height = fmaxf((float)GetScreenHeight(), 1.0f);

    const float required_width = (float)APP_SETTINGS_DESIGN_WIDTH;
    const float required_height =
        debug_visible
            ? 900.0f
            : (float)APP_SETTINGS_DESIGN_HEIGHT;

    float scale = fminf(
        window_width / required_width,
        window_height / required_height
    );

    if (!isfinite(scale) || scale <= 0.0f)
        scale = 1.0f;

    g_ui_scale = scale;

    g_canvas_width = (int)ceilf(window_width / scale);
    g_canvas_height = (int)ceilf(window_height / scale);

    if (g_canvas_width < APP_SETTINGS_DESIGN_WIDTH)
        g_canvas_width = APP_SETTINGS_DESIGN_WIDTH;

    const int required_canvas_height =
        debug_visible
            ? 900
            : APP_SETTINGS_DESIGN_HEIGHT;

    if (g_canvas_height < required_canvas_height)
        g_canvas_height = required_canvas_height;

    Camera2D camera = {0};
    camera.target = (Vector2){0.0f, 0.0f};
    camera.offset = (Vector2){0.0f, 0.0f};
    camera.rotation = 0.0f;
    camera.zoom = scale;

    return camera;
}

static double clampd(double value, double minimum, double maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static double lerpd(double a, double b, double t)
{
    return a + (b - a) * t;
}

static double visual_playback_time_ms(
    const TosuSnapshot *tosu
)
{
    static double last_raw_time = -1.0;
    static double anchor_raw_time = 0.0;
    static double anchor_wall_time = 0.0;
    static double anchor_rate = 1.0;
    static bool was_advancing = false;

    if (!tosu)
        return 0.0;

    const double now = GetTime();
    const double raw = tosu->time_ms;

    if (
        last_raw_time < 0.0 ||
        fabs(raw - last_raw_time) > 0.001
    )
    {
        const double delta =
            last_raw_time < 0.0
                ? 0.0
                : raw - last_raw_time;

        was_advancing =
            last_raw_time >= 0.0 &&
            delta > 0.0 &&
            delta < 3000.0;

        last_raw_time = raw;
        anchor_raw_time = raw;
        anchor_wall_time = now;
        anchor_rate =
            tosu->rate > 0.01
                ? tosu->rate
                : 1.0;
    }

    double display = anchor_raw_time;
    const double age = now - anchor_wall_time;

    if (was_advancing && age >= 0.0 && age <= 0.22)
    {
        display +=
            age *
            1000.0 *
            anchor_rate;
    }

    if (tosu->length_ms > 0.0)
    {
        display = clampd(
            display,
            0.0,
            tosu->length_ms
        );
    }

    return display;
}

static void format_time(double milliseconds, char *output, size_t output_size)
{
    if (!output || output_size == 0)
        return;

    if (milliseconds < 0.0)
        milliseconds = 0.0;

    const int total_seconds = (int)(milliseconds / 1000.0);
    const int hours = total_seconds / 3600;
    const int minutes = (total_seconds / 60) % 60;
    const int seconds = total_seconds % 60;

    if (hours > 0)
    {
        snprintf(output, output_size, "%d:%02d:%02d", hours, minutes, seconds);
    }
    else
    {
        snprintf(output, output_size, "%02d:%02d", minutes, seconds);
    }
}

static Color translucent(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

static float physical_pixel_step(void)
{
    const float scale =
        g_ui_scale > 0.001f
            ? g_ui_scale
            : 1.0f;

    return 1.0f / scale;
}

static void draw_panel(Rectangle bounds)
{
    DrawRectangleRounded(bounds, 0.035f, 8, PANEL);
    DrawRectangleRoundedLinesEx(bounds, 0.035f, 8, 1.0f, RULE);
}

static void draw_section_label(const char *text, float x, float y)
{
    ui_draw_text(UiFontBold(), text, (Vector2){x, y}, 13.0f, 0.0f, MUTED);
}

static void draw_rank_badge(const ReformRankResult *rank, DanTheme theme, Rectangle bounds)
{
    if (!rank)
        return;

    const char *name = ReformTierName(rank->tier_index);

    DrawRectangleRounded(bounds, 0.08f, 8, theme.badge_background);
    DrawRectangleRoundedLinesEx(bounds, 0.08f, 8, 2.0f, theme.badge_border);

    const float font_size = strlen(name) >= 7 ? 28.0f : 33.0f;
    const Vector2 size = ui_measure_text(UiFontBold(), name, font_size, 0.0f);

    ui_draw_text(
        UiFontBold(),
        name,
        (Vector2){bounds.x + (bounds.width - size.x) * 0.5f,
                  bounds.y + (bounds.height - size.y) * 0.5f - 1.0f},
        font_size,
        0.0f,
        theme.badge_text);
}

static void draw_ln_badge(
    const LnCourseResult *ln,
    DanTheme theme,
    Rectangle bounds
)
{
    if (!ln || !ln->valid)
        return;

    const char *name =
        LnCourseStageName(ln->stage);

    DrawRectangleRounded(
        bounds,
        0.08f,
        8,
        theme.badge_background
    );

    DrawRectangleRoundedLinesEx(
        bounds,
        0.08f,
        8,
        2.0f,
        theme.badge_border
    );

    const float font_size =
        strlen(name) >= 7
            ? 25.0f
            : 32.0f;

    const Vector2 size =
        ui_measure_text(
            UiFontBold(),
            name,
            font_size,
            0.0f
        );

    ui_draw_text(
        UiFontBold(),
        name,
        (Vector2)
        {
            bounds.x +
                (bounds.width - size.x) * 0.5f,
            bounds.y +
                (bounds.height - size.y) * 0.5f -
                1.0f
        },
        font_size,
        0.0f,
        theme.badge_text
    );
}

static void draw_pattern_row(
    const char *name,
    double value,
    double maximum,
    Rectangle bounds,
    DanTheme theme,
    bool primary)
{
    DrawRectangleRounded(bounds, 0.12f, 6, PANEL_INSET);

    const double ratio = maximum > 0.0 ? fmin(fmax(value / maximum, 0.0), 1.0) : 0.0;

    Rectangle fill = bounds;
    fill.width *= (float)ratio;

    DrawRectangleRounded(fill, 0.12f, 6, translucent(theme.accent, primary ? 56 : 34));

    ui_draw_text(
        primary ? UiFontBold() : UiFontRegular(),
        name,
        (Vector2){bounds.x + 12.0f, bounds.y + 6.0f},
        15.0f,
        0.0f,
        primary ? theme.accent : TEXT);

    char score[64];
    snprintf(score, sizeof(score), "%.2f", value);

    const Vector2 score_size = ui_measure_text(UiFontBold(), score, 15.0f, 0.0f);

    ui_draw_text(
        UiFontBold(),
        score,
        (Vector2){bounds.x + bounds.width - score_size.x - 12.0f, bounds.y + 6.0f},
        15.0f,
        0.0f,
        primary ? theme.accent : TEXT);
}

static void draw_pattern_panel(
    const AppViewModel *model,
    Rectangle bounds
)
{
    draw_panel(bounds);

    if (model->ln_route)
    {
        draw_section_label(
            "LN PROFILE / MSD",
            bounds.x + 15.0f,
            bounds.y + 13.0f
        );

        if (model->msd_ready && model->msd)
        {
            char overall[80];

            snprintf(
                overall,
                sizeof(overall),
                "MSD %.2f",
                model->msd->overall
            );

            const Vector2 overall_size =
                ui_measure_text(
                    UiFontBold(),
                    overall,
                    16.0f,
                    0.0f
                );

            ui_draw_text(
                UiFontBold(),
                overall,
                (Vector2)
                {
                    bounds.x +
                        bounds.width -
                        overall_size.x -
                        15.0f,
                    bounds.y + 12.0f
                },
                16.0f,
                0.0f,
                model->theme.accent
            );
        }

        if (
            !model->ln_profile_ready ||
            !model->ln_profile ||
            !model->ln_profile->valid
        )
        {
            ui_draw_text(
                UiFontRegular(),
                model->beatmap_ready
                    ? "Building LN profile..."
                    : "Waiting for beatmap...",
                (Vector2)
                {
                    bounds.x + 15.0f,
                    bounds.y + 45.0f
                },
                15.0f,
                0.0f,
                MUTED
            );

            return;
        }

        ui_draw_text(
            UiFontBold(),
            LnCourseFamilyName(
                model->ln_profile->family
            ),
            (Vector2)
            {
                bounds.x + 15.0f,
                bounds.y + 39.0f
            },
            18.0f,
            0.0f,
            model->theme.accent
        );

        char trait_a[80];
        char trait_b[80];
        char trait_c[80];

        LnPlayerProfileFormatTrait(
            &model->ln_profile->traits[0],
            trait_a,
            sizeof(trait_a)
        );

        LnPlayerProfileFormatTrait(
            &model->ln_profile->traits[1],
            trait_b,
            sizeof(trait_b)
        );

        LnPlayerProfileFormatTrait(
            &model->ln_profile->traits[2],
            trait_c,
            sizeof(trait_c)
        );

        char traits[280];

        snprintf(
            traits,
            sizeof(traits),
            "%s  |  %s  |  %s",
            trait_a,
            trait_b,
            trait_c
        );

        ui_draw_text(
            UiFontRegular(),
            traits,
            (Vector2)
            {
                bounds.x + 15.0f,
                bounds.y + 72.0f
            },
            13.0f,
            0.0f,
            TEXT
        );

        ui_draw_text(
            UiFontBold(),
            "HEADS",
            (Vector2)
            {
                bounds.x + 15.0f,
                bounds.y + 105.0f
            },
            11.0f,
            0.0f,
            MUTED
        );

        if (model->msd_ready && model->msd)
        {
            MinaCalcPatternSummary summary = {0};

            MinaCalcGetPatternSummary(
                model->msd,
                &summary
            );

            char heads[260];

            snprintf(
                heads,
                sizeof(heads),
                "%s %.2f  /  %s %.2f  /  %s %.2f",
                MinaCalcSkillsetName(summary.primary),
                summary.primary_score,
                MinaCalcSkillsetName(summary.secondary),
                summary.secondary_score,
                MinaCalcSkillsetName(summary.tertiary),
                summary.tertiary_score
            );

            ui_draw_text(
                UiFontRegular(),
                heads,
                (Vector2)
                {
                    bounds.x + 15.0f,
                    bounds.y + 123.0f
                },
                13.0f,
                0.0f,
                MUTED
            );
        }
        else
        {
            ui_draw_text(
                UiFontRegular(),
                "MinaCalc pending...",
                (Vector2)
                {
                    bounds.x + 15.0f,
                    bounds.y + 123.0f
                },
                13.0f,
                0.0f,
                MUTED
            );
        }

        return;
    }

    draw_section_label(
        "PATTERNS / MSD",
        bounds.x + 15.0f,
        bounds.y + 13.0f
    );

    if (!model->msd_ready || !model->msd)
    {
        ui_draw_text(
            UiFontRegular(),
            model->beatmap_ready
                ? "Calculating MinaCalc MSD..."
                : "Waiting for beatmap...",
            (Vector2)
            {
                bounds.x + 15.0f,
                bounds.y + 45.0f
            },
            15.0f,
            0.0f,
            MUTED
        );

        return;
    }

    char overall[112];

    snprintf(
        overall,
        sizeof(overall),
        "MSD %.2f  @  %.2fx",
        model->msd->overall,
        model->msd->rate
    );

    const Vector2 overall_size =
        ui_measure_text(
            UiFontBold(),
            overall,
            16.0f,
            0.0f
        );

    ui_draw_text(
        UiFontBold(),
        overall,
        (Vector2)
        {
            bounds.x +
                bounds.width -
                overall_size.x -
                15.0f,
            bounds.y + 12.0f
        },
        16.0f,
        0.0f,
        model->theme.accent
    );

    MinaCalcPatternSummary summary = {0};

    MinaCalcGetPatternSummary(
        model->msd,
        &summary
    );

    const double maximum =
        fmax(summary.primary_score, 0.01);

    const float row_x = bounds.x + 15.0f;
    const float row_width = bounds.width - 30.0f;
    const float row_height = 29.0f;

    draw_pattern_row(
        MinaCalcSkillsetName(summary.primary),
        summary.primary_score,
        maximum,
        (Rectangle)
        {
            row_x,
            bounds.y + 40.0f,
            row_width,
            row_height
        },
        model->theme,
        true
    );

    draw_pattern_row(
        MinaCalcSkillsetName(summary.secondary),
        summary.secondary_score,
        maximum,
        (Rectangle)
        {
            row_x,
            bounds.y + 76.0f,
            row_width,
            row_height
        },
        model->theme,
        false
    );

    draw_pattern_row(
        MinaCalcSkillsetName(summary.tertiary),
        summary.tertiary_score,
        maximum,
        (Rectangle)
        {
            row_x,
            bounds.y + 112.0f,
            row_width,
            row_height
        },
        model->theme,
        false
    );
}

static void draw_rank_panel(
    const AppViewModel *model,
    Rectangle bounds
)
{
    draw_panel(bounds);

    if (model->ln_route)
    {
        draw_section_label(
            "LN COURSE",
            bounds.x + 15.0f,
            bounds.y + 13.0f
        );

        if (
            !model->ln_course_ready ||
            !model->ln_course ||
            !model->ln_course->valid
        )
        {
            ui_draw_text(
                UiFontBold(),
                model->beatmap_ready
                    ? "Calculating LN course..."
                    : "Waiting for beatmap...",
                (Vector2)
                {
                    bounds.x + 15.0f,
                    bounds.y + 48.0f
                },
                18.0f,
                0.0f,
                TEXT
            );

            return;
        }

        draw_ln_badge(
            model->ln_course,
            model->theme,
            (Rectangle)
            {
                bounds.x + 15.0f,
                bounds.y + 37.0f,
                bounds.width - 30.0f,
                58.0f
            }
        );

        ui_draw_text(
            UiFontBold(),
            LnCourseSublevelName(
                model->ln_course->sublevel
            ),
            (Vector2)
            {
                bounds.x + 15.0f,
                bounds.y + 111.0f
            },
            17.0f,
            0.0f,
            model->theme.accent
        );

        char dp[72];

        snprintf(
            dp,
            sizeof(dp),
            "%.2f LN",
            model->ln_course->dp
        );

        const Vector2 dp_size =
            ui_measure_text(
                UiFontBold(),
                dp,
                20.0f,
                0.0f
            );

        ui_draw_text(
            UiFontBold(),
            dp,
            (Vector2)
            {
                bounds.x +
                    bounds.width -
                    dp_size.x -
                    15.0f,
                bounds.y + 108.0f
            },
            20.0f,
            0.0f,
            TEXT
        );

        return;
    }

    draw_section_label(
        "REFORM",
        bounds.x + 15.0f,
        bounds.y + 13.0f
    );

    if (!model->rank_ready || !model->rank)
    {
        ui_draw_text(
            UiFontBold(),
            model->beatmap_ready
                ? "Calculating rating..."
                : "Waiting for beatmap...",
            (Vector2)
            {
                bounds.x + 15.0f,
                bounds.y + 48.0f
            },
            18.0f,
            0.0f,
            TEXT
        );

        return;
    }

    draw_rank_badge(
        model->rank,
        model->theme,
        (Rectangle)
        {
            bounds.x + 15.0f,
            bounds.y + 37.0f,
            bounds.width - 30.0f,
            58.0f
        }
    );

    const char *sublevel =
        ReformSublevelName(
            model->rank->sublevel
        );

    ui_draw_text(
        UiFontBold(),
        sublevel,
        (Vector2)
        {
            bounds.x + 15.0f,
            bounds.y + 111.0f
        },
        17.0f,
        0.0f,
        model->theme.accent
    );

    char dp[64];

    snprintf(
        dp,
        sizeof(dp),
        "%.2f DP",
        model->rank->dp
    );

    const Vector2 dp_size =
        ui_measure_text(
            UiFontBold(),
            dp,
            20.0f,
            0.0f
        );

    ui_draw_text(
        UiFontBold(),
        dp,
        (Vector2)
        {
            bounds.x +
                bounds.width -
                dp_size.x -
                15.0f,
            bounds.y + 108.0f
        },
        20.0f,
        0.0f,
        TEXT
    );

    if (!model->sunny_current)
    {
        ui_draw_text(
            UiFontBold(),
            "updating",
            (Vector2)
            {
                bounds.x + 15.0f,
                bounds.y +
                    bounds.height -
                    19.0f
            },
            11.0f,
            0.0f,
            model->theme.accent
        );
    }
}


static double sample_visible_nps(
    const ChartFeatures *features,
    double time_ms,
    size_t *cursor
)
{
    if (
        !features ||
        !features->nps_curve ||
        features->nps_curve_count == 0
    )
        return 0.0;

    const size_t count =
        features->nps_curve_count;

    if (time_ms <= features->nps_curve[0].time_ms)
    {
        if (cursor)
            *cursor = 0;
        return features->nps_curve[0].nps;
    }

    if (time_ms >= features->nps_curve[count - 1].time_ms)
    {
        if (cursor)
            *cursor = count - 1;
        return features->nps_curve[count - 1].nps;
    }

    size_t i = cursor ? *cursor : 0;

    if (i >= count - 1)
        i = count - 2;

    while (
        i + 1 < count &&
        features->nps_curve[i + 1].time_ms < time_ms
    )
    {
        ++i;
    }

    while (
        i > 0 &&
        features->nps_curve[i].time_ms > time_ms
    )
    {
        --i;
    }

    if (cursor)
        *cursor = i;

    const NpsPoint *a =
        &features->nps_curve[i];

    const NpsPoint *b =
        &features->nps_curve[i + 1];

    const double denom =
        fmax(
            b->time_ms - a->time_ms,
            1.0
        );

    const double t =
        clampd(
            (time_ms - a->time_ms) / denom,
            0.0,
            1.0
        );

    return lerpd(
        a->nps,
        b->nps,
        t
    );
}


static double sample_overview_smoothed_nps(
    const ChartFeatures *features,
    double time_ms,
    double pixel_time_ms
)
{
    static const double weights[7] =
    {
        1.0,
        2.0,
        3.0,
        4.0,
        3.0,
        2.0,
        1.0
    };

    double weighted = 0.0;
    double total_weight = 0.0;

    for (int tap = -3; tap <= 3; ++tap)
    {
        const double sample_time =
            time_ms +
            (double)tap * pixel_time_ms;

        const double value =
            sample_visible_nps(
                features,
                sample_time,
                NULL
            );

        const double weight =
            weights[tap + 3];

        weighted += value * weight;
        total_weight += weight;
    }

    if (total_weight <= 0.0)
        return 0.0;

    return weighted / total_weight;
}


static double overview_smoothed_maximum(
    const ChartFeatures *features,
    double view_start,
    double view_span,
    float left,
    float right
)
{
    const double logical_width =
        fmax(
            (double)(right - left),
            1.0
        );

    const double physical_width =
        fmax(
            logical_width *
            (double)(
                g_ui_scale > 0.001f
                    ? g_ui_scale
                    : 1.0f
            ),
            1.0
        );

    const double pixel_time_ms =
        view_span / physical_width;

    const float step =
        physical_pixel_step();

    double maximum = 1.0;

    for (float x = left; x < right; x += step)
    {
        const float x_next =
            fminf(
                x + step,
                right
            );

        const double normalized =
            clampd(
                (
                    (double)(
                        x +
                        (x_next - x) * 0.5f
                    ) -
                    (double)left
                ) /
                logical_width,
                0.0,
                1.0
            );

        const double time_ms =
            view_start +
            normalized * view_span;

        const double nps =
            sample_overview_smoothed_nps(
                features,
                time_ms,
                pixel_time_ms
            );

        if (nps > maximum)
            maximum = nps;
    }

    return maximum * 1.035;
}

static void draw_overview_smoothed_area(
    const ChartFeatures *features,
    double view_start,
    double view_span,
    double current_time,
    double maximum,
    float left,
    float right,
    float top,
    float bottom,
    Color played_fill,
    Color future_fill,
    Color played_outline,
    Color future_outline
)
{
    if (
        !features ||
        !features->nps_curve ||
        features->nps_curve_count == 0 ||
        view_span <= 0.0 ||
        maximum <= 0.0
    )
        return;

    const double logical_width =
        fmax(
            (double)(right - left),
            1.0
        );

    const double physical_width =
        fmax(
            logical_width *
            (double)(
                g_ui_scale > 0.001f
                    ? g_ui_scale
                    : 1.0f
            ),
            1.0
        );

    const double pixel_time_ms =
        view_span / physical_width;

    const float step =
        physical_pixel_step();

    const float usable_height =
        bottom - top;

    const float outline_thickness =
        1.15f /
        (
            g_ui_scale > 0.001f
                ? g_ui_scale
                : 1.0f
        );

    bool have_previous = false;
    Vector2 previous = {0.0f, 0.0f};
    double previous_time = view_start;

    for (float x = left; x < right; x += step)
    {
        const float x_next =
            fminf(
                x + step,
                right
            );

        const float sample_x =
            x +
            (x_next - x) * 0.5f;

        const double normalized =
            clampd(
                (
                    (double)sample_x -
                    (double)left
                ) /
                logical_width,
                0.0,
                1.0
            );

        const double time_ms =
            view_start +
            normalized * view_span;

        const double nps =
            sample_overview_smoothed_nps(
                features,
                time_ms,
                pixel_time_ms
            );

        const float y =
            bottom -
            (float)(nps / maximum) *
            usable_height;

        const Color fill =
            time_ms <= current_time
                ? played_fill
                : future_fill;

        DrawRectangleRec(
            (Rectangle)
            {
                x,
                y,
                fmaxf(x_next - x, 0.001f),
                fmaxf(bottom - y, 0.0f)
            },
            fill
        );

        const Vector2 current =
        {
            sample_x,
            y
        };

        if (have_previous)
        {
            Color outline = future_outline;

            if (time_ms <= current_time)
            {
                outline = played_outline;
            }
            else if (
                previous_time < current_time &&
                time_ms > current_time
            )
            {
                outline = played_outline;
            }

            DrawLineEx(
                previous,
                current,
                outline_thickness,
                outline
            );
        }

        previous = current;
        previous_time = time_ms;
        have_previous = true;
    }
}

static void draw_density_fill_columns(
    const ChartFeatures *features,
    double view_start,
    double view_span,
    double current_time,
    double maximum,
    float left,
    float right,
    float top,
    float bottom,
    Color played_color,
    Color future_color
)
{
    if (
        !features ||
        !features->nps_curve ||
        features->nps_curve_count == 0 ||
        view_span <= 0.0 ||
        maximum <= 0.0
    )
        return;

    const double logical_width =
        fmax(
            (double)(right - left),
            1.0
        );

    const float step =
        physical_pixel_step();

    const float usable_height =
        bottom - top;

    size_t cursor = 0;

    for (float x = left; x < right; x += step)
    {
        const float x_next =
            fminf(
                x + step,
                right
            );

        const float sample_x =
            x +
            (x_next - x) * 0.5f;

        const double normalized =
            clampd(
                (
                    (double)sample_x -
                    (double)left
                ) /
                logical_width,
                0.0,
                1.0
            );

        const double time_ms =
            view_start +
            normalized * view_span;

        const double nps =
            sample_visible_nps(
                features,
                time_ms,
                &cursor
            );

        const float y =
            bottom -
            (float)(nps / maximum) *
            usable_height;

        const Color fill =
            time_ms <= current_time
                ? played_color
                : future_color;

        DrawRectangleRec(
            (Rectangle)
            {
                x,
                y,
                fmaxf(x_next - x, 0.001f),
                fmaxf(bottom - y, 0.0f)
            },
            fill
        );
    }
}

static void draw_outline_segment(
    double t0,
    double n0,
    double t1,
    double n1,
    double view_start,
    double view_span,
    double maximum,
    float left,
    float right,
    float top,
    float bottom,
    Color color,
    float thickness
)
{
    if (
        t1 <= t0 ||
        view_span <= 0.0 ||
        maximum <= 0.0
    )
        return;

    const float usable_width =
        right - left;

    const float usable_height =
        bottom - top;

    const float x0 =
        left +
        (float)((t0 - view_start) / view_span) *
        usable_width;

    const float x1 =
        left +
        (float)((t1 - view_start) / view_span) *
        usable_width;

    const float y0 =
        bottom -
        (float)(n0 / maximum) *
        usable_height;

    const float y1 =
        bottom -
        (float)(n1 / maximum) *
        usable_height;

    DrawLineEx(
        (Vector2){x0, y0},
        (Vector2){x1, y1},
        thickness,
        color
    );
}


static void draw_nps_graph(const AppViewModel *model, Rectangle bounds)
{
    const ChartFeatures *features = model->features;
    if (!features || !features->nps_curve || features->nps_curve_count == 0)
    {
        ui_draw_text(UiFontRegular(), "No density data", (Vector2){bounds.x + 18.0f, bounds.y + 48.0f}, 15.0f, 0.0f, MUTED);
        return;
    }

    double maximum = 1.0;
    for (size_t i = 0; i < features->nps_curve_count; ++i)
    {
        if (features->nps_curve[i].nps > maximum)
            maximum = features->nps_curve[i].nps;
    }

    const double first_time = features->nps_curve[0].time_ms;
    const double last_time = features->nps_curve[features->nps_curve_count - 1].time_ms;

    double current_time =
        visual_playback_time_ms(model->tosu);

    current_time =
        clampd(current_time, first_time, last_time);

    const float left = bounds.x + 18.0f;
    const float right = bounds.x + bounds.width - 18.0f;
    const float top = bounds.y + 42.0f;
    const float bottom = bounds.y + bounds.height - 20.0f;
    const float usable_height = bottom - top;

    for (int grid = 1; grid <= 4; ++grid)
    {
        const float y = top + usable_height * ((float)grid / 5.0f);
        DrawLine((int)left, (int)y, (int)right, (int)y, translucent(RULE, 120));
    }

    double view_start = first_time;
    double view_end = last_time;

    if (model->graph_mode == DENSITY_GRAPH_FOCUS)
    {
        const double playback_rate =
            model->tosu && model->tosu->rate > 0.01
                ? model->tosu->rate
                : 1.0;

        const double focus_span =
            fmax(
                (double)model->focus_span_seconds *
                1000.0 *
                playback_rate,
                5000.0
            );
        const double anchor = 0.35;
        const double before = focus_span * anchor;
        const double after = focus_span * (1.0 - anchor);

        view_start = current_time - before;
        view_end = current_time + after;

        if (view_start < first_time)
        {
            view_start = first_time;
            view_end = fmin(first_time + focus_span, last_time);
        }
        if (view_end > last_time)
        {
            view_end = last_time;
            view_start = fmax(last_time - focus_span, first_time);
        }
    }

    const double view_span = fmax(view_end - view_start, 1.0);
    const bool overview_mode =
        model->graph_mode == DENSITY_GRAPH_OVERVIEW;

    const Color past_color =
        model->theme.graph;

    const Color future_color =
        translucent(
            model->theme.graph,
            overview_mode ? 150 : 105
        );

    const Color past_fill =
        translucent(
            model->theme.graph,
            overview_mode ? 150 : 110
        );

    const Color future_fill =
        translucent(
            model->theme.graph,
            overview_mode ? 86 : 65
        );

    if (overview_mode)
    {
        maximum =
            overview_smoothed_maximum(
                features,
                view_start,
                view_span,
                left,
                right
            );

        draw_overview_smoothed_area(
            features,
            view_start,
            view_span,
            current_time,
            maximum,
            left,
            right,
            top,
            bottom,
            past_fill,
            future_fill,
            past_color,
            future_color
        );
    }
    else
    {
        draw_density_fill_columns(
            features,
            view_start,
            view_span,
            current_time,
            maximum,
            left,
            right,
            top,
            bottom,
            past_fill,
            future_fill
        );

        const float line_thickness =
            1.45f /
            (
                g_ui_scale > 0.001f
                    ? g_ui_scale
                    : 1.0f
            );

        for (size_t i = 1; i < features->nps_curve_count; ++i)
        {
            const NpsPoint *a = &features->nps_curve[i - 1];
            const NpsPoint *b = &features->nps_curve[i];

            if (b->time_ms < view_start)
                continue;
            if (a->time_ms > view_end)
                break;

            double t0 = a->time_ms;
            double t1 = b->time_ms;
            double n0 = a->nps;
            double n1 = b->nps;

            if (t0 < view_start)
            {
                const double u = (view_start - t0) / fmax(t1 - t0, 1.0);
                n0 = lerpd(n0, n1, u);
                t0 = view_start;
            }
            if (t1 > view_end)
            {
                const double u = (view_end - t0) / fmax(t1 - t0, 1.0);
                n1 = lerpd(n0, n1, u);
                t1 = view_end;
            }

            if (t1 <= current_time)
            {
                draw_outline_segment(t0, n0, t1, n1, view_start, view_span, maximum, left, right, top, bottom, past_color, line_thickness);
            }
            else if (t0 >= current_time)
            {
                draw_outline_segment(t0, n0, t1, n1, view_start, view_span, maximum, left, right, top, bottom, future_color, line_thickness);
            }
            else
            {
                const double u = (current_time - t0) / fmax(t1 - t0, 1.0);
                const double mid_n = lerpd(n0, n1, u);
                draw_outline_segment(t0, n0, current_time, mid_n, view_start, view_span, maximum, left, right, top, bottom, past_color, line_thickness);
                draw_outline_segment(current_time, mid_n, t1, n1, view_start, view_span, maximum, left, right, top, bottom, future_color, line_thickness);
            }
        }
    }

    const float current_x =
        left +
        (float)((current_time - view_start) / view_span) *
        (right - left);

    DrawLineEx(
        (Vector2){current_x, top},
        (Vector2){current_x, bottom},
        4.0f /
        (
            g_ui_scale > 0.001f
                ? g_ui_scale
                : 1.0f
        ),
        translucent(model->theme.accent, 50)
    );

    DrawLineEx(
        (Vector2){current_x, top},
        (Vector2){current_x, bottom},
        1.6f /
        (
            g_ui_scale > 0.001f
                ? g_ui_scale
                : 1.0f
        ),
        model->theme.accent
    );
}

static void draw_density_panel(const AppViewModel *model, Rectangle bounds)
{
    draw_panel(bounds);
    draw_section_label("DENSITY", bounds.x + 18.0f, bounds.y + 16.0f);

    if (!model->features_ready || !model->features)
    {
        ui_draw_text(
            UiFontRegular(),
            model->beatmap_ready ? "Building density profile..." : "Waiting for beatmap...",
            (Vector2){bounds.x + 18.0f, bounds.y + 50.0f},
            15.0f,
            0.0f,
            MUTED);
        return;
    }

    draw_nps_graph(model, bounds);
}

static void draw_header(const AppViewModel *model)
{
    const TosuSnapshot *tosu = model->tosu;
    if (!tosu)
        return;

    ui_draw_text(
        UiFontRegular(),
        tosu->artist[0] ? tosu->artist : "Unknown artist",
        (Vector2){22.0f, 15.0f},
        13.0f,
        0.0f,
        MUTED);

    ui_draw_text(
        UiFontBold(),
        tosu->title[0] ? tosu->title : "Unknown title",
        (Vector2){22.0f, 35.0f},
        23.0f,
        0.0f,
        TEXT);

    char difficulty[320];
    snprintf(difficulty, sizeof(difficulty), "[%s]", tosu->difficulty[0] ? tosu->difficulty : "Unknown difficulty");

    ui_draw_text(UiFontRegular(), difficulty, (Vector2){22.0f, 68.0f}, 14.0f, 0.0f, MUTED);

    char rate[96];
    snprintf(rate, sizeof(rate), "%s  %.2fx", tosu->mod[0] ? tosu->mod : "NM", tosu->rate);

    const Vector2 rate_size = ui_measure_text(UiFontBold(), rate, 18.0f, 0.0f);
    ui_draw_text(
        UiFontBold(),
        rate,
        (Vector2){(float)g_canvas_width - rate_size.x - 22.0f, 20.0f},
        18.0f,
        0.0f,
        model->rank_ready ? model->theme.accent : TEXT);

    char current[32];
    char total[32];
    format_time(tosu->time_ms, current, sizeof(current));
    format_time(tosu->length_ms, total, sizeof(total));

    char clock[80];
    snprintf(clock, sizeof(clock), "%s / %s", current, total);
    const Vector2 clock_size = ui_measure_text(UiFontRegular(), clock, 12.0f, 0.0f);
    ui_draw_text(
        UiFontRegular(),
        clock,
        (Vector2){(float)g_canvas_width - clock_size.x - 22.0f, 48.0f},
        12.0f,
        0.0f,
        MUTED);

    if (model->beatmap_ready && model->beatmap)
    {
        char map_info[160];
        snprintf(
            map_info,
            sizeof(map_info),
            "%dK  |  %zu objects  |  %.0f BPM",
            model->beatmap->keys,
            model->beatmap->note_count,
            model->beatmap->bpm);

        const Vector2 map_size = ui_measure_text(UiFontRegular(), map_info, 11.0f, 0.0f);
        ui_draw_text(
            UiFontRegular(),
            map_info,
            (Vector2){(float)g_canvas_width - map_size.x - 22.0f, 70.0f},
            11.0f,
            0.0f,
            MUTED);
    }

    DrawRectangle(22, 94, g_canvas_width - 44, 1, RULE);
}

static void draw_debug_panel(const AppViewModel *model, Rectangle bounds)
{
    draw_panel(bounds);
    draw_section_label("DIAGNOSTICS", bounds.x + 18.0f, bounds.y + 14.0f);

    const float mid = bounds.x + bounds.width * 0.5f;
    DrawLine((int)mid, (int)(bounds.y + 42.0f), (int)mid, (int)(bounds.y + bounds.height - 16.0f), RULE);

    const float left = bounds.x + 18.0f;
    const float right = mid + 18.0f;

    if (model->classification_ready)
    {
        char family[192];
        snprintf(
            family,
            sizeof(family),
            "family %s %.0f%%   ruler %s",
            ChartFamilyName(model->active_family),
            model->active_family_confidence * 100.0,
            ReformRulerName(model->selected_ruler));
        ui_draw_text(UiFontBold(), family, (Vector2){left, bounds.y + 48.0f}, 15.0f, 0.0f, model->theme.accent);
    }
    else
    {
        ui_draw_text(UiFontRegular(), "classifier pending", (Vector2){left, bounds.y + 48.0f}, 15.0f, 0.0f, MUTED);
    }

    if (model->sanity_ready && model->sanity)
    {
        char sanity[192];
        snprintf(sanity, sizeof(sanity), "sanity %s   requested %s", RulerSanityActionName(model->sanity->action), ReformRulerName(model->sanity->requested_ruler));
        ui_draw_text(UiFontRegular(), sanity, (Vector2){left, bounds.y + 76.0f}, 14.0f, 0.0f, MUTED);
    }

    if (model->sunny_ready && model->sunny)
    {
        char sunny_components[240];
        snprintf(
            sunny_components,
            sizeof(sunny_components),
            "Sunny SR %.4f  |  J %.2f  P %.2f  X %.2f  A %.2f",
            model->sunny->star_rating,
            model->sunny->jbar_max,
            model->sunny->pbar_max,
            model->sunny->xbar_max,
            model->sunny->abar_mean);
        ui_draw_text(UiFontRegular(), sunny_components, (Vector2){left, bounds.y + 104.0f}, 14.0f, 0.0f, TEXT);

        char sunny_shares[240];
        snprintf(
            sunny_shares,
            sizeof(sunny_shares),
            "shares  J %.2f  P %.2f  X %.2f   jack ratio %.2f",
            model->sunny->jbar_share,
            model->sunny->pbar_share,
            model->sunny->xbar_share,
            model->sunny->jack_ratio);
        ui_draw_text(UiFontRegular(), sunny_shares, (Vector2){left, bounds.y + 130.0f}, 13.0f, 0.0f, MUTED);
    }

    if (model->rhythm_ready && model->rhythm)
    {
        char rhythm[240];
        snprintf(
            rhythm,
            sizeof(rhythm),
            "rhythm %s/%s @ %.0f BPM   %.0f%%",
            model->rhythm->primary_rhythm,
            model->rhythm->subtype,
            model->rhythm->primary_bpm,
            model->rhythm->confidence * 100.0);
        ui_draw_text(UiFontRegular(), rhythm, (Vector2){left, bounds.y + 158.0f}, 13.0f, 0.0f, MUTED);
    }

    if (model->ln_route && model->ln_course_ready && model->ln_course)
    {
        char ln_debug[280];

        snprintf(
            ln_debug,
            sizeof(ln_debug),
            "LN %s  %.2f | %s | base %.2f | conf %.0f%%",
            LnCourseStageName(model->ln_course->stage),
            model->ln_course->dp,
            LnCourseFamilyName(model->ln_course->family),
            model->ln_course->base_sr_dp,
            model->ln_course->confidence * 100.0
        );

        ui_draw_text(
            UiFontRegular(),
            ln_debug,
            (Vector2){left, bounds.y + 184.0f},
            13.0f,
            0.0f,
            model->theme.accent
        );
    }

    if (model->features_ready && model->features)
    {
        char structure1[256];
        snprintf(structure1, sizeof(structure1), "jack %.2f  strict %.2f  vibro %.2f  anchor %.2f", model->features->jack_ratio, model->features->jack_density, model->features->vibro_density, model->features->anchor_ratio);
        ui_draw_text(UiFontRegular(), structure1, (Vector2){right, bounds.y + 48.0f}, 14.0f, 0.0f, TEXT);

        char structure2[256];
        snprintf(structure2, sizeof(structure2), "transition %.2f  timing %.2f  pattern %.2f", model->features->transition_var, model->features->timing_irregularity, model->features->pattern_irregularity);
        ui_draw_text(UiFontRegular(), structure2, (Vector2){right, bounds.y + 76.0f}, 13.0f, 0.0f, MUTED);

        char structure3[256];
        snprintf(structure3, sizeof(structure3), "NPS  %.1f / %.1f   stamina %.2f   burst %.2f", model->features->nps_p90, model->features->nps_p95, model->features->stamina_index, model->features->burst_ratio);
        ui_draw_text(UiFontRegular(), structure3, (Vector2){right, bounds.y + 104.0f}, 13.0f, 0.0f, MUTED);

        char structure4[256];
        snprintf(structure4, sizeof(structure4), "LN %.2f  occupancy %.2f  release %.2f/s", model->features->ln_ratio, model->features->hold_occupancy, model->features->release_density);
        ui_draw_text(UiFontRegular(), structure4, (Vector2){right, bounds.y + 132.0f}, 13.0f, 0.0f, MUTED);
    }

    if (
        model->ln_route &&
        model->ln_profile_ready &&
        model->ln_profile &&
        model->ln_profile->valid
    )
    {
        char trait_a[80];
        char trait_b[80];
        char trait_c[80];

        LnPlayerProfileFormatTrait(
            &model->ln_profile->traits[0],
            trait_a,
            sizeof(trait_a)
        );

        LnPlayerProfileFormatTrait(
            &model->ln_profile->traits[1],
            trait_b,
            sizeof(trait_b)
        );

        LnPlayerProfileFormatTrait(
            &model->ln_profile->traits[2],
            trait_c,
            sizeof(trait_c)
        );

        char profile[360];

        snprintf(
            profile,
            sizeof(profile),
            "LN profile  %s | %s | %s | %s",
            LnCourseFamilyName(
                model->ln_profile->family
            ),
            trait_a,
            trait_b,
            trait_c
        );

        ui_draw_text(
            UiFontRegular(),
            profile,
            (Vector2)
            {
                left,
                bounds.y + 186.0f
            },
            12.0f,
            0.0f,
            MUTED
        );
    }

    if (model->msd_ready && model->msd)
    {
        char msd1[320];
        snprintf(msd1, sizeof(msd1), "MSD  S %.2f  JS %.2f  HS %.2f  St %.2f", model->msd->stream, model->msd->jumpstream, model->msd->handstream, model->msd->stamina);
        ui_draw_text(UiFontRegular(), msd1, (Vector2){right, bounds.y + 162.0f}, 13.0f, 0.0f, TEXT);

        char msd2[240];
        snprintf(msd2, sizeof(msd2), "Jk %.2f  CJ %.2f  Tech %.2f", model->msd->jackspeed, model->msd->chordjack, model->msd->technical);
        ui_draw_text(UiFontRegular(), msd2, (Vector2){right, bounds.y + 188.0f}, 13.0f, 0.0f, MUTED);
    }
}

static void draw_footer(
    const AppViewModel *model
)
{
    const float y =
        (float)g_canvas_height - 22.0f;

    char controls[180];

    if (model->graph_mode == DENSITY_GRAPH_FOCUS)
    {
        snprintf(
            controls,
            sizeof(controls),
            "F2  settings   G  focus   - / +  %ds   F3  %s",
            model->focus_span_seconds,
            model->debug_visible
                ? "hide diagnostics"
                : "diagnostics"
        );
    }
    else
    {
        snprintf(
            controls,
            sizeof(controls),
            "F2  settings   G  overview   F3  %s",
            model->debug_visible
                ? "hide diagnostics"
                : "diagnostics"
        );
    }

    ui_draw_text(
        UiFontRegular(),
        controls,
        (Vector2){22.0f, y},
        11.0f,
        0.0f,
        MUTED
    );

    if (!model->tosu)
        return;

    char status[128];
    snprintf(
        status,
        sizeof(status),
        "tosu  |  %s",
        model->tosu->client[0]
            ? model->tosu->client
            : "lazer"
    );

    const Vector2 size =
        ui_measure_text(UiFontRegular(), status, 11.0f, 0.0f);

    ui_draw_text(
        UiFontRegular(),
        status,
        (Vector2){
            (float)g_canvas_width - size.x - 22.0f,
            y
        },
        11.0f,
        0.0f,
        GOOD
    );
}

static void draw_disconnected(const AppViewModel *model)
{
    const float width = fmin((float)g_canvas_width - 48.0f, 460.0f);
    const Rectangle panel = {((float)g_canvas_width - width) * 0.5f, ((float)g_canvas_height - 160.0f) * 0.5f, width, 160.0f};

    draw_panel(panel);

    ui_draw_text(UiFontBold(), "TOSU OFFLINE", (Vector2){panel.x + 22.0f, panel.y + 28.0f}, 23.0f, 0.0f, BAD);
    ui_draw_text(UiFontRegular(), "Waiting for osu!lazer telemetry...", (Vector2){panel.x + 22.0f, panel.y + 72.0f}, 16.0f, 0.0f, TEXT);
    ui_draw_text(UiFontRegular(), "The last valid state is preserved across brief poll drops.", (Vector2){panel.x + 22.0f, panel.y + 104.0f}, 13.0f, 0.0f, MUTED);

    (void)model;
}

static void draw_hud_mixed_line(
    const char *prefix,
    const char *value,
    float x,
    float y,
    float nominal_size,
    float minimum_physical_size,
    float maximum_width,
    Color value_color)
{
    char combined[320];

    snprintf(
        combined,
        sizeof(combined),
        "%s%s",
        prefix ? prefix : "",
        value ? value : ""
    );

    const float font_size =
        hud_fit_font_size(
            UiFontBold(),
            combined,
            nominal_size,
            minimum_physical_size,
            maximum_width
        );

    DrawTextEx(
        UiFontBold(),
        prefix ? prefix : "",
        (Vector2){x, y},
        font_size,
        0.0f,
        TEXT
    );

    const Vector2 prefix_size =
        MeasureTextEx(
            UiFontBold(),
            prefix ? prefix : "",
            font_size,
            0.0f
        );

    DrawTextEx(
        UiFontBold(),
        value ? value : "",
        (Vector2){x + prefix_size.x, y},
        font_size,
        0.0f,
        value_color
    );
}

static void draw_hud_density_panel(
    const AppViewModel *model,
    Rectangle bounds)
{
    draw_panel(bounds);

    draw_section_label(
        "DENSITY",
        bounds.x + 14.0f,
        bounds.y + 12.0f
    );

    if (!model->features_ready || !model->features)
    {
        hud_draw_text(
            UiFontBold(),
            model->beatmap_ready
                ? "Building density profile..."
                : "Waiting for beatmap...",
            (Vector2){bounds.x + 16.0f, bounds.y + 52.0f},
            20.0f,
            14.0f,
            MUTED
        );

        return;
    }

    draw_nps_graph(model, bounds);
}

static void draw_hud_rank_line(
    const AppViewModel *model,
    Rectangle info_bounds,
    float y)
{
    const float left = info_bounds.x + 16.0f;
    const float maximum_width = info_bounds.width - 32.0f;

    if (model->ln_route)
    {
        if (
            !model->ln_course_ready ||
            !model->ln_course ||
            !model->ln_course->valid
        )
        {
            hud_draw_text(
                UiFontBold(),
                model->beatmap_ready
                    ? "Calculating LN Course..."
                    : "Waiting for beatmap...",
                (Vector2){left, y},
                38.0f,
                22.0f,
                TEXT
            );
            return;
        }

        char result[160];
        snprintf(
            result,
            sizeof(result),
            "%s %s (%.1f)",
            LnCourseStageName(model->ln_course->stage),
            LnCourseSublevelName(model->ln_course->sublevel),
            model->ln_course->dp
        );

        draw_hud_mixed_line(
            "Est. LN: ",
            result,
            left,
            y,
            39.0f,
            22.0f,
            maximum_width,
            model->theme.accent
        );
        return;
    }

    if (!model->rank_ready || !model->rank)
    {
        hud_draw_text(
            UiFontBold(),
            model->beatmap_ready
                ? "Calculating Dan rating..."
                : "Waiting for beatmap...",
            (Vector2){left, y},
            38.0f,
            22.0f,
            TEXT
        );
        return;
    }

    char result[160];
    snprintf(
        result,
        sizeof(result),
        "%s %s (%.1f)",
        ReformTierName(model->rank->tier_index),
        ReformSublevelName(model->rank->sublevel),
        model->rank->dp
    );

    draw_hud_mixed_line(
        "Est. Dan: ",
        result,
        left,
        y,
        39.0f,
        22.0f,
        maximum_width,
        model->theme.accent
    );
}

static void draw_hud_rice_patterns(
    const AppViewModel *model,
    Rectangle info_bounds,
    float y)
{
    const float left = info_bounds.x + 16.0f;
    const float maximum_width = info_bounds.width - 32.0f;

    if (!model->msd_ready || !model->msd)
    {
        hud_draw_text(
            UiFontBold(),
            "MinaCalc MSD pending...",
            (Vector2){left, y},
            27.0f,
            17.0f,
            MUTED
        );
        return;
    }

    MinaCalcPatternSummary summary = {0};
    MinaCalcGetPatternSummary(model->msd, &summary);

    char patterns[200];

    if (summary.has_secondary)
    {
        snprintf(
            patterns,
            sizeof(patterns),
            "%s, %s",
            MinaCalcSkillsetName(summary.primary),
            MinaCalcSkillsetName(summary.secondary)
        );
    }
    else
    {
        snprintf(
            patterns,
            sizeof(patterns),
            "%s",
            MinaCalcSkillsetName(summary.primary)
        );
    }

    char score[80];
    snprintf(
        score,
        sizeof(score),
        "  %.2f MSD",
        model->msd->overall
    );

    char combined[300];
    snprintf(
        combined,
        sizeof(combined),
        "%s%s",
        patterns,
        score
    );

    const float font_size =
        hud_fit_font_size(
            UiFontBold(),
            combined,
            28.0f,
            17.0f,
            maximum_width
        );

    DrawTextEx(
        UiFontBold(),
        patterns,
        (Vector2){left, y},
        font_size,
        0.0f,
        TEXT
    );

    const Vector2 pattern_size =
        MeasureTextEx(
            UiFontBold(),
            patterns,
            font_size,
            0.0f
        );

    DrawTextEx(
        UiFontBold(),
        score,
        (Vector2){left + pattern_size.x, y},
        font_size,
        0.0f,
        model->theme.accent
    );
}

static void draw_hud_ln_profile(
    const AppViewModel *model,
    Rectangle info_bounds,
    float family_y,
    float patterns_y)
{
    const float left = info_bounds.x + 16.0f;
    const float maximum_width = info_bounds.width - 32.0f;

    if (
        model->ln_profile_ready &&
        model->ln_profile &&
        model->ln_profile->valid
    )
    {
        const char *family =
            LnCourseFamilyName(model->ln_profile->family);

        const float family_size =
            hud_fit_font_size(
                UiFontBold(),
                family,
                28.0f,
                17.0f,
                maximum_width
            );

        DrawTextEx(
            UiFontBold(),
            family,
            (Vector2){left, family_y},
            family_size,
            0.0f,
            TEXT
        );
    }
    else
    {
        hud_draw_text(
            UiFontBold(),
            "Building LN profile...",
            (Vector2){left, family_y},
            27.0f,
            17.0f,
            MUTED
        );
    }

    if (!model->msd_ready || !model->msd)
        return;

    MinaCalcPatternSummary summary = {0};
    MinaCalcGetPatternSummary(model->msd, &summary);

    char heads[260];

    if (summary.has_tertiary)
    {
        snprintf(
            heads,
            sizeof(heads),
            "%s, %s, %s",
            MinaCalcSkillsetName(summary.primary),
            MinaCalcSkillsetName(summary.secondary),
            MinaCalcSkillsetName(summary.tertiary)
        );
    }
    else if (summary.has_secondary)
    {
        snprintf(
            heads,
            sizeof(heads),
            "%s, %s",
            MinaCalcSkillsetName(summary.primary),
            MinaCalcSkillsetName(summary.secondary)
        );
    }
    else
    {
        snprintf(
            heads,
            sizeof(heads),
            "%s",
            MinaCalcSkillsetName(summary.primary)
        );
    }

    char score[80];
    snprintf(
        score,
        sizeof(score),
        "  %.2f MSD",
        model->msd->overall
    );

    char combined[360];
    snprintf(
        combined,
        sizeof(combined),
        "%s%s",
        heads,
        score
    );

    const float font_size =
        hud_fit_font_size(
            UiFontBold(),
            combined,
            22.0f,
            13.5f,
            maximum_width
        );

    DrawTextEx(
        UiFontBold(),
        heads,
        (Vector2){left, patterns_y},
        font_size,
        0.0f,
        MUTED
    );

    const Vector2 heads_size =
        MeasureTextEx(
            UiFontBold(),
            heads,
            font_size,
            0.0f
        );

    DrawTextEx(
        UiFontBold(),
        score,
        (Vector2){left + heads_size.x, patterns_y},
        font_size,
        0.0f,
        model->theme.accent
    );
}

static void draw_hud_footer(
    const AppViewModel *model,
    Rectangle info_bounds)
{
    if (!model || !model->tosu)
        return;

    char footer[260];

    snprintf(
        footer,
        sizeof(footer),
        "%s %.2fx  |  4K  |  %s%s  |  F2 SETTINGS",
        model->tosu->mod[0]
            ? model->tosu->mod
            : "NM",
        model->tosu->rate,
        model->graph_mode == DENSITY_GRAPH_FOCUS
            ? "FOCUS "
            : "OVERVIEW",
        model->graph_mode == DENSITY_GRAPH_FOCUS
            ? TextFormat("%ds", model->focus_span_seconds)
            : ""
    );

    const float y =
        info_bounds.y +
        info_bounds.height -
        hud_font_size(15.0f, 10.5f) -
        10.0f;

    const float size =
        hud_fit_font_size(
            UiFontBold(),
            footer,
            15.0f,
            10.5f,
            info_bounds.width - 32.0f
        );

    DrawTextEx(
        UiFontBold(),
        footer,
        (Vector2){info_bounds.x + 16.0f, y},
        size,
        0.0f,
        MUTED
    );
}

static void draw_hud_player(
    const AppViewModel *model)
{
    const float margin = 8.0f;
    const float gap = 8.0f;

    const float player_height =
        model->debug_visible
            ? fmaxf(560.0f, (float)g_canvas_height * 0.66f)
            : (float)g_canvas_height;

    const float graph_height =
        fmaxf(210.0f, player_height * 0.60f);

    const Rectangle density =
    {
        margin,
        margin,
        (float)g_canvas_width - margin * 2.0f,
        graph_height - margin
    };

    draw_hud_density_panel(model, density);

    const Rectangle info =
    {
        margin,
        density.y + density.height + gap,
        (float)g_canvas_width - margin * 2.0f,
        player_height -
            (density.y + density.height + gap) -
            margin
    };

    DrawRectangleRounded(info, 0.02f, 8, PANEL);
    DrawRectangleRoundedLinesEx(info, 0.02f, 8, 1.0f, RULE);

    DrawRectangle(
        (int)(info.x + 1.0f),
        (int)info.y,
        (int)(info.width - 2.0f),
        2,
        translucent(model->theme.accent, 190)
    );

    const float rank_y = info.y + 12.0f;
    draw_hud_rank_line(model, info, rank_y);

    const float scale =
        g_ui_scale > 0.001f
            ? g_ui_scale
            : 1.0f;

    const float rank_line_height =
        fmaxf(
            38.0f,
            24.0f / scale
        );

    if (model->ln_route)
    {
        const float family_y =
            rank_y + rank_line_height + 7.0f;

        const float patterns_y =
            family_y +
            fmaxf(28.0f, 18.0f / scale) +
            5.0f;

        draw_hud_ln_profile(
            model,
            info,
            family_y,
            patterns_y
        );
    }
    else
    {
        const float patterns_y =
            rank_y + rank_line_height + 8.0f;

        draw_hud_rice_patterns(
            model,
            info,
            patterns_y
        );
    }

    draw_hud_footer(model, info);

    if (model->debug_visible)
    {
        const Rectangle debug_panel =
        {
            margin,
            player_height + gap,
            (float)g_canvas_width - margin * 2.0f,
            (float)g_canvas_height - player_height - gap - 28.0f
        };

        if (debug_panel.height > 120.0f)
            draw_debug_panel(model, debug_panel);
    }
}

static void draw_common_player_layout(
    const AppViewModel *model,
    float margin,
    float cards_y,
    bool simple_view)
{
    const float gap = 12.0f;
    const float usable =
        (float)g_canvas_width -
        margin * 2.0f;

    float rank_width =
        usable * 0.31f;

    if (rank_width < 250.0f)
        rank_width = 250.0f;

    const float cards_height = 150.0f;

    const Rectangle rank_panel =
    {
        margin,
        cards_y,
        rank_width,
        cards_height
    };

    const Rectangle pattern_panel =
    {
        margin + rank_width + gap,
        cards_y,
        usable - rank_width - gap,
        cards_height
    };

    draw_rank_panel(model, rank_panel);
    (void)simple_view;
    draw_pattern_panel(model, pattern_panel);

    const float density_y =
        cards_y +
        cards_height +
        12.0f;

    const float footer_reserved = 34.0f;
    const float available =
        (float)g_canvas_height -
        density_y -
        footer_reserved;

    Rectangle density_panel =
    {
        margin,
        density_y,
        usable,
        available
    };

    if (model->debug_visible)
    {
        density_panel.height =
            fmax(
                250.0f,
                available * 0.52f
            );
    }

    draw_density_panel(model, density_panel);

    if (model->debug_visible)
    {
        const Rectangle debug_panel =
        {
            margin,
            density_panel.y +
                density_panel.height +
                gap,
            usable,
            (float)g_canvas_height -
                (density_panel.y +
                 density_panel.height +
                 gap) -
                34.0f
        };

        draw_debug_panel(model, debug_panel);
    }
}

static void draw_extra_info_player(
    const AppViewModel *model)
{
    draw_header(model);

    draw_common_player_layout(
        model,
        22.0f,
        108.0f,
        false
    );
}

void AppViewDraw(
    const AppViewModel *model
)
{
    if (!model)
        return;

    const Camera2D camera =
        make_ui_camera(model->debug_visible);

    BeginDrawing();
    ClearBackground(BG);
    BeginMode2D(camera);

    if (!model->tosu || !model->tosu->connected)
    {
        draw_disconnected(model);
        draw_footer(model);

        EndMode2D();

        if (
            model->settings_visible &&
            model->settings
        )
        {
            SettingsPanelDraw(
                model->settings,
                model->theme,
                GetMousePosition(),
                (float)GetScreenWidth(),
                (float)GetScreenHeight(),
                1.0f
            );
        }

        EndDrawing();
        return;
    }

    if (model->view_mode == APP_SETTINGS_VIEW_EXTRA_INFO)
    {
        draw_extra_info_player(model);
        draw_footer(model);
    }
    else
    {
        draw_hud_player(model);
    }

    EndMode2D();

    if (
        model->settings_visible &&
        model->settings
    )
    {
        SettingsPanelDraw(
            model->settings,
            model->theme,
            GetMousePosition(),
            (float)GetScreenWidth(),
            (float)GetScreenHeight(),
            1.0f
        );
    }

    EndDrawing();
}
