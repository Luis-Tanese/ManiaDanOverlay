#include "beatmap_fetch.h"

#include <stdio.h>

#include "net/local_http.h"
#include "osu_parser.h"

#define BEATMAP_PATH "/files/beatmap/file"

static bool g_beatmap_fetch_ready = false;

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
        message ? message : "Unknown beatmap error"
    );
}

bool BeatmapFetchInit(void)
{
    if (g_beatmap_fetch_ready)
        return true;

    if (!LocalHttpInit())
        return false;

    g_beatmap_fetch_ready = true;
    return true;
}

void BeatmapFetchShutdown(void)
{
    if (!g_beatmap_fetch_ready)
        return;

    g_beatmap_fetch_ready = false;
    LocalHttpShutdown();
}

/* 
 * fetch the exact map Tosu says is active instead of trying to locate lazer storage ourselves. 
 * the checksum in main.c decides when this needs refreshing. 
 */

bool BeatmapFetchCurrent(
    Beatmap *out_map,
    char *error,
    size_t error_size
)
{
    if (!g_beatmap_fetch_ready || !out_map)
    {
        set_error(
            error,
            error_size,
            "Beatmap fetcher is not initialized"
        );
        return false;
    }

    LocalHttpResponse response = {0};
    char request_error[256] = "";

    if (
        !LocalHttpGet(
            BEATMAP_PATH,
            500,
            2000,
            &response,
            request_error,
            sizeof(request_error)
        )
    )
    {
        char message[320];

        snprintf(
            message,
            sizeof(message),
            "Beatmap request failed: %s",
            request_error[0] ? request_error : "local HTTP error"
        );

        set_error(error, error_size, message);
        LocalHttpResponseFree(&response);
        return false;
    }

    if (response.status_code != 200)
    {
        char message[128];

        snprintf(
            message,
            sizeof(message),
            "Beatmap endpoint returned HTTP %d",
            response.status_code
        );

        set_error(error, error_size, message);
        LocalHttpResponseFree(&response);
        return false;
    }

    if (!response.body || response.body_size == 0)
    {
        set_error(
            error,
            error_size,
            "Beatmap endpoint returned no data"
        );

        LocalHttpResponseFree(&response);
        return false;
    }

    const bool parsed =
        OsuParseBeatmapText(
            (const char *)response.body,
            out_map,
            error,
            error_size
        );

    LocalHttpResponseFree(&response);
    return parsed;
}
