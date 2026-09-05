#include "settings_panel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "ui/font.h"

static const Color PANEL = {29, 32, 34, 255};
static const Color PANEL_INSET = {25, 28, 30, 255};
static const Color TEXT = {232, 229, 223, 255};
static const Color MUTED = {140, 140, 135, 255};
static const Color RULE = {52, 54, 56, 255};

static Vector2 g_mouse = {0.0f, 0.0f};
static Rectangle g_interaction_clip = {0};
static bool g_use_interaction_clip = false;
static float g_scroll = 0.0f;

static bool raw_point_in_rect(Vector2 point, Rectangle bounds)
{
    return
        point.x >= bounds.x &&
        point.x <= bounds.x + bounds.width &&
        point.y >= bounds.y &&
        point.y <= bounds.y + bounds.height;
}

static bool point_in_rect(Vector2 point, Rectangle bounds)
{
    if (!raw_point_in_rect(point, bounds))
        return false;

    if (
        g_use_interaction_clip &&
        !raw_point_in_rect(point, g_interaction_clip)
    )
    {
        return false;
    }

    return true;
}

static Color with_alpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

static void draw_label(
    const char *text,
    float x,
    float y,
    float size,
    Color color,
    bool bold)
{
    DrawTextEx(
        bold ? UiFontBold() : UiFontRegular(),
        text,
        (Vector2){x, y},
        size,
        0.0f,
        color
    );
}

static bool draw_button(
    Rectangle bounds,
    const char *text,
    bool active,
    DanTheme theme)
{
    const bool hovered = point_in_rect(g_mouse, bounds);

    Color background = PANEL_INSET;
    Color border = RULE;
    Color foreground = TEXT;

    if (active)
    {
        background = with_alpha(theme.accent, 42);
        border = with_alpha(theme.accent, 190);
        foreground = theme.accent;
    }
    else if (hovered)
    {
        background = (Color){36, 39, 41, 255};
        border = (Color){76, 78, 80, 255};
    }

    DrawRectangleRounded(bounds, 0.12f, 6, background);
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 6, 1.0f, border);

    const Font font = active ? UiFontBold() : UiFontRegular();
    const float font_size = 13.0f;
    const Vector2 size = MeasureTextEx(font, text, font_size, 0.0f);

    DrawTextEx(
        font,
        text,
        (Vector2)
        {
            bounds.x + (bounds.width - size.x) * 0.5f,
            bounds.y + (bounds.height - size.y) * 0.5f - 1.0f
        },
        font_size,
        0.0f,
        foreground
    );

    return
        hovered &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static bool draw_toggle_row(
    Rectangle bounds,
    const char *label,
    const char *description,
    bool *value,
    DanTheme theme)
{
    if (!value)
        return false;

    const bool hovered = point_in_rect(g_mouse, bounds);

    if (hovered)
    {
        DrawRectangleRounded(
            bounds,
            0.08f,
            6,
            (Color){34, 37, 39, 255}
        );
    }

    draw_label(
        label,
        bounds.x + 8.0f,
        bounds.y + 6.0f,
        14.0f,
        TEXT,
        true
    );

    draw_label(
        description,
        bounds.x + 8.0f,
        bounds.y + 27.0f,
        11.5f,
        MUTED,
        false
    );

    const Rectangle switch_track =
    {
        bounds.x + bounds.width - 50.0f,
        bounds.y + 14.0f,
        38.0f,
        20.0f
    };

    DrawRectangleRounded(
        switch_track,
        0.5f,
        8,
        *value
            ? with_alpha(theme.accent, 70)
            : (Color){45, 48, 50, 255}
    );

    DrawRectangleRoundedLinesEx(
        switch_track,
        0.5f,
        8,
        1.0f,
        *value
            ? with_alpha(theme.accent, 190)
            : RULE
    );

    const float knob_x =
        *value
            ? switch_track.x + 21.0f
            : switch_track.x + 3.0f;

    DrawCircle(
        (int)(knob_x + 7.0f),
        (int)(switch_track.y + 10.0f),
        7.0f,
        *value ? theme.accent : MUTED
    );

    if (
        hovered &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    )
    {
        *value = !*value;
        return true;
    }

    return false;
}

