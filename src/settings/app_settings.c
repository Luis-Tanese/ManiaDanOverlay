#include "app_settings.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <yyjson.h>

#include "app/identity.h"
#include "platform/platform_paths.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define SETTINGS_FILENAME "settings.json"
#define SETTINGS_PATH_CAPACITY 1024

static const int FOCUS_SPANS[] =
{
    15,
    30,
    45,
    60,
    90,
    120,
    180
};

static void set_error(
    char *error,
    size_t error_size,
    const char *message
)
{
    if (!error || error_size == 0)
        return;

    snprintf(
        error,
        error_size,
        "%s",
        message ? message : "Unknown settings error"
    );
}

static int clamp_int(
    int value,
    int minimum,
    int maximum
)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static int nearest_focus_span(
    int value
)
{
    int nearest = FOCUS_SPANS[0];
    int nearest_distance = value - nearest;

    if (nearest_distance < 0)
        nearest_distance = -nearest_distance;

    for (
        size_t i = 1;
        i < sizeof(FOCUS_SPANS) / sizeof(FOCUS_SPANS[0]);
        ++i
    )
    {
        int distance = value - FOCUS_SPANS[i];

        if (distance < 0)
            distance = -distance;

        if (distance < nearest_distance)
        {
            nearest = FOCUS_SPANS[i];
            nearest_distance = distance;
        }
    }

    return nearest;
}

static bool json_bool_or(
    yyjson_val *object,
    const char *key,
    bool fallback
)
{
    if (!object || !key)
        return fallback;

    yyjson_val *value = yyjson_obj_get(object, key);

    if (!value || !yyjson_is_bool(value))
        return fallback;

    return yyjson_get_bool(value);
}

static int json_int_or(
    yyjson_val *object,
    const char *key,
    int fallback
)
{
    if (!object || !key)
        return fallback;

    yyjson_val *value = yyjson_obj_get(object, key);

    if (!value)
        return fallback;

    if (yyjson_is_int(value))
        return (int)yyjson_get_int(value);

    if (yyjson_is_uint(value))
        return (int)yyjson_get_uint(value);

    return fallback;
}

static bool flush_settings_file(
    FILE *file,
    char *error,
    size_t error_size
)
{
    if (!file)
        return false;

    if (fflush(file) != 0)
    {
        set_error(error, error_size, "Could not flush settings file");
        return false;
    }

#ifdef _WIN32
    if (_commit(_fileno(file)) != 0)
    {
        set_error(error, error_size, "Could not commit settings file");
        return false;
    }
#else
    if (fsync(fileno(file)) != 0)
    {
        set_error(error, error_size, "Could not sync settings file");
        return false;
    }
#endif

    return true;
}

void AppSettingsDefaults(
    AppSettings *settings
)
{
    if (!settings)
        return;

    *settings = (AppSettings)
    {
        .version = APP_SETTINGS_VERSION,

        .view_mode = APP_SETTINGS_VIEW_HUD,
        .graph_mode = APP_SETTINGS_GRAPH_OVERVIEW,
        .focus_span_seconds = 30,

        .always_on_top = true,

        .remember_window_position = true,
        .remember_window_size = true,
        .has_window_position = true,

        .window_x = 1920,
        .window_y = 381,
        .window_width = APP_SETTINGS_DEFAULT_WIDTH,
        .window_height = APP_SETTINGS_DEFAULT_HEIGHT
    };
}

