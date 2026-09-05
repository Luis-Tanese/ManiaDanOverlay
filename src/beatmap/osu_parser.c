#include "osu_parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef enum
{
    SECTION_NONE,
    SECTION_GENERAL,
    SECTION_METADATA,
    SECTION_DIFFICULTY,
    SECTION_TIMINGPOINTS,
    SECTION_HITOBJECTS
} OsuSection;


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
        message ? message : "Unknown parser error"
    );
}


static void trim_right(
    char *text
)
{
    if (!text)
        return;

    size_t length = strlen(text);

    while (
        length > 0 &&
        (
            text[length - 1] == '\r' ||
            text[length - 1] == '\n' ||
            text[length - 1] == ' '  ||
            text[length - 1] == '\t'
        )
    )
    {
        text[length - 1] = '\0';
        length--;
    }
}


static const char *skip_space(
    const char *text
)
{
    while (
        text &&
        (
            *text == ' ' ||
            *text == '\t'
        )
    )
    {
        text++;
    }

    return text;
}


static bool starts_with(
    const char *text,
    const char *prefix
)
{
    if (!text || !prefix)
        return false;

    const size_t prefix_length = strlen(prefix);

    return strncmp(text, prefix, prefix_length) == 0;
}


static void copy_value(
    char *destination,
    size_t destination_size,
    const char *line,
    const char *prefix
)
{
    if (
        !destination ||
        destination_size == 0 ||
        !line ||
        !prefix
    )
    {
        return;
    }

    const char *value = line + strlen(prefix);
    value = skip_space(value);

    snprintf(
        destination,
        destination_size,
        "%s",
        value
    );
}


static int mania_column_from_x(
    long x,
    int keys
)
{
    if (keys <= 0)
        return 0;

    int column = (int)((x * keys) / 512);

    if (column < 0)
        column = 0;

    if (column >= keys)
        column = keys - 1;

    return column;
}


static bool consume_long(
    const char **cursor,
    long *value
)
{
    if (!cursor || !*cursor || !value)
        return false;

    errno = 0;

    char *end = NULL;

    const long result = strtol(
        *cursor,
        &end,
        10
    );

    if (end == *cursor || errno != 0)
        return false;

    *cursor = end;
    *value = result;

    return true;
}


static bool consume_double(
    const char **cursor,
    double *value
)
{
    if (!cursor || !*cursor || !value)
        return false;

    errno = 0;

    char *end = NULL;

    const double result = strtod(
        *cursor,
        &end
    );

    if (end == *cursor || errno != 0)
        return false;

    *cursor = end;
    *value = result;

    return true;
}


static bool consume_comma(
    const char **cursor
)
{
    if (!cursor || !*cursor || **cursor != ',')
        return false;

    (*cursor)++;
    return true;
}


static bool csv_field(
    const char *line,
    int field_index,
    char *output,
    size_t output_size
)
{
    if (
        !line ||
        field_index < 0 ||
        !output ||
        output_size == 0
    )
    {
        return false;
    }

    const char *start = line;

    for (int index = 0; index < field_index; ++index)
    {
        const char *comma = strchr(start, ',');

        if (!comma)
            return false;

        start = comma + 1;
    }

    const char *end = strchr(start, ',');

    if (!end)
        end = start + strlen(start);

    size_t length = (size_t)(end - start);

    if (length >= output_size)
        length = output_size - 1;

    memcpy(output, start, length);
    output[length] = '\0';

    return true;
}


static bool parse_timing_point_bpm(
    const char *line,
    double *out_bpm
)
{
    if (!line || !out_bpm)
        return false;

    char beat_length_text[64];
    char uninherited_text[32];

    if (
        !csv_field(
            line,
            1,
            beat_length_text,
            sizeof(beat_length_text)
        ) ||
        !csv_field(
            line,
            6,
            uninherited_text,
            sizeof(uninherited_text)
        )
    )
    {
        return false;
    }

    char *end = NULL;

    const double beat_length = strtod(
        beat_length_text,
        &end
    );

    if (
        end == beat_length_text ||
        beat_length <= 0.0
    )
    {
        return false;
    }

    end = NULL;

    const long uninherited = strtol(
        uninherited_text,
        &end,
        10
    );

    if (
        end == uninherited_text ||
        uninherited != 1
    )
    {
        return false;
    }

    *out_bpm = 60000.0 / beat_length;

    return true;
}


static bool parse_hitobject(
    const char *line,
    int keys,
    ManiaNote *out_note
)
{
    if (!line || !out_note || keys <= 0)
        return false;

    const char *cursor = line;

    long x = 0;
    long y = 0;
    double start_ms = 0.0;
    long type = 0;
    long hit_sound = 0;

    if (!consume_long(&cursor, &x))
        return false;

    if (!consume_comma(&cursor))
        return false;

    if (!consume_long(&cursor, &y))
        return false;

    if (!consume_comma(&cursor))
        return false;

    if (!consume_double(&cursor, &start_ms))
        return false;

    if (!consume_comma(&cursor))
        return false;

    if (!consume_long(&cursor, &type))
        return false;

    if (!consume_comma(&cursor))
        return false;

    if (!consume_long(&cursor, &hit_sound))
        return false;

    (void)y;
    (void)hit_sound;

    const bool is_hold = (type & 128) != 0;
    const bool is_tap = (type & 1) != 0;

    if (!is_hold && !is_tap)
        return false;

    double end_ms = start_ms;

    if (is_hold)
    {
        if (!consume_comma(&cursor))
            return false;

        if (!consume_double(&cursor, &end_ms))
            return false;

        if (end_ms < start_ms)
            end_ms = start_ms;
    }

    out_note->type =
        is_hold
            ? MANIA_NOTE_HOLD
            : MANIA_NOTE_TAP;

    out_note->column = (uint8_t)mania_column_from_x(
        x,
        keys
    );

    out_note->start_ms = start_ms;
    out_note->end_ms = end_ms;

    return true;
}


