#include "rhythm_profile.h"
#include "calibration/mania4k_calibration.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



typedef enum
{
    RHYTHM_LINEAR_STREAM = 0,
    RHYTHM_HARMONIC_FLOW,
    RHYTHM_ANCHOR_BURST,
    RHYTHM_COORDINATION,
    RHYTHM_DENSITY,
    RHYTHM_WILDCARD,

    RHYTHM_COUNT
} RhythmKind;


typedef enum
{
    SUB_NONE = 0,

    SUB_ROLLS,
    SUB_TRILLS,
    SUB_MINI_TRILLS,

    SUB_CHORD_STREAM,
    SUB_SPLIT_TRILL,
    SUB_JUMP_TRILL,
    SUB_JUMPSTREAM,
    SUB_HANDSTREAM,
    SUB_DOUBLE_JUMP,
    SUB_TRIPLE_JUMP,
    SUB_QUAD_STREAM,
    SUB_LIGHT_CHORDS,
    SUB_DENSE_CHORDS,
    SUB_CHORD_ROLL,
    SUB_BRACKETS,

    SUB_LONG_JACKS,
    SUB_CHORD_JACKS,
    SUB_MINI_JACKS,
    SUB_GLUTS,

    SUB_COLUMN_LOCK,
    SUB_SHIELD,
    SUB_RELEASE,

    SUB_INVERSE,
    SUB_JS_DENSITY,
    SUB_HS_DENSITY,
    SUB_DS_DENSITY,
    SUB_DCS_DENSITY,
    SUB_LCS_DENSITY,

    SUB_JACK_WILD,
    SUB_SPEED_WILD,

    SUB_COUNT
} SubTexture;


typedef enum
{
    FLOW_NONE = 0,
    FLOW_LEFT,
    FLOW_RIGHT,
    FLOW_INWARD,
    FLOW_OUTWARD
} FlowDirection;


typedef struct
{
    double time_ms;
    uint8_t column;
} RhythmEvent;


typedef struct
{
    double time_ms;
    uint8_t mask;
} RhythmRow;


typedef struct
{
    double offset_ms;
    double ms_per_beat;
    double beat_length;

    uint8_t raw_mask;

    int active_count;
    int jack_count;

    FlowDirection direction;
    bool is_roll;
} BeatFrame;


typedef struct
{
    RhythmKind rhythm;
    SubTexture subtype;

    bool is_volatile;

    double start_ms;
    double end_ms;
    double ms_per_beat;

    size_t cluster_index;
} Texture;


typedef struct
{
    bool is_volatile;
    RhythmKind rhythm;

    double anchor_mpb;
    double sum_mpb;
    size_t count;

    long bpm;
} TextureCluster;


typedef struct
{
    double start_ms;
    double end_ms;
    SubTexture subtype;
} ProfileItem;


typedef struct
{
    RhythmKind rhythm;
    bool is_volatile;
    long bpm;

    ProfileItem *items;
    size_t item_count;
    size_t item_capacity;

    size_t subtype_counts[SUB_COUNT];
    size_t subtype_first_seen[SUB_COUNT];
    size_t subtype_seen_counter;

    SubTexture dominant_sub;

    double weight;
    double coverage_ms;
    double importance;
} RhythmProfile;


static int event_compare(
    const void *a,
    const void *b
)
{
    const RhythmEvent *left = a;
    const RhythmEvent *right = b;

    if (left->time_ms < right->time_ms)
        return -1;

    if (left->time_ms > right->time_ms)
        return 1;

    if (left->column < right->column)
        return -1;

    if (left->column > right->column)
        return 1;

    return 0;
}


static int profile_item_compare(
    const void *a,
    const void *b
)
{
    const ProfileItem *left = a;
    const ProfileItem *right = b;

    if (left->start_ms < right->start_ms)
        return -1;

    if (left->start_ms > right->start_ms)
        return 1;

    if (left->end_ms < right->end_ms)
        return -1;

    if (left->end_ms > right->end_ms)
        return 1;

    return 0;
}


static int mask_popcount(
    uint8_t mask
)
{
    int count = 0;

    while (mask)
    {
        count += mask & 1u;
        mask >>= 1u;
    }

    return count;
}


static int mask_first_column(
    uint8_t mask
)
{
    for (int column = 0; column < 4; ++column)
    {
        if (mask & (1u << column))
            return column;
    }

    return -1;
}


static int mask_last_column(
    uint8_t mask
)
{
    for (int column = 3; column >= 0; --column)
    {
        if (mask & (1u << column))
            return column;
    }

    return -1;
}


static double round_places(
    double value,
    double scale
)
{
    if (!isfinite(value))
        return 0.0;

    return floor(value * scale + 0.5) / scale;
}


static double min_double(
    double a,
    double b
)
{
    return a < b ? a : b;
}


static double max_double(
    double a,
    double b
)
{
    return a > b ? a : b;
}


static const char *rhythm_name(
    RhythmKind rhythm
)
{
    switch (rhythm)
    {
        case RHYTHM_LINEAR_STREAM:
            return "linear_stream";

        case RHYTHM_HARMONIC_FLOW:
            return "harmonic_flow";

        case RHYTHM_ANCHOR_BURST:
            return "anchor_burst";

        case RHYTHM_COORDINATION:
            return "coordination";

        case RHYTHM_DENSITY:
            return "density";

        case RHYTHM_WILDCARD:
            return "wildcard";

        default:
            return "unknown";
    }
}


