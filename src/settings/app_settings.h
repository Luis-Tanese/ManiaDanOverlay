#ifndef MANIADANOVERLAY_APP_SETTINGS_H
#define MANIADANOVERLAY_APP_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#define APP_SETTINGS_VERSION 3

#define APP_SETTINGS_DEFAULT_WIDTH 503
#define APP_SETTINGS_DEFAULT_HEIGHT 240

#define APP_SETTINGS_DESIGN_WIDTH 900
#define APP_SETTINGS_DESIGN_HEIGHT 600
#define APP_SETTINGS_MIN_WIDTH 360
#define APP_SETTINGS_MIN_HEIGHT 240

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

    bool always_on_top;

    bool remember_window_position;
    bool remember_window_size;
    bool has_window_position;

    int window_x;
    int window_y;
    int window_width;
    int window_height;
} AppSettings;

void AppSettingsDefaults(
    AppSettings *settings
);

void AppSettingsSanitize(
    AppSettings *settings
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