static OsuSection parse_section(
    const char *line
)
{
    if (strcmp(line, "[General]") == 0)
        return SECTION_GENERAL;

    if (strcmp(line, "[Metadata]") == 0)
        return SECTION_METADATA;

    if (strcmp(line, "[Difficulty]") == 0)
        return SECTION_DIFFICULTY;

    if (strcmp(line, "[TimingPoints]") == 0)
        return SECTION_TIMINGPOINTS;

    if (strcmp(line, "[HitObjects]") == 0)
        return SECTION_HITOBJECTS;

    return SECTION_NONE;
}


bool OsuParseBeatmapText(
    const char *text,
    Beatmap *out_map,
    char *error,
    size_t error_size
)
{
    if (!text || !out_map)
    {
        set_error(
            error,
            error_size,
            "Invalid parser arguments"
        );

        return false;
    }

    BeatmapFree(out_map);
    BeatmapInit(out_map);

    out_map->bpm = 120.0;

    OsuSection section = SECTION_NONE;

    bool found_circle_size = false;
    bool found_hitobjects = false;
    bool found_base_bpm = false;

    const char *cursor = text;

    while (*cursor)
    {
        const char *line_end = strchr(cursor, '\n');

        const size_t line_length =
            line_end
                ? (size_t)(line_end - cursor)
                : strlen(cursor);

        char *line = malloc(line_length + 1);

        if (!line)
        {
            set_error(
                error,
                error_size,
                "Out of memory while parsing beatmap"
            );

            BeatmapFree(out_map);
            return false;
        }

        memcpy(line, cursor, line_length);
        line[line_length] = '\0';

        trim_right(line);

        const char *clean = skip_space(line);

        if (
            clean[0] == '\0' ||
            starts_with(clean, "//")
        )
        {
            free(line);

            if (!line_end)
                break;

            cursor = line_end + 1;
            continue;
        }

        if (clean[0] == '[')
        {
            section = parse_section(clean);

            if (section == SECTION_HITOBJECTS)
                found_hitobjects = true;

            free(line);

            if (!line_end)
                break;

            cursor = line_end + 1;
            continue;
        }

        if (section == SECTION_METADATA)
        {
            if (starts_with(clean, "Artist:"))
            {
                copy_value(
                    out_map->artist,
                    sizeof(out_map->artist),
                    clean,
                    "Artist:"
                );
            }
            else if (starts_with(clean, "Title:"))
            {
                copy_value(
                    out_map->title,
                    sizeof(out_map->title),
                    clean,
                    "Title:"
                );
            }
            else if (starts_with(clean, "Creator:"))
            {
                copy_value(
                    out_map->creator,
                    sizeof(out_map->creator),
                    clean,
                    "Creator:"
                );
            }
            else if (starts_with(clean, "Version:"))
            {
                copy_value(
                    out_map->difficulty,
                    sizeof(out_map->difficulty),
                    clean,
                    "Version:"
                );
            }
        }
        else if (section == SECTION_DIFFICULTY)
        {
            if (starts_with(clean, "CircleSize:"))
            {
                const char *value =
                    skip_space(
                        clean + strlen("CircleSize:")
                    );

                const double keys = strtod(value, NULL);

                out_map->keys = (int)(keys + 0.5);
                found_circle_size = true;
            }
        }
        else if (
            section == SECTION_TIMINGPOINTS &&
            !found_base_bpm
        )
        {
            double bpm = 0.0;

            if (parse_timing_point_bpm(clean, &bpm))
            {
                out_map->bpm = bpm;
                found_base_bpm = true;
            }
        }
        else if (section == SECTION_HITOBJECTS)
        {
            ManiaNote note;

            if (
                parse_hitobject(
                    clean,
                    out_map->keys,
                    &note
                )
            )
            {
                if (!BeatmapAppendNote(out_map, note))
                {
                    free(line);

                    set_error(
                        error,
                        error_size,
                        "Out of memory while storing notes"
                    );

                    BeatmapFree(out_map);
                    return false;
                }
            }
        }

        free(line);

        if (!line_end)
            break;

        cursor = line_end + 1;
    }

    if (!found_circle_size)
    {
        set_error(
            error,
            error_size,
            "Beatmap has no CircleSize"
        );

        BeatmapFree(out_map);
        return false;
    }

    if (out_map->keys != 4)
    {
        char message[128];

        snprintf(
            message,
            sizeof(message),
            "Unsupported key count: %dK (only 4K is supported)",
            out_map->keys
        );

        set_error(
            error,
            error_size,
            message
        );

        BeatmapFree(out_map);
        return false;
    }

    if (!found_hitobjects)
    {
        set_error(
            error,
            error_size,
            "Beatmap has no [HitObjects] section"
        );

        BeatmapFree(out_map);
        return false;
    }

    if (out_map->note_count == 0)
    {
        set_error(
            error,
            error_size,
            "No mania notes were parsed"
        );

        BeatmapFree(out_map);
        return false;
    }

    set_error(error, error_size, "");
    return true;
}