static const char *subtype_name(
    SubTexture subtype
)
{
    switch (subtype)
    {
        case SUB_ROLLS: return "rolls";
        case SUB_TRILLS: return "trills";
        case SUB_MINI_TRILLS: return "mini_trills";
        case SUB_CHORD_STREAM: return "chord_stream";
        case SUB_SPLIT_TRILL: return "split_trill";
        case SUB_JUMP_TRILL: return "jump_trill";
        case SUB_JUMPSTREAM: return "jumpstream";
        case SUB_HANDSTREAM: return "handstream";
        case SUB_DOUBLE_JUMP: return "double_jump";
        case SUB_TRIPLE_JUMP: return "triple_jump";
        case SUB_QUAD_STREAM: return "quad_stream";
        case SUB_LIGHT_CHORDS: return "light_chords";
        case SUB_DENSE_CHORDS: return "dense_chords";
        case SUB_CHORD_ROLL: return "chord_roll";
        case SUB_BRACKETS: return "brackets";
        case SUB_LONG_JACKS: return "long_jacks";
        case SUB_CHORD_JACKS: return "chord_jacks";
        case SUB_MINI_JACKS: return "mini_jacks";
        case SUB_GLUTS: return "gluts";
        case SUB_COLUMN_LOCK: return "column_lock";
        case SUB_SHIELD: return "shield";
        case SUB_RELEASE: return "release";
        case SUB_INVERSE: return "inverse";
        case SUB_JS_DENSITY: return "js_density";
        case SUB_HS_DENSITY: return "hs_density";
        case SUB_DS_DENSITY: return "ds_density";
        case SUB_DCS_DENSITY: return "dcs_density";
        case SUB_LCS_DENSITY: return "lcs_density";
        case SUB_JACK_WILD: return "jack_wild";
        case SUB_SPEED_WILD: return "speed_wild";
        case SUB_NONE:
        default:
            return "generic";
    }
}


static double rhythm_weight(
    RhythmKind rhythm
)
{
    if (rhythm < 0 || rhythm >= RHYTHM_COUNT)
        return 1.0;

    return MANIA4K_CALIBRATION.rhythm_profile.rhythm_weights[rhythm];
}


static ChartFamily rhythm_family(
    RhythmKind rhythm
)
{
    switch (rhythm)
    {
        case RHYTHM_LINEAR_STREAM:
            return CHART_FAMILY_STREAM;

        case RHYTHM_HARMONIC_FLOW:
            return CHART_FAMILY_SPEED;

        case RHYTHM_ANCHOR_BURST:
            return CHART_FAMILY_JACK;

        case RHYTHM_COORDINATION:
            return CHART_FAMILY_TECH;

        case RHYTHM_DENSITY:
            return CHART_FAMILY_STAMINA;

        case RHYTHM_WILDCARD:
            return CHART_FAMILY_TECH;

        default:
            return CHART_FAMILY_HYBRID;
    }
}


static ChartFamily subtype_family(
    SubTexture subtype,
    RhythmKind fallback_rhythm
)
{
    switch (subtype)
    {
        case SUB_ROLLS:
            return CHART_FAMILY_STREAM;

        case SUB_TRILLS:
        case SUB_MINI_TRILLS:
        case SUB_SPLIT_TRILL:
        case SUB_JUMP_TRILL:
        case SUB_CHORD_ROLL:
        case SUB_BRACKETS:
        case SUB_COLUMN_LOCK:
        case SUB_SHIELD:
        case SUB_RELEASE:
        case SUB_INVERSE:
            return CHART_FAMILY_TECH;

        case SUB_CHORD_STREAM:
        case SUB_JUMPSTREAM:
        case SUB_DOUBLE_JUMP:
        case SUB_TRIPLE_JUMP:
        case SUB_LIGHT_CHORDS:
        case SUB_JS_DENSITY:
        case SUB_LCS_DENSITY:
        case SUB_SPEED_WILD:
            return CHART_FAMILY_SPEED;

        case SUB_HANDSTREAM:
        case SUB_QUAD_STREAM:
        case SUB_DENSE_CHORDS:
        case SUB_HS_DENSITY:
        case SUB_DS_DENSITY:
        case SUB_DCS_DENSITY:
            return CHART_FAMILY_STAMINA;

        case SUB_LONG_JACKS:
        case SUB_CHORD_JACKS:
        case SUB_MINI_JACKS:
        case SUB_GLUTS:
        case SUB_JACK_WILD:
            return CHART_FAMILY_JACK;

        case SUB_NONE:
        default:
            return rhythm_family(fallback_rhythm);
    }
}


static bool subtype_forces_tech(
    SubTexture subtype
)
{
    switch (subtype)
    {
        case SUB_TRILLS:
        case SUB_MINI_TRILLS:
        case SUB_SPLIT_TRILL:
        case SUB_JUMP_TRILL:
        case SUB_CHORD_ROLL:
        case SUB_BRACKETS:
        case SUB_COLUMN_LOCK:
        case SUB_SHIELD:
        case SUB_INVERSE:
            return true;

        default:
            return false;
    }
}


static bool append_profile_item(
    RhythmProfile *profile,
    ProfileItem item
)
{
    if (!profile)
        return false;

    if (profile->item_count >= profile->item_capacity)
    {
        const size_t new_capacity =
            profile->item_capacity == 0
                ? 16
                : profile->item_capacity * 2;

        ProfileItem *new_items = realloc(
            profile->items,
            new_capacity * sizeof(*new_items)
        );

        if (!new_items)
            return false;

        profile->items = new_items;
        profile->item_capacity = new_capacity;
    }

    profile->items[profile->item_count++] = item;

    if (
        item.subtype > SUB_NONE &&
        item.subtype < SUB_COUNT
    )
    {
        if (profile->subtype_counts[item.subtype] == 0)
        {
            profile->subtype_first_seen[item.subtype] =
                profile->subtype_seen_counter++;
        }

        profile->subtype_counts[item.subtype]++;
    }

    return true;
}