void AppSettingsSanitize(
    AppSettings *settings
)
{
    if (!settings)
        return;

    settings->version = APP_SETTINGS_VERSION;

    if (
        settings->view_mode != APP_SETTINGS_VIEW_HUD &&
        settings->view_mode != APP_SETTINGS_VIEW_EXTRA_INFO
    )
    {
        settings->view_mode = APP_SETTINGS_VIEW_HUD;
    }

    if (
        settings->graph_mode != APP_SETTINGS_GRAPH_OVERVIEW &&
        settings->graph_mode != APP_SETTINGS_GRAPH_FOCUS
    )
    {
        settings->graph_mode = APP_SETTINGS_GRAPH_OVERVIEW;
    }

    settings->focus_span_seconds =
        nearest_focus_span(settings->focus_span_seconds);

    settings->window_width =
        clamp_int(
            settings->window_width,
            APP_SETTINGS_MIN_WIDTH,
            3840
        );

    settings->window_height =
        clamp_int(
            settings->window_height,
            APP_SETTINGS_MIN_HEIGHT,
            2160
        );

    settings->window_x =
        clamp_int(
            settings->window_x,
            -32768,
            32768
        );

    settings->window_y =
        clamp_int(
            settings->window_y,
            -32768,
            32768
        );
}

bool AppSettingsGetPath(
    char *output,
    size_t output_size,
    char *error,
    size_t error_size
)
{
    return PlatformConfigFilePath(
        MANIADANOVERLAY_NAME,
        SETTINGS_FILENAME,
        output,
        output_size,
        error,
        error_size
    );
}

