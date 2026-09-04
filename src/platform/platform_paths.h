#ifndef MANIADANOVERLAY_PLATFORM_PATHS_H
#define MANIADANOVERLAY_PLATFORM_PATHS_H

#include <stdbool.h>
#include <stddef.h>

bool PlatformConfigFilePath(
    const char *application_name,
    const char *filename,
    char *output,
    size_t output_size,
    char *error,
    size_t error_size
);

bool PlatformAtomicReplaceFile(
    const char *temporary_path,
    const char *destination_path,
    char *error,
    size_t error_size
);

#endif