static void free_profiles(
    RhythmProfile *profiles,
    size_t count
)
{
    if (!profiles)
        return;

    for (size_t i = 0; i < count; ++i)
        free(profiles[i].items);

    free(profiles);
}


static void detect_flow(
    uint8_t previous_mask,
    uint8_t current_mask,
    FlowDirection *out_direction,
    bool *out_roll
)
{
    if (!out_direction || !out_roll)
        return;

    *out_direction = FLOW_NONE;
    *out_roll = false;

    if (previous_mask == 0 || current_mask == 0)
        return;

    const int pl = mask_first_column(previous_mask);
    const int pr = mask_last_column(previous_mask);
    const int cl = mask_first_column(current_mask);
    const int cr = mask_last_column(current_mask);

    const int dl = cl - pl;
    const int dr = cr - pr;

    if (dl > 0)
        *out_direction = dr > 0 ? FLOW_RIGHT : FLOW_INWARD;
    else if (dl < 0)
        *out_direction = dr < 0 ? FLOW_LEFT : FLOW_OUTWARD;
    else if (dr < 0)
        *out_direction = FLOW_INWARD;
    else if (dr > 0)
        *out_direction = FLOW_OUTWARD;
    else
        *out_direction = FLOW_NONE;

    *out_roll =
        pl > cr ||
        pr < cl;
}


static int detect_stream(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 5)
        return 0;

    for (size_t i = 0; i < 5; ++i)
    {
        if (
            frames[i].active_count != 1 ||
            frames[i].jack_count != 0
        )
        {
            return 0;
        }
    }

    if (
        mask_first_column(frames[0].raw_mask) !=
        mask_first_column(frames[4].raw_mask)
    )
    {
        return 5;
    }

    return 0;
}


static int detect_anchor_burst(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count == 0)
        return 0;

    return
        frames[0].jack_count > 1 &&
        frames[0].ms_per_beat < 2000.0
            ? 1
            : 0;
}


static int detect_harmonic_flow(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 4)
        return 0;

    const BeatFrame *a = &frames[0];
    const BeatFrame *b = &frames[1];
    const BeatFrame *c = &frames[2];
    const BeatFrame *d = &frames[3];

    if (
        a->active_count > 1 &&
        a->jack_count == 0 &&
        b->jack_count == 0 &&
        c->jack_count == 0 &&
        d->jack_count == 0
    )
    {
        if (
            b->active_count > 1 ||
            c->active_count > 1 ||
            d->active_count > 1
        )
        {
            return 4;
        }
    }

    return 0;
}


static int sub_long_jacks(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 5)
        return 0;

    for (size_t i = 0; i < 5; ++i)
    {
        if (frames[i].jack_count <= 0)
            return 0;
    }

    uint8_t common = frames[0].raw_mask;

    for (size_t i = 1; i < 5; ++i)
        common &= frames[i].raw_mask;

    return common != 0 ? 5 : 0;
}


static int sub_chord_jacks(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 2)
        return 0;

    const BeatFrame *a = &frames[0];
    const BeatFrame *b = &frames[1];

    if (
        a->active_count > 2 &&
        b->active_count > 1 &&
        b->jack_count >= 1
    )
    {
        if (
            b->active_count < a->active_count ||
            b->jack_count < b->active_count
        )
        {
            return 2;
        }
    }

    return 0;
}


static int sub_mini_jacks(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 2)
        return 0;

    return
        frames[0].jack_count > 0 &&
        frames[1].jack_count == 0
            ? 2
            : 0;
}


static int sub_handstream(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 4)
        return 0;

    return
        frames[0].active_count == 3 &&
        frames[0].jack_count == 0 &&
        frames[1].jack_count == 0 &&
        frames[2].jack_count == 0 &&
        frames[3].jack_count == 0
            ? 4
            : 0;
}


static int sub_jumpstream(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 4)
        return 0;

    if (
        frames[0].active_count == 2 &&
        frames[0].jack_count == 0 &&
        frames[1].active_count == 1 &&
        frames[1].jack_count == 0 &&
        frames[2].jack_count == 0 &&
        frames[3].jack_count == 0
    )
    {
        if (
            frames[2].active_count < 3 &&
            frames[3].active_count < 3
        )
        {
            return 4;
        }
    }

    return 0;
}


static int sub_jump_trill(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 4)
        return 0;

    return
        frames[0].active_count == 2 &&
        frames[1].active_count == 2 &&
        frames[2].active_count == 2 &&
        frames[3].active_count == 2 &&
        frames[1].is_roll &&
        frames[2].is_roll &&
        frames[3].is_roll
            ? 4
            : 0;
}


static int sub_split_trill(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 3)
        return 0;

    return
        frames[0].active_count == 2 &&
        frames[1].active_count == 2 &&
        frames[2].active_count == 2 &&
        frames[1].jack_count == 0 &&
        frames[2].jack_count == 0 &&
        !frames[1].is_roll &&
        !frames[2].is_roll
            ? 3
            : 0;
}


static int sub_gluts(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 3)
        return 0;

    if (
        frames[1].jack_count == 1 &&
        frames[2].jack_count == 1
    )
    {
        const uint8_t common =
            frames[0].raw_mask &
            frames[1].raw_mask &
            frames[2].raw_mask;

        return common == 0 ? 3 : 0;
    }

    return 0;
}