bool AppSettingsLoad(
    AppSettings *settings,
    char *error,
    size_t error_size
)
{
    if (!settings)
    {
        set_error(error, error_size, "Settings output is null");
        return false;
    }

    AppSettingsDefaults(settings);

    char path[SETTINGS_PATH_CAPACITY];

    if (
        !AppSettingsGetPath(
            path,
            sizeof(path),
            error,
            error_size
        )
    )
    {
        return false;
    }

    FILE *probe = fopen(path, "rb");

    if (!probe)
    {
        if (errno == ENOENT)
            return true;

        char message[256];
        snprintf(
            message,
            sizeof(message),
            "Could not open settings file: %s",
            strerror(errno)
        );
        set_error(error, error_size, message);
        return false;
    }

    fclose(probe);

    yyjson_read_err read_error;
    yyjson_doc *document =
        yyjson_read_file(
            path,
            0,
            NULL,
            &read_error
        );

    if (!document)
    {
        char message[320];
        snprintf(
            message,
            sizeof(message),
            "Could not parse settings.json near byte %zu: %s",
            read_error.pos,
            read_error.msg ? read_error.msg : "invalid JSON"
        );
        set_error(error, error_size, message);
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(document);

    if (!root || !yyjson_is_obj(root))
    {
        yyjson_doc_free(document);
        set_error(error, error_size, "settings.json root must be an object");
        return false;
    }

    const int version =
        json_int_or(root, "version", APP_SETTINGS_VERSION);

    if (version > APP_SETTINGS_VERSION)
    {
        yyjson_doc_free(document);
        set_error(error, error_size, "settings.json was written by a newer ManiaDanOverlay version");
        return false;
    }

    yyjson_val *view_mode =
        yyjson_obj_get(root, "view_mode");

    if (view_mode && yyjson_is_str(view_mode))
    {
        const char *mode = yyjson_get_str(view_mode);

        if (mode && strcmp(mode, "extra_info") == 0)
            settings->view_mode = APP_SETTINGS_VIEW_EXTRA_INFO;
        else
            settings->view_mode = APP_SETTINGS_VIEW_HUD;
    }

    yyjson_val *graph_mode =
        yyjson_obj_get(root, "graph_mode");

    if (graph_mode && yyjson_is_str(graph_mode))
    {
        const char *mode = yyjson_get_str(graph_mode);

        if (mode && strcmp(mode, "focus") == 0)
            settings->graph_mode = APP_SETTINGS_GRAPH_FOCUS;
        else
            settings->graph_mode = APP_SETTINGS_GRAPH_OVERVIEW;
    }

    settings->focus_span_seconds =
        json_int_or(
            root,
            "focus_span_seconds",
            settings->focus_span_seconds
        );

    settings->always_on_top =
        json_bool_or(
            root,
            "always_on_top",
            settings->always_on_top
        );

    yyjson_val *window = yyjson_obj_get(root, "window");

    if (window && yyjson_is_obj(window))
    {
        settings->remember_window_position =
            json_bool_or(
                window,
                "remember_position",
                settings->remember_window_position
            );

        settings->remember_window_size =
            json_bool_or(
                window,
                "remember_size",
                settings->remember_window_size
            );

        settings->has_window_position =
            json_bool_or(
                window,
                "has_position",
                settings->has_window_position
            );

        settings->window_x =
            json_int_or(window, "x", settings->window_x);

        settings->window_y =
            json_int_or(window, "y", settings->window_y);

        settings->window_width =
            json_int_or(window, "width", settings->window_width);

        settings->window_height =
            json_int_or(window, "height", settings->window_height);
    }

    yyjson_doc_free(document);

    AppSettingsSanitize(settings);
    return true;
}

bool AppSettingsSave(
    const AppSettings *settings,
    char *error,
    size_t error_size
)
{
    if (!settings)
    {
        set_error(error, error_size, "Settings input is null");
        return false;
    }

    AppSettings clean = *settings;
    AppSettingsSanitize(&clean);

    char path[SETTINGS_PATH_CAPACITY];

    if (
        !AppSettingsGetPath(
            path,
            sizeof(path),
            error,
            error_size
        )
    )
    {
        return false;
    }

    char temporary_path[SETTINGS_PATH_CAPACITY + 8];

    if (
        snprintf(
            temporary_path,
            sizeof(temporary_path),
            "%s.tmp",
            path
        ) >= (int)sizeof(temporary_path)
    )
    {
        set_error(error, error_size, "Temporary settings path is too long");
        return false;
    }

    FILE *file = fopen(temporary_path, "wb");

    if (!file)
    {
        char message[256];
        snprintf(
            message,
            sizeof(message),
            "Could not create settings file: %s",
            strerror(errno)
        );
        set_error(error, error_size, message);
        return false;
    }

    const char *view_mode =
        clean.view_mode == APP_SETTINGS_VIEW_EXTRA_INFO
            ? "extra_info"
            : "hud";

    const char *graph_mode =
        clean.graph_mode == APP_SETTINGS_GRAPH_FOCUS
            ? "focus"
            : "overview";

    const int write_result =
        fprintf(
            file,
            "{\n"
            "  \"version\": %d,\n"
            "  \"view_mode\": \"%s\",\n"
            "  \"graph_mode\": \"%s\",\n"
            "  \"focus_span_seconds\": %d,\n"
            "  \"always_on_top\": %s,\n"
            "  \"window\": {\n"
            "    \"remember_position\": %s,\n"
            "    \"remember_size\": %s,\n"
            "    \"has_position\": %s,\n"
            "    \"x\": %d,\n"
            "    \"y\": %d,\n"
            "    \"width\": %d,\n"
            "    \"height\": %d\n"
            "  }\n"
            "}\n",
            APP_SETTINGS_VERSION,
            view_mode,
            graph_mode,
            clean.focus_span_seconds,
            clean.always_on_top ? "true" : "false",
            clean.remember_window_position ? "true" : "false",
            clean.remember_window_size ? "true" : "false",
            clean.has_window_position ? "true" : "false",
            clean.window_x,
            clean.window_y,
            clean.window_width,
            clean.window_height
        );

    if (write_result < 0)
    {
        fclose(file);
        remove(temporary_path);
        set_error(error, error_size, "Could not write settings file");
        return false;
    }

    if (!flush_settings_file(file, error, error_size))
    {
        fclose(file);
        remove(temporary_path);
        return false;
    }

    if (fclose(file) != 0)
    {
        remove(temporary_path);
        set_error(error, error_size, "Could not close settings file");
        return false;
    }

    if (
        !PlatformAtomicReplaceFile(
            temporary_path,
            path,
            error,
            error_size
        )
    )
    {
        remove(temporary_path);
        return false;
    }

    return true;
}
