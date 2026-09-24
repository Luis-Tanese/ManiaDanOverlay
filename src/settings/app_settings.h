#ifndef MANIADANOVERLAY_APP_SETTINGS_H
#define MANIADANOVERLAY_APP_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#define APP_SETTINGS_VERSION 6

#define APP_SETTINGS_HUD_DEFAULT_WIDTH 503
#define APP_SETTINGS_HUD_DEFAULT_HEIGHT 240
#define APP_SETTINGS_EXTRA_INFO_DEFAULT_WIDTH 900
#define APP_SETTINGS_EXTRA_INFO_DEFAULT_HEIGHT 600

#define APP_SETTINGS_DESIGN_WIDTH 900
#define APP_SETTINGS_DESIGN_HEIGHT 600

#define APP_SETTINGS_HUD_MIN_WIDTH 360
#define APP_SETTINGS_HUD_MIN_HEIGHT 240
#define APP_SETTINGS_EXTRA_INFO_MIN_WIDTH 540
#define APP_SETTINGS_EXTRA_INFO_MIN_HEIGHT 360

#define APP_SETTINGS_MAX_WIDTH 3840
#define APP_SETTINGS_MAX_HEIGHT 2160

typedef enum
{
    APP_SETTINGS_VIEW_HUD = 0,
    APP_SETTINGS_VIEW_EXTRA_INFO = 1
} AppSettingsViewMode;

typedef enum
{
    APP_SETTINGS_GRAPH_OVERVIEW = 0,
    APP_SETTINGS_GRAPH_FOCUS = 1
} AppSettingsGraphMode;

typedef struct
{
    int version;

    AppSettingsViewMode view_mode;
    AppSettingsGraphMode graph_mode;
    int focus_span_seconds;
    int focus_density_interval_ms;
    bool pause_markers_enabled;

    bool always_on_top;

    bool hud_show_mod_rate;
    bool hud_show_key_mode;
    bool hud_show_graph_mode;
    bool hud_show_pause_count;
    bool hud_show_msd;

    bool remember_window_position;
    bool remember_window_size;
    bool has_window_position;

    int window_x;
    int window_y;

    int hud_window_width;
    int hud_window_height;
    int extra_info_window_width;
    int extra_info_window_height;
} AppSettings;

const int *AppSettingsFocusSpanPresets(size_t *count);
const int *AppSettingsFocusDensityIntervals(size_t *count);

bool AppSettingsEqual(
    const AppSettings *a,
    const AppSettings *b
);

void AppSettingsDefaults(
    AppSettings *settings
);

void AppSettingsSanitize(
    AppSettings *settings
);

int AppSettingsViewWidth(
    const AppSettings *settings,
    AppSettingsViewMode view_mode
);

int AppSettingsViewHeight(
    const AppSettings *settings,
    AppSettingsViewMode view_mode
);

int AppSettingsViewDefaultWidth(
    AppSettingsViewMode view_mode
);

int AppSettingsViewDefaultHeight(
    AppSettingsViewMode view_mode
);

int AppSettingsViewMinWidth(
    AppSettingsViewMode view_mode
);

int AppSettingsViewMinHeight(
    AppSettingsViewMode view_mode
);

void AppSettingsSetViewSize(
    AppSettings *settings,
    AppSettingsViewMode view_mode,
    int width,
    int height
);

bool AppSettingsLoad(
    AppSettings *settings,
    char *error,
    size_t error_size
);

bool AppSettingsSave(
    const AppSettings *settings,
    char *error,
    size_t error_size
);

bool AppSettingsGetPath(
    char *output,
    size_t output_size,
    char *error,
    size_t error_size
);

#endif