static int sub_quad_stream(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 4)
        return 0;

    return
        frames[0].active_count == 4 &&
        frames[2].jack_count == 0 &&
        frames[3].jack_count == 0
            ? 4
            : 0;
}


static int sub_roll(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 3)
        return 0;

    if (
        frames[0].active_count == 1 &&
        frames[1].active_count == 1 &&
        frames[2].active_count == 1
    )
    {
        const bool left =
            frames[0].direction == FLOW_LEFT &&
            frames[1].direction == FLOW_LEFT &&
            frames[2].direction == FLOW_LEFT;

        const bool right =
            frames[0].direction == FLOW_RIGHT &&
            frames[1].direction == FLOW_RIGHT &&
            frames[2].direction == FLOW_RIGHT;

        if (left || right)
            return 3;
    }

    return 0;
}


static int sub_trill(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 4)
        return 0;

    if (
        frames[1].jack_count == 0 &&
        frames[2].jack_count == 0 &&
        frames[3].jack_count == 0 &&
        frames[0].raw_mask == frames[2].raw_mask &&
        frames[1].raw_mask == frames[3].raw_mask
    )
    {
        return 4;
    }

    return 0;
}


static int sub_mini_trill(
    const BeatFrame *frames,
    size_t count
)
{
    if (!frames || count < 4)
        return 0;

    if (
        frames[1].jack_count == 0 &&
        frames[2].jack_count == 0 &&
        frames[0].raw_mask == frames[2].raw_mask &&
        frames[1].raw_mask != frames[3].raw_mask
    )
    {
        return 4;
    }

    return 0;
}


static void best_subtype_for(
    RhythmKind rhythm,
    const BeatFrame *frames,
    size_t count,
    int primary_length,
    int *out_length,
    SubTexture *out_subtype
)
{
    if (!out_length || !out_subtype)
        return;

    int best_length = primary_length;
    SubTexture best_subtype = SUB_NONE;

#define TRY_SUB(name, fn) \
    do \
    { \
        const int candidate_length = fn(frames, count); \
        if (candidate_length > best_length) \
        { \
            best_length = candidate_length; \
            best_subtype = name; \
        } \
    } while (0)

    switch (rhythm)
    {
        case RHYTHM_LINEAR_STREAM:
            TRY_SUB(SUB_ROLLS, sub_roll);
            TRY_SUB(SUB_TRILLS, sub_trill);
            TRY_SUB(SUB_MINI_TRILLS, sub_mini_trill);
            break;

        case RHYTHM_HARMONIC_FLOW:
            TRY_SUB(SUB_HANDSTREAM, sub_handstream);
            TRY_SUB(SUB_SPLIT_TRILL, sub_split_trill);
            TRY_SUB(SUB_JUMP_TRILL, sub_jump_trill);
            TRY_SUB(SUB_JUMPSTREAM, sub_jumpstream);
            break;

        case RHYTHM_ANCHOR_BURST:
            TRY_SUB(SUB_LONG_JACKS, sub_long_jacks);
            TRY_SUB(SUB_QUAD_STREAM, sub_quad_stream);
            TRY_SUB(SUB_GLUTS, sub_gluts);
            TRY_SUB(SUB_CHORD_JACKS, sub_chord_jacks);
            TRY_SUB(SUB_MINI_JACKS, sub_mini_jacks);
            break;

        default:
            break;
    }

#undef TRY_SUB

    *out_length = best_length;
    *out_subtype = best_subtype;
}


static bool build_rows(
    const Beatmap *beatmap,
    RhythmRow **out_rows,
    size_t *out_count
)
{
    if (!beatmap || !out_rows || !out_count)
        return false;

    *out_rows = NULL;
    *out_count = 0;

    if (!beatmap->notes || beatmap->note_count == 0)
        return false;

    const size_t max_events =
        beatmap->note_count +
        beatmap->hold_count;

    RhythmEvent *events = calloc(
        max_events,
        sizeof(*events)
    );

    if (!events)
        return false;

    size_t event_count = 0;

    for (size_t i = 0; i < beatmap->note_count; ++i)
    {
        const ManiaNote *note = &beatmap->notes[i];

        if (note->column >= 4)
            continue;

        events[event_count++] = (RhythmEvent)
        {
            .time_ms = note->start_ms,
            .column = note->column
        };

        if (
            note->type == MANIA_NOTE_HOLD &&
            note->end_ms > note->start_ms
        )
        {
            events[event_count++] = (RhythmEvent)
            {
                .time_ms = note->end_ms,
                .column = note->column
            };
        }
    }

    if (event_count == 0)
    {
        free(events);
        return false;
    }

    qsort(
        events,
        event_count,
        sizeof(*events),
        event_compare
    );

    RhythmRow *rows = calloc(
        event_count,
        sizeof(*rows)
    );

    if (!rows)
    {
        free(events);
        return false;
    }

    size_t row_count = 0;
    bool have_previous_event = false;
    RhythmEvent previous_event = {0};

    for (size_t i = 0; i < event_count; ++i)
    {
        const RhythmEvent event = events[i];

        if (
            have_previous_event &&
            event.time_ms == previous_event.time_ms &&
            event.column == previous_event.column
        )
        {
            continue;
        }

        previous_event = event;
        have_previous_event = true;

        if (
            row_count == 0 ||
            rows[row_count - 1].time_ms != event.time_ms
        )
        {
            rows[row_count++] = (RhythmRow)
            {
                .time_ms = event.time_ms,
                .mask = 0
            };
        }

        rows[row_count - 1].mask |=
            (uint8_t)(1u << event.column);
    }

    free(events);

    if (row_count == 0)
    {
        free(rows);
        return false;
    }

    *out_rows = rows;
    *out_count = row_count;

    return true;
}


