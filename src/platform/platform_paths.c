#include "platform_paths.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define PLATFORM_PATH_CAPACITY 1024

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
        message ? message : "Unknown platform path error"
    );
}

static char path_separator(void)
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

static bool is_separator(char value)
{
    return value == '/' || value == '\\';
}

static bool make_one_directory(
    const char *path,
    char *error,
    size_t error_size
)
{
    if (!path || path[0] == '\0')
        return false;

#ifdef _WIN32
    if (_mkdir(path) == 0 || errno == EEXIST)
        return true;
#else
    if (mkdir(path, 0700) == 0 || errno == EEXIST)
        return true;
#endif

    char message[256];
    snprintf(
        message,
        sizeof(message),
        "Could not create config directory '%s': %s",
        path,
        strerror(errno)
    );

    set_error(error, error_size, message);
    return false;
}

static bool ensure_directory_tree(
    const char *path,
    char *error,
    size_t error_size
)
{
    if (!path || path[0] == '\0')
    {
        set_error(error, error_size, "Config directory path is empty");
        return false;
    }

    char buffer[PLATFORM_PATH_CAPACITY];

    if (strlen(path) >= sizeof(buffer))
    {
        set_error(error, error_size, "Config directory path is too long");
        return false;
    }

    snprintf(buffer, sizeof(buffer), "%s", path);

    size_t start = 0;

#ifdef _WIN32
    if (
        strlen(buffer) >= 3 &&
        buffer[1] == ':' &&
        is_separator(buffer[2])
    )
    {
        start = 3;
    }
#else
    if (buffer[0] == '/')
        start = 1;
#endif

    for (size_t i = start; buffer[i] != '\0'; ++i)
    {
        if (!is_separator(buffer[i]))
            continue;

        const char saved = buffer[i];
        buffer[i] = '\0';

        if (
            buffer[0] != '\0' &&
            !make_one_directory(buffer, error, error_size)
        )
        {
            return false;
        }

        buffer[i] = saved;
    }

    return make_one_directory(buffer, error, error_size);
}

static bool get_config_root(
    char *output,
    size_t output_size,
    char *error,
    size_t error_size
)
{
    if (!output || output_size == 0)
        return false;

#ifdef _WIN32
    const char *app_data = getenv("APPDATA");

    if (app_data && app_data[0] != '\0')
    {
        if (snprintf(output, output_size, "%s", app_data) >= (int)output_size)
        {
            set_error(error, error_size, "APPDATA path is too long");
            return false;
        }

        return true;
    }

    const char *user_profile = getenv("USERPROFILE");

    if (!user_profile || user_profile[0] == '\0')
    {
        set_error(error, error_size, "Neither APPDATA nor USERPROFILE is available");
        return false;
    }

    if (
        snprintf(
            output,
            output_size,
            "%s\\AppData\\Roaming",
            user_profile
        ) >= (int)output_size
    )
    {
        set_error(error, error_size, "Windows roaming config path is too long");
        return false;
    }
#else
    const char *xdg_config = getenv("XDG_CONFIG_HOME");

    if (xdg_config && xdg_config[0] != '\0')
    {
        if (snprintf(output, output_size, "%s", xdg_config) >= (int)output_size)
        {
            set_error(error, error_size, "XDG_CONFIG_HOME path is too long");
            return false;
        }

        return true;
    }

    const char *home = getenv("HOME");

    if (!home || home[0] == '\0')
    {
        set_error(error, error_size, "HOME is unavailable");
        return false;
    }

    if (
        snprintf(
            output,
            output_size,
            "%s/.config",
            home
        ) >= (int)output_size
    )
    {
        set_error(error, error_size, "Linux config path is too long");
        return false;
    }
#endif

    return true;
}

bool PlatformConfigFilePath(
    const char *application_name,
    const char *filename,
    char *output,
    size_t output_size,
    char *error,
    size_t error_size
)
{
    if (
        !application_name ||
        application_name[0] == '\0' ||
        !filename ||
        filename[0] == '\0' ||
        !output ||
        output_size == 0
    )
    {
        set_error(error, error_size, "Invalid config path arguments");
        return false;
    }

    char root[PLATFORM_PATH_CAPACITY];

    if (!get_config_root(root, sizeof(root), error, error_size))
        return false;

    if (!ensure_directory_tree(root, error, error_size))
        return false;

    const char separator = path_separator();

    char app_directory[PLATFORM_PATH_CAPACITY];

    if (
        snprintf(
            app_directory,
            sizeof(app_directory),
            "%s%c%s",
            root,
            separator,
            application_name
        ) >= (int)sizeof(app_directory)
    )
    {
        set_error(error, error_size, "Application config directory path is too long");
        return false;
    }

    if (!ensure_directory_tree(app_directory, error, error_size))
        return false;

    if (
        snprintf(
            output,
            output_size,
            "%s%c%s",
            app_directory,
            separator,
            filename
        ) >= (int)output_size
    )
    {
        set_error(error, error_size, "Settings file path is too long");
        return false;
    }

    return true;
}

bool PlatformAtomicReplaceFile(
    const char *temporary_path,
    const char *destination_path,
    char *error,
    size_t error_size
)
{
    if (!temporary_path || !destination_path)
    {
        set_error(error, error_size, "Invalid atomic replace arguments");
        return false;
    }

#ifdef _WIN32
    if (
        MoveFileExA(
            temporary_path,
            destination_path,
            MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH
        ) != 0
    )
    {
        return true;
    }

    char message[256];
    snprintf(
        message,
        sizeof(message),
        "Could not replace settings file (Windows error %lu)",
        (unsigned long)GetLastError()
    );

    set_error(error, error_size, message);
    return false;
#else
    if (rename(temporary_path, destination_path) == 0)
        return true;

    char message[256];
    snprintf(
        message,
        sizeof(message),
        "Could not replace settings file: %s",
        strerror(errno)
    );

    set_error(error, error_size, message);
    return false;
#endif
}