static bool size_matches_percent(
    const AppSettings *settings,
    int percent)
{
    if (!settings)
        return false;

    const int expected_width =
        APP_SETTINGS_DESIGN_WIDTH * percent / 100;
    const int expected_height =
        APP_SETTINGS_DESIGN_HEIGHT * percent / 100;

    return
        abs(settings->window_width - expected_width) <= 3 &&
        abs(settings->window_height - expected_height) <= 3;
}

static void apply_size_percent(
    AppSettings *settings,
    int percent)
{
    if (!settings)
        return;

    settings->window_width =
        APP_SETTINGS_DESIGN_WIDTH * percent / 100;
    settings->window_height =
        APP_SETTINGS_DESIGN_HEIGHT * percent / 100;

    settings->remember_window_size = true;
}

static void draw_scrollbar(
    Rectangle viewport,
    float content_height,
    float scroll)
{
    if (content_height <= viewport.height + 0.5f)
        return;

    const float track_x =
        viewport.x + viewport.width - 3.0f;

    const float thumb_height =
        fmaxf(
            26.0f,
            viewport.height * viewport.height / content_height
        );

    const float max_scroll =
        content_height - viewport.height;

    const float travel =
        viewport.height - thumb_height;

    const float thumb_y =
        viewport.y +
        travel * (max_scroll > 0.0f ? scroll / max_scroll : 0.0f);

    DrawRectangle(
        (int)track_x,
        (int)viewport.y,
        2,
        (int)viewport.height,
        with_alpha(RULE, 100)
    );

    DrawRectangleRounded(
        (Rectangle)
        {
            track_x - 1.0f,
            thumb_y,
            4.0f,
            thumb_height
        },
        0.5f,
        4,
        (Color){105, 107, 108, 190}
    );
}