static bool build_frames(
    const RhythmRow *rows,
    size_t row_count,
    BeatFrame **out_frames,
    size_t *out_count
)
{
    if (!rows || row_count < 2 || !out_frames || !out_count)
        return false;

    *out_frames = NULL;
    *out_count = 0;

    BeatFrame *frames = calloc(
        row_count - 1,
        sizeof(*frames)
    );

    if (!frames)
        return false;

    const double first_time = rows[0].time_ms;

    uint8_t previous_mask = rows[0].mask;
    double previous_time = rows[0].time_ms;

    size_t frame_count = 0;

    for (size_t i = 1; i < row_count; ++i)
    {
        const RhythmRow *row = &rows[i];

        if (row->mask == 0)
            continue;

        FlowDirection direction = FLOW_NONE;
        bool is_roll = false;

        detect_flow(
            previous_mask,
            row->mask,
            &direction,
            &is_roll
        );

        frames[frame_count++] = (BeatFrame)
        {
            .offset_ms = row->time_ms - first_time,
            .ms_per_beat = (row->time_ms - previous_time) * 4.0,
            .beat_length = MANIA4K_CALIBRATION.rhythm_profile.default_beat_length_ms,
            .raw_mask = row->mask,
            .active_count = mask_popcount(row->mask),
            .jack_count = mask_popcount(row->mask & previous_mask),
            .direction = direction,
            .is_roll = is_roll
        };

        previous_mask = row->mask;
        previous_time = row->time_ms;
    }

    if (frame_count == 0)
    {
        free(frames);
        return false;
    }

    *out_frames = frames;
    *out_count = frame_count;

    return true;
}


static bool extract_textures(
    const BeatFrame *frames,
    size_t frame_count,
    double total_ms,
    Texture **out_textures,
    size_t *out_count
)
{
    if (!frames || frame_count == 0 || !out_textures || !out_count)
        return false;

    *out_textures = NULL;
    *out_count = 0;

    const size_t capacity = frame_count * RHYTHM_COUNT;

    Texture *textures = calloc(
        capacity,
        sizeof(*textures)
    );

    if (!textures)
        return false;

    size_t texture_count = 0;

    for (size_t start = 0; start < frame_count; ++start)
    {
        const BeatFrame *remaining = &frames[start];
        const size_t remaining_count = frame_count - start;

        for (RhythmKind rhythm = 0; rhythm < RHYTHM_COUNT; ++rhythm)
        {
            int matched_length = 0;

            switch (rhythm)
            {
                case RHYTHM_LINEAR_STREAM:
                    matched_length = detect_stream(
                        remaining,
                        remaining_count
                    );
                    break;

                case RHYTHM_HARMONIC_FLOW:
                    matched_length = detect_harmonic_flow(
                        remaining,
                        remaining_count
                    );
                    break;

                case RHYTHM_ANCHOR_BURST:
                    matched_length = detect_anchor_burst(
                        remaining,
                        remaining_count
                    );
                    break;

                case RHYTHM_COORDINATION:
                case RHYTHM_DENSITY:
                case RHYTHM_WILDCARD:
                default:
                    matched_length = 0;
                    break;
            }

            if (matched_length <= 0)
                continue;

            SubTexture subtype = SUB_NONE;

            best_subtype_for(
                rhythm,
                remaining,
                remaining_count,
                matched_length,
                &matched_length,
                &subtype
            );

            if (matched_length <= 0)
                continue;

            double mean_mpb = 0.0;

            for (int i = 0; i < matched_length; ++i)
                mean_mpb += remaining[i].ms_per_beat;

            mean_mpb /= (double)matched_length;

            bool is_volatile = false;

            for (int i = 0; i < matched_length; ++i)
            {
                if (
                    fabs(
                        remaining[i].ms_per_beat -
                        mean_mpb
                    ) >= MANIA4K_CALIBRATION.rhythm_profile.stability_threshold_ms
                )
                {
                    is_volatile = true;
                    break;
                }
            }

            const double start_ms = remaining[0].offset_ms;

            const double end_ms =
                start + (size_t)matched_length < frame_count
                    ? frames[start + (size_t)matched_length].offset_ms
                    : total_ms;

            if (texture_count >= capacity)
            {
                free(textures);
                return false;
            }

            textures[texture_count++] = (Texture)
            {
                .rhythm = rhythm,
                .subtype = subtype,
                .is_volatile = is_volatile,
                .start_ms = start_ms,
                .end_ms = end_ms,
                .ms_per_beat = mean_mpb,
                .cluster_index = 0
            };
        }
    }

    if (texture_count == 0)
    {
        free(textures);
        return true;
    }

    *out_textures = textures;
    *out_count = texture_count;

    return true;
}


