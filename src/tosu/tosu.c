#include "tosu.h"

#include <yyjson.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "net/local_http.h"

#define TOSU_PATH "/json/v2"
#define TOSU_POLL_INTERVAL 0.10

static TosuSnapshot g_snapshot;

static double g_last_poll = -1000.0;

static void copy_string(
    char *dest,
    size_t dest_size,
    const char *source
)
{
    if (dest_size == 0)
        return;

    if (source == NULL)
    {
        dest[0] = '\0';
        return;
    }

    snprintf(
        dest,
        dest_size,
        "%s",
        source
    );
}



static yyjson_val *json_pointer(
    yyjson_doc *doc,
    const char *pointer
)
{
    if (!doc || !pointer)
        return NULL;

    return yyjson_doc_ptr_get(
        doc,
        pointer
    );
}



static const char *json_string(
    yyjson_doc *doc,
    const char *pointer
)
{
    yyjson_val *value =
        json_pointer(
            doc,
            pointer
        );


    if (
        value &&
        yyjson_is_str(value)
    )
    {
        return yyjson_get_str(
            value
        );
    }


    return NULL;
}



static double json_number(
    yyjson_doc *doc,
    const char *pointer,
    double fallback
)
{
    yyjson_val *value =
        json_pointer(
            doc,
            pointer
        );


    if (!value)
        return fallback;

    if (yyjson_is_real(value))
    {
        return yyjson_get_real(
            value
        );
    }


    if (yyjson_is_int(value))
    {
        return (double)
            yyjson_get_int(
                value
            );
    }


    if (yyjson_is_uint(value))
    {
        return (double)
            yyjson_get_uint(
                value
            );
    }


    return fallback;
}



static int json_int(
    yyjson_doc *doc,
    const char *pointer,
    int fallback
)
{
    yyjson_val *value =
        json_pointer(
            doc,
            pointer
        );


    if (!value)
        return fallback;


    if (yyjson_is_int(value))
    {
        return (int)
            yyjson_get_int(
                value
            );
    }


    if (yyjson_is_uint(value))
    {
        return (int)
            yyjson_get_uint(
                value
            );
    }


    if (yyjson_is_real(value))
    {
        return (int)
            yyjson_get_real(
                value
            );
    }


    return fallback;
}



static const char *first_string(
    yyjson_doc *doc,
    const char *a,
    const char *b,
    const char *c
)
{
    const char *value;


    if (a)
    {
        value =
            json_string(
                doc,
                a
            );

        if (value)
            return value;
    }


    if (b)
    {
        value =
            json_string(
                doc,
                b
            );

        if (value)
            return value;
    }


    if (c)
    {
        value =
            json_string(
                doc,
                c
            );

        if (value)
            return value;
    }


    return "";
}



static bool is_supported_rate_mod(
    const char *acronym
)
{
    if (!acronym)
        return false;


    return
        strcmp(acronym, "DT") == 0 ||
        strcmp(acronym, "NC") == 0 ||
        strcmp(acronym, "HT") == 0 ||
        strcmp(acronym, "DC") == 0;
}



static bool mod_name_contains_supported_rate_mod(
    const char *mod_name
)
{
    if (!mod_name)
        return false;


    if (strstr(mod_name, "DT"))
        return true;

    if (strstr(mod_name, "NC"))
        return true;

    if (strstr(mod_name, "HT"))
        return true;

    if (strstr(mod_name, "DC"))
        return true;


    return false;
}



static double extract_rate(
    yyjson_doc *doc,
    const char *mod_name
)
{
    if (
        !mod_name_contains_supported_rate_mod(
            mod_name
        )
    )
    {
        return 1.0;
    }

    double rate =
        json_number(
            doc,
            "/play/mods/rate",
            0.0
        );


    if (rate > 0.0)
        return rate;

    yyjson_val *array =
        json_pointer(
            doc,
            "/play/mods/array"
        );


    if (
        array &&
        yyjson_is_arr(array)
    )
    {
        size_t index;
        size_t max;

        yyjson_val *mod;


        yyjson_arr_foreach(
            array,
            index,
            max,
            mod
        )
        {
            if (!yyjson_is_obj(mod))
                continue;


            yyjson_val *acronym_value =
                yyjson_obj_get(
                    mod,
                    "acronym"
                );


            if (
                !acronym_value ||
                !yyjson_is_str(
                    acronym_value
                )
            )
            {
                continue;
            }


            const char *acronym =
                yyjson_get_str(
                    acronym_value
                );


            if (
                !is_supported_rate_mod(
                    acronym
                )
            )
            {
                continue;
            }


            yyjson_val *settings =
                yyjson_obj_get(
                    mod,
                    "settings"
                );


            if (
                settings &&
                yyjson_is_obj(
                    settings
                )
            )
            {
                yyjson_val *speed =
                    yyjson_obj_get(
                        settings,
                        "speed_change"
                    );


                if (
                    speed &&
                    yyjson_is_real(speed)
                )
                {
                    double result =
                        yyjson_get_real(
                            speed
                        );

                    if (result > 0.0)
                        return result;
                }


                if (
                    speed &&
                    yyjson_is_int(speed)
                )
                {
                    double result =
                        (double)
                        yyjson_get_int(
                            speed
                        );

                    if (result > 0.0)
                        return result;
                }


                if (
                    speed &&
                    yyjson_is_uint(speed)
                )
                {
                    double result =
                        (double)
                        yyjson_get_uint(
                            speed
                        );

                    if (result > 0.0)
                        return result;
                }
            }

            if (
                strcmp(acronym, "DT") == 0 ||
                strcmp(acronym, "NC") == 0
            )
            {
                return 1.5;
            }


            if (
                strcmp(acronym, "HT") == 0 ||
                strcmp(acronym, "DC") == 0
            )
            {
                return 0.75;
            }
        }
    }

    if (
        strstr(mod_name, "DT") ||
        strstr(mod_name, "NC")
    )
    {
        return 1.5;
    }

    if (
        strstr(mod_name, "HT") ||
        strstr(mod_name, "DC")
    )
    {
        return 0.75;
    }

    return 1.0;
}