bool SettingsPanelDraw(
    AppSettings *settings,
    DanTheme theme,
    Vector2 mouse_position,
    float canvas_width,
    float canvas_height,
    float ui_scale)
{
    (void)ui_scale;

    if (!settings)
        return false;

    g_mouse = mouse_position;

    const float screen_width = fmaxf(canvas_width, 1.0f);
    const float screen_height = fmaxf(canvas_height, 1.0f);

    bool changed = false;

    DrawRectangle(
        0,
        0,
        (int)screen_width,
        (int)screen_height,
        (Color){0, 0, 0, 176}
    );

    float panel_width = fminf(680.0f, screen_width - 16.0f);
    float panel_height = fminf(640.0f, screen_height - 16.0f);

    if (panel_width < 320.0f)
        panel_width = fmaxf(screen_width - 4.0f, 1.0f);

    if (panel_height < 210.0f)
        panel_height = fmaxf(screen_height - 4.0f, 1.0f);

    const Rectangle panel =
    {
        (screen_width - panel_width) * 0.5f,
        (screen_height - panel_height) * 0.5f,
        panel_width,
        panel_height
    };

    DrawRectangleRounded(panel, 0.025f, 10, PANEL);
    DrawRectangleRoundedLinesEx(panel, 0.025f, 10, 1.0f, RULE);

    const float left = panel.x + 16.0f;
    const float right = panel.x + panel.width - 16.0f;

    draw_label(
        "SETTINGS",
        left,
        panel.y + 12.0f,
        19.0f,
        TEXT,
        true
    );

    draw_label(
        "Saved automatically",
        left,
        panel.y + 36.0f,
        11.5f,
        MUTED,
        false
    );

    const float header_bottom = panel.y + 56.0f;
    const float footer_height = 43.0f;
    const float footer_top = panel.y + panel.height - footer_height;

    DrawLine(
        (int)left,
        (int)header_bottom,
        (int)right,
        (int)header_bottom,
        RULE
    );

    const Rectangle viewport =
    {
        left,
        header_bottom + 7.0f,
        right - left,
        fmaxf(footer_top - header_bottom - 14.0f, 1.0f)
    };

    const float content_height = 640.0f;
    const float max_scroll =
        fmaxf(content_height - viewport.height, 0.0f);

    if (raw_point_in_rect(g_mouse, viewport))
    {
        g_scroll -= GetMouseWheelMove() * 44.0f;
    }

    if (g_scroll < 0.0f)
        g_scroll = 0.0f;
    if (g_scroll > max_scroll)
        g_scroll = max_scroll;

    BeginScissorMode(
        (int)viewport.x,
        (int)viewport.y,
        (int)viewport.width,
        (int)viewport.height
    );

    g_use_interaction_clip = true;
    g_interaction_clip = viewport;

    float y = viewport.y - g_scroll;
    const float content_width = viewport.width - 8.0f;

    draw_label("VIEW", left, y, 12.0f, MUTED, true);
    y += 22.0f;

    const float view_gap = 8.0f;
    const float view_width =
        (content_width - view_gap) * 0.5f;

    if (
        draw_button(
            (Rectangle){left, y, view_width, 34.0f},
            "HUD",
            settings->view_mode == APP_SETTINGS_VIEW_HUD,
            theme
        )
    )
    {
        settings->view_mode = APP_SETTINGS_VIEW_HUD;
        changed = true;
    }

    if (
        draw_button(
            (Rectangle)
            {
                left + view_width + view_gap,
                y,
                view_width,
                34.0f
            },
            "EXTRA INFO",
            settings->view_mode == APP_SETTINGS_VIEW_EXTRA_INFO,
            theme
        )
    )
    {
        settings->view_mode = APP_SETTINGS_VIEW_EXTRA_INFO;
        changed = true;
    }

    y += 42.0f;

    draw_label(
        settings->view_mode == APP_SETTINGS_VIEW_HUD
            ? "HUD: big Dan/LN text, MSD patterns and density. No song metadata."
            : "Extra Info: song metadata, map details, rating, patterns and density.",
        left,
        y,
        11.5f,
        MUTED,
        false
    );

    y += 31.0f;
    DrawLine((int)left, (int)y, (int)(left + content_width), (int)y, RULE);
    y += 14.0f;

    draw_label("GRAPH", left, y, 12.0f, MUTED, true);
    y += 22.0f;

    const float mode_gap = 8.0f;
    const float mode_width =
        (content_width - mode_gap) * 0.5f;

    if (
        draw_button(
            (Rectangle){left, y, mode_width, 34.0f},
            "OVERVIEW",
            settings->graph_mode == APP_SETTINGS_GRAPH_OVERVIEW,
            theme
        )
    )
    {
        settings->graph_mode = APP_SETTINGS_GRAPH_OVERVIEW;
        changed = true;
    }

    if (
        draw_button(
            (Rectangle)
            {
                left + mode_width + mode_gap,
                y,
                mode_width,
                34.0f
            },
            "FOCUS",
            settings->graph_mode == APP_SETTINGS_GRAPH_FOCUS,
            theme
        )
    )
    {
        settings->graph_mode = APP_SETTINGS_GRAPH_FOCUS;
        changed = true;
    }

    y += 43.0f;
    draw_label("Focus window", left, y, 12.0f, MUTED, false);
    y += 20.0f;

    static const int spans[] =
    {
        15, 30, 45, 60, 90, 120, 180
    };

    const int span_columns =
        content_width < 430.0f ? 4 : 7;
    const int span_rows =
        (7 + span_columns - 1) / span_columns;
    const float span_gap = 5.0f;
    const float span_width =
        (content_width - span_gap * (float)(span_columns - 1)) /
        (float)span_columns;

    for (int i = 0; i < 7; ++i)
    {
        const int row = i / span_columns;
        const int column = i % span_columns;

        char label[16];
        snprintf(label, sizeof(label), "%ds", spans[i]);

        if (
            draw_button(
                (Rectangle)
                {
                    left + (span_width + span_gap) * (float)column,
                    y + (31.0f + span_gap) * (float)row,
                    span_width,
                    31.0f
                },
                label,
                settings->focus_span_seconds == spans[i],
                theme
            )
        )
        {
            settings->focus_span_seconds = spans[i];
            settings->graph_mode = APP_SETTINGS_GRAPH_FOCUS;
            changed = true;
        }
    }

    y +=
        (31.0f + span_gap) * (float)span_rows +
        9.0f;

    DrawLine((int)left, (int)y, (int)(left + content_width), (int)y, RULE);
    y += 14.0f;

    draw_label("WINDOW", left, y, 12.0f, MUTED, true);
    y += 22.0f;
    draw_label("3:2 size presets", left, y, 12.0f, MUTED, false);
    y += 20.0f;

    static const int scales[] =
    {
        40, 50, 60, 75, 100
    };

    const float scale_gap = 5.0f;
    const float scale_width =
        (content_width - scale_gap * 4.0f) / 5.0f;

    for (int i = 0; i < 5; ++i)
    {
        char label[16];
        snprintf(label, sizeof(label), "%d%%", scales[i]);

        if (
            draw_button(
                (Rectangle)
                {
                    left + (scale_width + scale_gap) * (float)i,
                    y,
                    scale_width,
                    31.0f
                },
                label,
                size_matches_percent(settings, scales[i]),
                theme
            )
        )
        {
            apply_size_percent(settings, scales[i]);
            changed = true;
        }
    }

    y += 39.0f;

    draw_label(
        "Drag any edge/corner freely. HUD and Extra Info use the full window shape.",
        left,
        y,
        11.5f,
        MUTED,
        false
    );

    y += 24.0f;

    changed |=
        draw_toggle_row(
            (Rectangle){left, y, content_width, 50.0f},
            "Always on top",
            "Keep ManiaDanOverlay above normal windows.",
            &settings->always_on_top,
            theme
        );

    y += 53.0f;

    changed |=
        draw_toggle_row(
            (Rectangle){left, y, content_width, 50.0f},
            "Remember position",
            "Restore the normal on-screen window position.",
            &settings->remember_window_position,
            theme
        );

    y += 53.0f;

    changed |=
        draw_toggle_row(
            (Rectangle){left, y, content_width, 50.0f},
            "Remember size",
            "Restore your resized overlay window next launch.",
            &settings->remember_window_size,
            theme
        );

    y += 60.0f;

    draw_label(
        "Tip: HUD is designed to remain readable at compact overlay sizes.",
        left,
        y,
        11.5f,
        theme.accent,
        false
    );

    g_use_interaction_clip = false;
    EndScissorMode();

    draw_scrollbar(viewport, content_height, g_scroll);

    DrawLine(
        (int)left,
        (int)footer_top,
        (int)right,
        (int)footer_top,
        RULE
    );

    g_use_interaction_clip = false;

    const Rectangle reset =
    {
        left,
        footer_top + 8.0f,
        132.0f,
        27.0f
    };

    if (
        draw_button(
            reset,
            "RESET DEFAULTS",
            false,
            theme
        )
    )
    {
        AppSettingsDefaults(settings);
        g_scroll = 0.0f;
        changed = true;
    }

    const char *hint =
        panel.width >= 470.0f
            ? "F2 / Esc close  |  F10 OBS hide"
            : "F2 / Esc close";

    const Vector2 hint_size =
        MeasureTextEx(
            UiFontRegular(),
            hint,
            11.5f,
            0.0f
        );

    draw_label(
        hint,
        right - hint_size.x,
        footer_top + 13.0f,
        11.5f,
        MUTED,
        false
    );

    return changed;
}