static bool cluster_textures(
    Texture *textures,
    size_t texture_count,
    TextureCluster **out_clusters,
    size_t *out_cluster_count
)
{
    if (!out_clusters || !out_cluster_count)
        return false;

    *out_clusters = NULL;
    *out_cluster_count = 0;

    if (!textures || texture_count == 0)
        return true;

    TextureCluster *clusters = calloc(
        texture_count,
        sizeof(*clusters)
    );

    if (!clusters)
        return false;

    size_t cluster_count = 0;
    size_t volatile_cluster[RHYTHM_COUNT];

    for (size_t i = 0; i < RHYTHM_COUNT; ++i)
        volatile_cluster[i] = SIZE_MAX;

    for (size_t i = 0; i < texture_count; ++i)
    {
        Texture *texture = &textures[i];
        size_t cluster_index = SIZE_MAX;

        if (texture->is_volatile)
        {
            cluster_index = volatile_cluster[texture->rhythm];

            if (cluster_index == SIZE_MAX)
            {
                cluster_index = cluster_count++;
                volatile_cluster[texture->rhythm] = cluster_index;

                clusters[cluster_index] = (TextureCluster)
                {
                    .is_volatile = true,
                    .rhythm = texture->rhythm,
                    .anchor_mpb = texture->ms_per_beat,
                    .sum_mpb = 0.0,
                    .count = 0,
                    .bpm = 0
                };
            }
        }
        else
        {
            for (size_t c = 0; c < cluster_count; ++c)
            {
                if (clusters[c].is_volatile)
                    continue;

                if (
                    fabs(
                        clusters[c].anchor_mpb -
                        texture->ms_per_beat
                    ) < MANIA4K_CALIBRATION.rhythm_profile.bpm_cluster_tolerance_ms
                )
                {
                    cluster_index = c;
                    break;
                }
            }

            if (cluster_index == SIZE_MAX)
            {
                cluster_index = cluster_count++;

                clusters[cluster_index] = (TextureCluster)
                {
                    .is_volatile = false,
                    .rhythm = texture->rhythm,
                    .anchor_mpb = texture->ms_per_beat,
                    .sum_mpb = 0.0,
                    .count = 0,
                    .bpm = 0
                };
            }
        }

        clusters[cluster_index].sum_mpb +=
            texture->ms_per_beat;

        clusters[cluster_index].count++;

        texture->cluster_index = cluster_index;
    }

    for (size_t i = 0; i < cluster_count; ++i)
    {
        const double average =
            clusters[i].count > 0
                ? clusters[i].sum_mpb /
                  (double)clusters[i].count
                : 0.0;

        clusters[i].bpm =
            average > 0.0
                ? (long)floor(60000.0 / average + 0.5)
                : 0;
    }

    *out_clusters = clusters;
    *out_cluster_count = cluster_count;

    return true;
}


static void stable_sort_profiles_by_importance(
    RhythmProfile *profiles,
    size_t count
)
{
    if (!profiles)
        return;

    for (size_t i = 1; i < count; ++i)
    {
        RhythmProfile key = profiles[i];
        size_t j = i;

        while (
            j > 0 &&
            profiles[j - 1].importance < key.importance
        )
        {
            profiles[j] = profiles[j - 1];
            j--;
        }

        profiles[j] = key;
    }
}


static bool build_profiles(
    const Texture *textures,
    size_t texture_count,
    const TextureCluster *clusters,
    RhythmProfile **out_profiles,
    size_t *out_count
)
{
    if (!out_profiles || !out_count)
        return false;

    *out_profiles = NULL;
    *out_count = 0;

    if (!textures || texture_count == 0)
        return true;

    RhythmProfile *profiles = calloc(
        texture_count,
        sizeof(*profiles)
    );

    if (!profiles)
        return false;

    size_t profile_count = 0;

    for (size_t i = 0; i < texture_count; ++i)
    {
        const Texture *texture = &textures[i];
        const TextureCluster *cluster =
            &clusters[texture->cluster_index];

        size_t profile_index = SIZE_MAX;

        for (size_t p = 0; p < profile_count; ++p)
        {
            if (
                profiles[p].rhythm == texture->rhythm &&
                profiles[p].is_volatile == texture->is_volatile &&
                profiles[p].bpm == cluster->bpm
            )
            {
                profile_index = p;
                break;
            }
        }

        if (profile_index == SIZE_MAX)
        {
            profile_index = profile_count++;

            profiles[profile_index].rhythm = texture->rhythm;
            profiles[profile_index].is_volatile = texture->is_volatile;
            profiles[profile_index].bpm = cluster->bpm;
            profiles[profile_index].weight = rhythm_weight(texture->rhythm);
            profiles[profile_index].dominant_sub = SUB_NONE;

            for (size_t s = 0; s < SUB_COUNT; ++s)
                profiles[profile_index].subtype_first_seen[s] = SIZE_MAX;
        }

        if (
            !append_profile_item(
                &profiles[profile_index],
                (ProfileItem)
                {
                    .start_ms = texture->start_ms,
                    .end_ms = texture->end_ms,
                    .subtype = texture->subtype
                }
            )
        )
        {
            free_profiles(profiles, profile_count);
            return false;
        }
    }

    for (size_t p = 0; p < profile_count; ++p)
    {
        RhythmProfile *profile = &profiles[p];

        qsort(
            profile->items,
            profile->item_count,
            sizeof(*profile->items),
            profile_item_compare
        );

        double total_duration = 0.0;

        if (profile->item_count > 0)
        {
            double current_start = profile->items[0].start_ms;
            double current_end = profile->items[0].end_ms;

            for (size_t i = 1; i < profile->item_count; ++i)
            {
                const double start = profile->items[i].start_ms;
                const double end = profile->items[i].end_ms;

                if (current_end < end)
                {
                    total_duration +=
                        current_end - current_start;

                    current_start = start;
                    current_end = end;
                }
                else
                {
                    current_end = max_double(
                        current_end,
                        end
                    );
                }
            }

            total_duration +=
                current_end - current_start;
        }

        profile->coverage_ms = total_duration;

        size_t best_count = 0;
        size_t best_seen = SIZE_MAX;
        SubTexture best_sub = SUB_NONE;

        for (SubTexture sub = SUB_ROLLS; sub < SUB_COUNT; ++sub)
        {
            const size_t count = profile->subtype_counts[sub];

            if (count == 0)
                continue;

            const size_t seen = profile->subtype_first_seen[sub];

            if (
                count > best_count ||
                (
                    count == best_count &&
                    seen < best_seen
                )
            )
            {
                best_count = count;
                best_seen = seen;
                best_sub = sub;
            }
        }

        profile->dominant_sub = best_sub;

        profile->importance =
            profile->coverage_ms *
            profile->weight *
            max_double(
                1.0,
                (double)profile->bpm
            );
    }

    stable_sort_profiles_by_importance(
        profiles,
        profile_count
    );

    RhythmProfile *filtered = calloc(
        profile_count,
        sizeof(*filtered)
    );

    if (!filtered)
    {
        free_profiles(profiles, profile_count);
        return false;
    }

    size_t filtered_count = 0;

    for (size_t i = 0; i < profile_count; ++i)
    {
        bool dominated = false;

        for (size_t j = 0; j < profile_count; ++j)
        {
            if (
                profiles[j].rhythm == profiles[i].rhythm &&
                profiles[j].coverage_ms * 0.5 >
                    profiles[i].coverage_ms &&
                profiles[j].bpm > profiles[i].bpm
            )
            {
                dominated = true;
                break;
            }
        }

        if (!dominated)
        {
            filtered[filtered_count++] = profiles[i];
            profiles[i].items = NULL;
            profiles[i].item_count = 0;
            profiles[i].item_capacity = 0;
        }
    }

    free_profiles(profiles, profile_count);

    RhythmProfile *capped = calloc(
        filtered_count,
        sizeof(*capped)
    );

    if (!capped)
    {
        free_profiles(filtered, filtered_count);
        return false;
    }

    size_t rhythm_counts[RHYTHM_COUNT] = {0};
    size_t capped_count = 0;

    for (size_t i = 0; i < filtered_count; ++i)
    {
        const RhythmKind rhythm = filtered[i].rhythm;

        if (rhythm_counts[rhythm] < 3)
        {
            capped[capped_count++] = filtered[i];
            filtered[i].items = NULL;
            filtered[i].item_count = 0;
            filtered[i].item_capacity = 0;

            rhythm_counts[rhythm]++;
        }
    }

    free_profiles(filtered, filtered_count);

    stable_sort_profiles_by_importance(
        capped,
        capped_count
    );

    *out_profiles = capped;
    *out_count = capped_count;

    return true;
}