static void parse_snapshot(
    const char *json_data
)
{
    if (!json_data)
    {
        g_snapshot.connected =
            false;

        return;
    }

    yyjson_doc *doc =
        yyjson_read(
            json_data,
            strlen(json_data),
            0
        );

    if (!doc)
    {
        g_snapshot.connected =
            false;

        return;
    }

    g_snapshot.connected =
        true;

    copy_string(
        g_snapshot.client,
        sizeof(
            g_snapshot.client
        ),
        json_string(
            doc,
            "/client"
        )
    );

    copy_string(
        g_snapshot.state,
        sizeof(
            g_snapshot.state
        ),
        first_string(
            doc,
            "/state/name",
            "/state",
            NULL
        )
    );

    copy_string(
        g_snapshot.artist,
        sizeof(
            g_snapshot.artist
        ),
        first_string(
            doc,
            "/beatmap/artist",
            "/beatmap/metadata/artist",
            NULL
        )
    );

    copy_string(
        g_snapshot.title,
        sizeof(
            g_snapshot.title
        ),
        first_string(
            doc,
            "/beatmap/title",
            "/beatmap/metadata/title",
            NULL
        )
    );

    copy_string(
        g_snapshot.difficulty,
        sizeof(
            g_snapshot.difficulty
        ),
        first_string(
            doc,
            "/beatmap/version",
            "/beatmap/difficulty",
            "/beatmap/metadata/difficulty"
        )
    );

    copy_string(
        g_snapshot.creator,
        sizeof(
            g_snapshot.creator
        ),
        first_string(
            doc,
            "/beatmap/mapper",
            "/beatmap/creator",
            "/beatmap/metadata/creator"
        )
    );

    copy_string(
        g_snapshot.checksum,
        sizeof(
            g_snapshot.checksum
        ),
        json_string(
            doc,
            "/beatmap/checksum"
        )
    );

    copy_string(
        g_snapshot.mod,
        sizeof(
            g_snapshot.mod
        ),
        json_string(
            doc,
            "/play/mods/name"
        )
    );

    g_snapshot.rate =
        extract_rate(
            doc,
            g_snapshot.mod
        );

    g_snapshot.time_ms =
        json_number(
            doc,
            "/beatmap/time/live",
            0.0
        );


    g_snapshot.length_ms =
        json_number(
            doc,
            "/beatmap/time/mp3Length",
            0.0
        );

    g_snapshot.mode =
        json_int(
            doc,
            "/beatmap/mode/number",
            -1
        );

    yyjson_doc_free(
        doc
    );
}



bool TosuInit(void)
{
    memset(
        &g_snapshot,
        0,
        sizeof(g_snapshot)
    );

    g_snapshot.rate = 1.0;
    g_snapshot.mode = -1;

    return LocalHttpInit();
}


void TosuShutdown(void)
{
    LocalHttpShutdown();
}


void TosuUpdate(void)
{
    static double last_success_time = -1000.0;

    const double now = GetTime();

    if (now - g_last_poll < TOSU_POLL_INTERVAL)
        return;

    g_last_poll = now;

    const bool was_connected = g_snapshot.connected;

    LocalHttpResponse response = {0};
    char error[192] = "";

    const bool got_response =
        LocalHttpGet(
            TOSU_PATH,
            40,
            80,
            &response,
            error,
            sizeof(error)
        );

    bool request_succeeded = false;

    if (
        got_response &&
        response.status_code == 200 &&
        response.body != NULL &&
        response.body_size > 0
    )
    {
        parse_snapshot((const char *)response.body);

        if (g_snapshot.connected)
        {
            request_succeeded = true;
            last_success_time = now;
        }
    }

    /* 
    * Tosu can miss a poll while lazer changes state or maps. 
    * keep the last good snapshot briefly so the HUD does not flash offline. 
    */

    if (!request_succeeded)
    {
        const double time_since_success =
            now - last_success_time;

        if (
            was_connected &&
            time_since_success <= 1.0
        )
        {
            g_snapshot.connected = true;
        }
        else
        {
            g_snapshot.connected = false;
        }
    }

    LocalHttpResponseFree(&response);
}


const TosuSnapshot *TosuGetSnapshot(void)
{
    return &g_snapshot;
}