static double *family_score_pointer(
    RhythmProfileResult *result,
    ChartFamily family
)
{
    if (!result)
        return NULL;

    switch (family)
    {
        case CHART_FAMILY_STREAM:
            return &result->stream_score;

        case CHART_FAMILY_JACK:
            return &result->jack_score;

        case CHART_FAMILY_TECH:
            return &result->tech_score;

        case CHART_FAMILY_SPEED:
            return &result->speed_score;

        case CHART_FAMILY_STAMINA:
            return &result->stamina_score;

        case CHART_FAMILY_HYBRID:
        default:
            return NULL;
    }
}


static void normalize_family_scores(
    RhythmProfileResult *result
)
{
    if (!result)
        return;

    double *scores[] =
    {
        &result->jack_score,
        &result->speed_score,
        &result->stamina_score,
        &result->tech_score,
        &result->stream_score
    };

    double max_score = 0.0;

    for (size_t i = 0; i < 5; ++i)
        max_score = max_double(max_score, *scores[i]);

    if (max_score <= 0.0)
        max_score = 1.0;

    for (size_t i = 0; i < 5; ++i)
    {
        *scores[i] = round_places(
            *scores[i] /
            max_score *
            100.0,
            10.0
        );
    }
}


bool RhythmProfileEvaluate(
    const Beatmap *beatmap,
    RhythmProfileResult *out_result
)
{
    if (!beatmap || !out_result)
        return false;

    memset(out_result, 0, sizeof(*out_result));

    out_result->family = CHART_FAMILY_HYBRID;

    snprintf(
        out_result->primary_rhythm,
        sizeof(out_result->primary_rhythm),
        "%s",
        "none"
    );

    snprintf(
        out_result->subtype,
        sizeof(out_result->subtype),
        "%s",
        "generic"
    );

    RhythmRow *rows = NULL;
    size_t row_count = 0;

    BeatFrame *frames = NULL;
    size_t frame_count = 0;

    Texture *textures = NULL;
    size_t texture_count = 0;

    TextureCluster *clusters = NULL;
    size_t cluster_count = 0;

    RhythmProfile *profiles = NULL;
    size_t profile_count = 0;

    bool success = false;

    if (!build_rows(beatmap, &rows, &row_count))
        goto cleanup;

    if (row_count < 2)
    {
        success = true;
        goto cleanup;
    }

    if (!build_frames(rows, row_count, &frames, &frame_count))
        goto cleanup;

    double total_ms =
        rows[row_count - 1].time_ms -
        rows[0].time_ms;

    if (total_ms <= 0.0)
        total_ms = 60000.0;

    if (
        !extract_textures(
            frames,
            frame_count,
            total_ms,
            &textures,
            &texture_count
        )
    )
    {
        goto cleanup;
    }

    out_result->texture_count = texture_count;

    if (texture_count == 0)
    {
        success = true;
        goto cleanup;
    }

    if (
        !cluster_textures(
            textures,
            texture_count,
            &clusters,
            &cluster_count
        )
    )
    {
        goto cleanup;
    }

    (void)cluster_count;

    if (
        !build_profiles(
            textures,
            texture_count,
            clusters,
            &profiles,
            &profile_count
        )
    )
    {
        goto cleanup;
    }

    out_result->profile_count = profile_count;

    if (profile_count == 0)
    {
        success = true;
        goto cleanup;
    }

    const double top_importance =
        profiles[0].importance;

    size_t important_indices[5];
    size_t important_count = 0;

    for (size_t i = 0; i < profile_count; ++i)
    {
        const double denominator =
            max_double(top_importance, 1.0);

        if (
            profiles[i].importance /
            denominator >
            MANIA4K_CALIBRATION.rhythm_profile.important_cluster_ratio
        )
        {
            if (important_count < 5)
                important_indices[important_count] = i;

            important_count++;
        }
    }

    const RhythmProfile *primary =
        important_count > 0
            ? &profiles[important_indices[0]]
            : &profiles[0];

    const RhythmProfile *secondary =
        important_count > 1
            ? &profiles[important_indices[1]]
            : NULL;

    ChartFamily primary_family =
        subtype_family(
            primary->dominant_sub,
            primary->rhythm
        );

    bool is_hybrid = false;

    if (
        secondary &&
        secondary->dominant_sub != SUB_NONE
    )
    {
        const ChartFamily secondary_family =
            subtype_family(
                secondary->dominant_sub,
                secondary->rhythm
            );

        if (secondary_family != primary_family)
            is_hybrid = true;
    }

    const bool is_tech =
        primary->is_volatile ||
        (
            is_hybrid &&
            primary->bpm < MANIA4K_CALIBRATION.rhythm_profile.hybrid_tech_bpm_ceiling
        ) ||
        subtype_forces_tech(
            primary->dominant_sub
        );

    ChartFamily family;

    if (is_hybrid && is_tech)
        family = CHART_FAMILY_TECH;
    else if (is_hybrid)
        family = CHART_FAMILY_HYBRID;
    else if (is_tech)
        family = CHART_FAMILY_TECH;
    else
        family = primary_family;

    const double drain_s =
        total_ms /
        1000.0;

    if (
        (
            family == CHART_FAMILY_SPEED ||
            family == CHART_FAMILY_STREAM
        ) &&
        beatmap->bpm > MANIA4K_CALIBRATION.rhythm_profile.stamina_bpm_floor &&
        drain_s > MANIA4K_CALIBRATION.rhythm_profile.stamina_duration_floor
    )
    {
        family = CHART_FAMILY_STAMINA;

        if (
            primary_family == CHART_FAMILY_SPEED ||
            primary_family == CHART_FAMILY_STREAM
        )
        {
            primary_family = CHART_FAMILY_STAMINA;
        }
    }

    double total_importance = 1.0;

    for (
        size_t i = 0;
        i < profile_count && i < 5;
        ++i
    )
    {
        total_importance +=
            profiles[i].importance;
    }

    out_result->confidence =
        min_double(
            1.0,
            round_places(
                primary->importance /
                total_importance,
                100.0
            )
        );

    const size_t score_count =
        min_double(
            (double)important_count,
            5.0
        );

    for (size_t i = 0; i < score_count; ++i)
    {
        const RhythmProfile *profile =
            &profiles[important_indices[i]];

        const ChartFamily score_family =
            subtype_family(
                profile->dominant_sub,
                profile->rhythm
            );

        double *score =
            family_score_pointer(
                out_result,
                score_family
            );

        if (score)
        {
            *score +=
                profile->importance /
                max_double(total_importance, 1.0) *
                100.0;
        }
    }

    normalize_family_scores(out_result);

    out_result->family = family;
    out_result->primary_bpm = (double)primary->bpm;
    out_result->primary_volatile = primary->is_volatile;

    snprintf(
        out_result->primary_rhythm,
        sizeof(out_result->primary_rhythm),
        "%s",
        rhythm_name(primary->rhythm)
    );

    snprintf(
        out_result->subtype,
        sizeof(out_result->subtype),
        "%s",
        subtype_name(primary->dominant_sub)
    );

    success = true;

cleanup:
    free(rows);
    free(frames);
    free(textures);
    free(clusters);
    free_profiles(profiles, profile_count);

    return success;
}


ReformRuler RhythmProfileRuler(
    const RhythmProfileResult *profile,
    bool *out_uses_skillset
)
{
    if (out_uses_skillset)
        *out_uses_skillset = false;

    if (!profile || profile->confidence < MANIA4K_CALIBRATION.rhythm_profile.ruler_confidence_floor)
        return REFORM_RULER_GENERAL;

    ReformRuler ruler;

    switch (profile->family)
    {
        case CHART_FAMILY_JACK:
            ruler = REFORM_RULER_JACK;
            break;

        case CHART_FAMILY_SPEED:
        case CHART_FAMILY_STREAM:
            ruler = REFORM_RULER_SPEED;
            break;

        case CHART_FAMILY_STAMINA:
            ruler = REFORM_RULER_STAMINA;
            break;

        case CHART_FAMILY_TECH:
            ruler = REFORM_RULER_TECH;
            break;

        case CHART_FAMILY_HYBRID:
        default:
            return REFORM_RULER_GENERAL;
    }

    if (out_uses_skillset)
        *out_uses_skillset = true;

    return ruler;
}
