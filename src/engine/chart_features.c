#include "chart_features.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FEATURE_KEYS 4
#define NPS_WINDOW_MS 500.0
#define NPS_STRIDE_MS 250.0
#define ENTROPY_WINDOW_MS 2000.0
#define ENTROPY_STRIDE_MS 1000.0

typedef struct
{
    double time_ms;
    int column;
    ManiaNoteType type;
    double end_ms;
} FeatureNote;

typedef struct
{
    double time_ms;
    uint8_t mask;
    uint8_t size;
} FeatureRow;

typedef struct
{
    double start_ms;
    double end_ms;
} HoldInterval;

static int compare_double(const void *a, const void *b)
{
    const double da = *(const double *)a;
    const double db = *(const double *)b;
    return (da > db) - (da < db);
}

static int compare_note(const void *a, const void *b)
{
    const FeatureNote *na = (const FeatureNote *)a;
    const FeatureNote *nb = (const FeatureNote *)b;

    if (na->time_ms < nb->time_ms) return -1;
    if (na->time_ms > nb->time_ms) return 1;
    return na->column - nb->column;
}

static int compare_interval_start(const void *a, const void *b)
{
    const HoldInterval *ia = (const HoldInterval *)a;
    const HoldInterval *ib = (const HoldInterval *)b;

    if (ia->start_ms < ib->start_ms) return -1;
    if (ia->start_ms > ib->start_ms) return 1;
    if (ia->end_ms < ib->end_ms) return -1;
    if (ia->end_ms > ib->end_ms) return 1;
    return 0;
}

static double round_to(double value, int digits)
{
    const double scale = pow(10.0, (double)digits);
    return round(value * scale) / scale;
}

static double coefficient_of_variation(
    const double *values,
    size_t count
)
{
    if (!values || count < 2)
        return 0.0;

    double sum = 0.0;
    for (size_t i = 0; i < count; ++i)
        sum += values[i];

    const double mean = sum / (double)count;
    if (mean < 1e-6)
        return 0.0;

    double variance = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        const double d = values[i] - mean;
        variance += d * d;
    }

    variance /= (double)count;
    return sqrt(variance) / mean;
}

static int bit_count_4(uint8_t mask)
{
    int count = 0;
    for (int i = 0; i < FEATURE_KEYS; ++i)
    {
        if (mask & (uint8_t)(1u << i))
            ++count;
    }
    return count;
}

static bool build_sorted_notes(
    const AnalysisMap *map,
    FeatureNote **out_notes
)
{
    if (!map || !out_notes || !map->notes || map->note_count == 0)
        return false;

    FeatureNote *notes = malloc(map->note_count * sizeof(FeatureNote));
    if (!notes)
        return false;

    for (size_t i = 0; i < map->note_count; ++i)
    {
        const ManiaNote *src = &map->notes[i];

        if (src->column >= FEATURE_KEYS)
        {
            free(notes);
            return false;
        }

        notes[i].time_ms = src->start_ms;
        notes[i].column = (int)src->column;
        notes[i].type = src->type;
        notes[i].end_ms = src->end_ms;
    }

    qsort(notes, map->note_count, sizeof(FeatureNote), compare_note);
    *out_notes = notes;
    return true;
}

static bool build_rows(
    const FeatureNote *notes,
    size_t note_count,
    FeatureRow **out_rows,
    size_t *out_row_count
)
{
    if (!notes || note_count == 0 || !out_rows || !out_row_count)
        return false;

    FeatureRow *rows = calloc(note_count, sizeof(FeatureRow));
    if (!rows)
        return false;

    size_t row_count = 0;
    size_t i = 0;

    while (i < note_count)
    {
        const double t = notes[i].time_ms;
        uint8_t mask = 0;
        size_t j = i;

        while (j < note_count && notes[j].time_ms == t)
        {
            if (notes[j].column >= 0 && notes[j].column < FEATURE_KEYS)
                mask |= (uint8_t)(1u << notes[j].column);
            ++j;
        }

        rows[row_count].time_ms = t;
        rows[row_count].mask = mask;
        rows[row_count].size = (uint8_t)bit_count_4(mask);
        ++row_count;

        i = j;
    }

    FeatureRow *shrunk = realloc(rows, row_count * sizeof(FeatureRow));
    *out_rows = shrunk ? shrunk : rows;
    *out_row_count = row_count;
    return true;
}

static bool build_nps_data(
    const FeatureNote *notes,
    size_t note_count,
    NpsPoint **out_points,
    size_t *out_count
)
{
    if (!notes || note_count == 0 || !out_points || !out_count)
        return false;

    const double t_min = notes[0].time_ms;
    const double t_max = notes[note_count - 1].time_ms;

    size_t capacity =
        (size_t)floor((t_max - t_min) / NPS_STRIDE_MS) + 2;

    if (capacity < 1)
        capacity = 1;

    NpsPoint *points = malloc(capacity * sizeof(NpsPoint));
    if (!points)
        return false;

    size_t count = 0;
    size_t left = 0;
    size_t right = 0;

    for (double t = t_min; t <= t_max; t += NPS_STRIDE_MS)
    {
        const double lo = t - NPS_WINDOW_MS * 0.5;
        const double hi = t + NPS_WINDOW_MS * 0.5;

        while (left < note_count && notes[left].time_ms < lo)
            ++left;

        if (right < left)
            right = left;

        while (right < note_count && notes[right].time_ms <= hi)
            ++right;

        if (count >= capacity)
        {
            capacity *= 2;
            NpsPoint *grown = realloc(points, capacity * sizeof(NpsPoint));
            if (!grown)
            {
                free(points);
                return false;
            }
            points = grown;
        }

        const size_t window_notes = right - left;
        points[count].time_ms = t;
        points[count].nps = (double)window_notes / (NPS_WINDOW_MS / 1000.0);
        ++count;
    }

    *out_points = points;
    *out_count = count;
    return true;
}

static bool build_display_nps_curve(
    const NpsPoint *points,
    size_t count,
    NpsPoint **out_curve,
    size_t *out_curve_count
)
{
    if (!out_curve || !out_curve_count)
        return false;

    *out_curve = NULL;
    *out_curve_count = 0;

    if (!points || count == 0)
        return true;

    NpsPoint *curve = malloc(count * sizeof(NpsPoint));
    if (!curve)
        return false;

    memcpy(
        curve,
        points,
        count * sizeof(NpsPoint)
    );

    *out_curve = curve;
    *out_curve_count = count;
    return true;
}

static double compute_pattern_irregularity(
    const FeatureNote *notes,
    size_t note_count
)
{
    if (!notes || note_count == 0)
        return 0.0;

    const double t_min = notes[0].time_ms;
    const double t_max = notes[note_count - 1].time_ms;

    size_t capacity =
        (size_t)floor((t_max - t_min) / ENTROPY_STRIDE_MS) + 2;

    if (capacity < 2)
        capacity = 2;

    double *entropies = malloc(capacity * sizeof(double));
    if (!entropies)
        return 0.0;

    size_t entropy_count = 0;
    size_t start_index = 0;

    for (double t = t_min; t <= t_max; t += ENTROPY_STRIDE_MS)
    {
        const double hi = t + ENTROPY_WINDOW_MS;

        while (start_index < note_count && notes[start_index].time_ms < t)
            ++start_index;

        int column_counts[FEATURE_KEYS] = {0, 0, 0, 0};
        int total = 0;

        for (size_t j = start_index; j < note_count; ++j)
        {
            if (notes[j].time_ms > hi)
                break;

            if (notes[j].column >= 0 && notes[j].column < FEATURE_KEYS)
            {
                ++column_counts[notes[j].column];
                ++total;
            }
        }

        if (total >= 4)
        {
            double entropy = 0.0;

            for (int c = 0; c < FEATURE_KEYS; ++c)
            {
                if (column_counts[c] <= 0)
                    continue;

                const double p = (double)column_counts[c] / (double)total;
                entropy -= p * (log(p) / log(2.0));
            }

            if (entropy_count >= capacity)
            {
                capacity *= 2;
                double *grown = realloc(entropies, capacity * sizeof(double));
                if (!grown)
                {
                    free(entropies);
                    return 0.0;
                }
                entropies = grown;
            }

            entropies[entropy_count++] = entropy;
        }
    }

    const double result = coefficient_of_variation(entropies, entropy_count);
    free(entropies);
    return result;
}

static double compute_chord_complexity(
    const FeatureRow *rows,
    size_t row_count,
    const NpsPoint *nps,
    size_t nps_count,
    double hand_ratio,
    double quad_ratio
)
{
    const double frac_chords_3plus = hand_ratio + quad_ratio;

    if (
        frac_chords_3plus <= 0.0 ||
        !rows || row_count == 0 ||
        !nps || nps_count == 0
    )
    {
        return 0.0;
    }

    double *chord_times = malloc(row_count * sizeof(double));
    if (!chord_times)
        return frac_chords_3plus;

    size_t chord_count = 0;
    for (size_t i = 0; i < row_count; ++i)
    {
        if (rows[i].size >= 3)
            chord_times[chord_count++] = rows[i].time_ms;
    }

    if (chord_count == 0)
    {
        free(chord_times);
        return 0.0;
    }

    double overall_sum = 0.0;
    for (size_t i = 0; i < nps_count; ++i)
        overall_sum += nps[i].nps;

    const double overall_mean = overall_sum / (double)nps_count;

    double chord_nps_sum = 0.0;
    size_t chord_nps_count = 0;
    size_t chord_left = 0;

    for (size_t i = 0; i < nps_count; ++i)
    {
        const double lo = nps[i].time_ms - 250.0;
        const double hi = nps[i].time_ms + 250.0;

        while (chord_left < chord_count && chord_times[chord_left] < lo)
            ++chord_left;

        if (chord_left < chord_count && chord_times[chord_left] <= hi)
        {
            chord_nps_sum += nps[i].nps;
            ++chord_nps_count;
        }
    }

    free(chord_times);

    if (chord_nps_count == 0 || overall_mean <= 0.0)
        return frac_chords_3plus;

    const double chord_mean = chord_nps_sum / (double)chord_nps_count;
    const double density_amp = chord_mean / overall_mean;

    return frac_chords_3plus * (1.0 + density_amp);
}

static void compute_ln_features(
    const FeatureNote *notes,
    size_t note_count,
    double duration_s,
    double first_head_ms,
    double last_head_ms,
    ChartFeatures *features
)
{
    if (!notes || !features || note_count == 0)
        return;

    size_t hold_count = 0;
    for (size_t i = 0; i < note_count; ++i)
    {
        if (notes[i].type == MANIA_NOTE_HOLD)
            ++hold_count;
    }

    features->hold_count = hold_count;
    features->ln_ratio = (double)hold_count / (double)note_count;

    if (hold_count == 0 || duration_s < 1.0)
        return;

    HoldInterval *holds = malloc(hold_count * sizeof(HoldInterval));
    double *durations = malloc(hold_count * sizeof(double));

    if (!holds || !durations)
    {
        free(holds);
        free(durations);
        return;
    }

    size_t h = 0;
    for (size_t i = 0; i < note_count; ++i)
    {
        if (notes[i].type != MANIA_NOTE_HOLD)
            continue;

        holds[h].start_ms = notes[i].time_ms;
        holds[h].end_ms = notes[i].end_ms;
        durations[h] = fmax(0.0, notes[i].end_ms - notes[i].time_ms);
        ++h;
    }

    qsort(holds, hold_count, sizeof(HoldInterval), compare_interval_start);

    double duration_sum = 0.0;
    size_t positive_duration_count = 0;

    for (size_t i = 0; i < hold_count; ++i)
    {
        if (durations[i] > 0.0)
        {
            duration_sum += durations[i];
            ++positive_duration_count;
        }
    }

    if (positive_duration_count > 0)
        features->ln_duration_mean_ms = duration_sum / (double)positive_duration_count;

    if (positive_duration_count >= 2)
    {
        double *positive = malloc(positive_duration_count * sizeof(double));
        if (positive)
        {
            size_t p = 0;
            for (size_t i = 0; i < hold_count; ++i)
            {
                if (durations[i] > 0.0)
                    positive[p++] = durations[i];
            }

            features->ln_duration_cv = coefficient_of_variation(positive, p);
            free(positive);
        }
    }

    const double chart_span_ms = fmax(last_head_ms - first_head_ms, 1.0);

    double occupied_ms = 0.0;
    double current_start = 0.0;
    double current_end = 0.0;
    bool have_interval = false;

    for (size_t i = 0; i < hold_count; ++i)
    {
        const double s = holds[i].start_ms;
        const double e = holds[i].end_ms;

        if (e <= s)
            continue;

        if (!have_interval)
        {
            current_start = s;
            current_end = e;
            have_interval = true;
        }
        else if (s <= current_end)
        {
            if (e > current_end)
                current_end = e;
        }
        else
        {
            occupied_ms += current_end - current_start;
            current_start = s;
            current_end = e;
        }
    }

    if (have_interval)
        occupied_ms += current_end - current_start;

    features->hold_occupancy = fmin(1.0, occupied_ms / chart_span_ms);

    double *active_ends = malloc(hold_count * sizeof(double));
    size_t active_count = 0;
    size_t simultaneous_count = 0;

    if (active_ends)
    {
        for (size_t i = 0; i < hold_count; ++i)
        {
            const double s = holds[i].start_ms;

            size_t write = 0;
            for (size_t a = 0; a < active_count; ++a)
            {
                if (active_ends[a] > s)
                    active_ends[write++] = active_ends[a];
            }
            active_count = write;

            if (active_count > 0)
                ++simultaneous_count;

            active_ends[active_count++] = holds[i].end_ms;
        }

        free(active_ends);
    }

    features->simultaneous_hold =
        (double)simultaneous_count / (double)hold_count;

    features->release_density =
        duration_s > 0.0
        ? (double)hold_count / duration_s
        : 0.0;

    size_t chorded_holds = 0;
    size_t start = 0;

    while (start < hold_count)
    {
        size_t end = start + 1;
        while (end < hold_count && holds[end].start_ms == holds[start].start_ms)
            ++end;

        const size_t group = end - start;
        if (group >= 2)
            chorded_holds += group;

        start = end;
    }

    features->hold_chord_ratio =
        (double)chorded_holds / (double)hold_count;

    free(durations);
    free(holds);
}

void ChartFeaturesInit(
    ChartFeatures *features
)
{
    if (!features)
        return;

    memset(features, 0, sizeof(*features));
}

void ChartFeaturesFree(
    ChartFeatures *features
)
{
    if (!features)
        return;

    free(features->nps_curve);
    ChartFeaturesInit(features);
}

bool ChartFeaturesBuild(
    const AnalysisMap *map,
    ChartFeatures *out_features
)
{
    if (
        !map ||
        !out_features ||
        !map->notes ||
        map->note_count == 0
    )
    {
        return false;
    }

    ChartFeatures candidate;
    ChartFeaturesInit(&candidate);

    FeatureNote *notes = NULL;
    FeatureRow *rows = NULL;
    NpsPoint *nps = NULL;

    size_t row_count = 0;
    size_t nps_count = 0;

    if (!build_sorted_notes(map, &notes))
        goto fail;

    if (!build_rows(notes, map->note_count, &rows, &row_count))
        goto fail;

    if (!build_nps_data(notes, map->note_count, &nps, &nps_count))
        goto fail;

    if (!build_display_nps_curve(
            nps,
            nps_count,
            &candidate.nps_curve,
            &candidate.nps_curve_count
        ))
    {
        goto fail;
    }

    candidate.note_count = map->note_count;
    candidate.row_count = row_count;

    if (row_count > 0)
    {
        size_t singles = 0;
        size_t jumps = 0;
        size_t hands = 0;
        size_t quads = 0;

        for (size_t i = 0; i < row_count; ++i)
        {
            if (rows[i].size == 1) ++singles;
            else if (rows[i].size == 2) ++jumps;
            else if (rows[i].size == 3) ++hands;
            else if (rows[i].size >= 4) ++quads;
        }

        candidate.stream_purity = (double)singles / (double)row_count;
        candidate.jump_ratio = (double)jumps / (double)row_count;
        candidate.hand_ratio = (double)hands / (double)row_count;
        candidate.quad_ratio = (double)quads / (double)row_count;
    }

    size_t column_counts[FEATURE_KEYS] = {0, 0, 0, 0};
    for (size_t i = 0; i < map->note_count; ++i)
        ++column_counts[notes[i].column];

    double *column_times[FEATURE_KEYS] = {NULL, NULL, NULL, NULL};
    size_t column_write[FEATURE_KEYS] = {0, 0, 0, 0};

    for (int c = 0; c < FEATURE_KEYS; ++c)
    {
        if (column_counts[c] > 0)
        {
            column_times[c] = malloc(column_counts[c] * sizeof(double));
            if (!column_times[c])
            {
                for (int k = 0; k < FEATURE_KEYS; ++k)
                    free(column_times[k]);
                goto fail;
            }
        }
    }

    for (size_t i = 0; i < map->note_count; ++i)
    {
        const int c = notes[i].column;
        column_times[c][column_write[c]++] = notes[i].time_ms;
    }

    size_t jack_hits = 0;
    size_t jack_strict_hits = 0;
    size_t vibro_hits = 0;
    size_t minijack_hits = 0;
    size_t total_pairs = 0;

    for (int c = 0; c < FEATURE_KEYS; ++c)
    {
        for (size_t i = 1; i < column_counts[c]; ++i)
        {
            const double gap = column_times[c][i] - column_times[c][i - 1];
            ++total_pairs;

            if (gap <= 80.0)
            {
                ++vibro_hits;
                ++jack_strict_hits;
                ++jack_hits;
            }
            else if (gap <= 120.0)
            {
                ++jack_strict_hits;
                ++jack_hits;
            }
            else if (gap <= 180.0)
            {
                ++jack_hits;
            }

            if (gap >= 100.0 && gap <= 200.0)
                ++minijack_hits;
        }
    }

    for (int c = 0; c < FEATURE_KEYS; ++c)
        free(column_times[c]);

    if (total_pairs > 0)
    {
        candidate.jack_ratio = (double)jack_hits / (double)total_pairs;
        candidate.jack_density = (double)jack_strict_hits / (double)total_pairs;
        candidate.vibro_density = (double)vibro_hits / (double)total_pairs;
        candidate.minijack_ratio = (double)minijack_hits / (double)total_pairs;
    }

    if (map->note_count > 0)
    {
        size_t max_col_count = 0;
        int used_columns = 0;

        for (int c = 0; c < FEATURE_KEYS; ++c)
        {
            if (column_counts[c] > 0)
            {
                ++used_columns;
                if (column_counts[c] > max_col_count)
                    max_col_count = column_counts[c];
            }
        }

        if (used_columns == 0)
            used_columns = FEATURE_KEYS;

        const double expected = (double)map->note_count / (double)used_columns;
        candidate.anchor_ratio = fmax(
            0.0,
            ((double)max_col_count - expected) / (double)map->note_count
        );
    }

    if (row_count >= 2)
    {
        double transition_sum = 0.0;
        size_t transition_count = 0;

        for (size_t i = 1; i < row_count; ++i)
        {
            const uint8_t prev = rows[i - 1].mask;
            const uint8_t curr = rows[i].mask;
            const int union_count = bit_count_4((uint8_t)(prev | curr));
            const int inter_count = bit_count_4((uint8_t)(prev & curr));

            if (union_count > 0)
            {
                transition_sum +=
                    1.0 - (double)inter_count / (double)union_count;
                ++transition_count;
            }
        }

        if (transition_count > 0)
            candidate.transition_var = transition_sum / (double)transition_count;
    }

    if (nps_count > 0)
    {
        double *values = malloc(nps_count * sizeof(double));
        double *sorted = malloc(nps_count * sizeof(double));

        if (!values || !sorted)
        {
            free(values);
            free(sorted);
            goto fail;
        }

        for (size_t i = 0; i < nps_count; ++i)
        {
            values[i] = nps[i].nps;
            sorted[i] = nps[i].nps;
        }

        candidate.density_cv = coefficient_of_variation(values, nps_count);

        qsort(sorted, nps_count, sizeof(double), compare_double);

        size_t p50 = (size_t)(0.50 * (double)nps_count);
        size_t p90 = (size_t)(0.90 * (double)nps_count);
        size_t p95 = (size_t)(0.95 * (double)nps_count);

        if (p50 >= nps_count) p50 = nps_count - 1;
        if (p90 >= nps_count) p90 = nps_count - 1;
        if (p95 >= nps_count) p95 = nps_count - 1;

        candidate.nps_p50 = sorted[p50];
        candidate.nps_p90 = sorted[p90];
        candidate.nps_p95 = sorted[p95];

        const size_t top30_cutoff = (size_t)(0.70 * (double)nps_count);
        double top30_sum = 0.0;
        size_t top30_count = 0;

        for (size_t i = top30_cutoff; i < nps_count; ++i)
        {
            top30_sum += sorted[i];
            ++top30_count;
        }

        if (top30_count > 0)
            candidate.nps_sustained_top30 = top30_sum / (double)top30_count;

        double *active = malloc(nps_count * sizeof(double));
        if (!active)
        {
            free(values);
            free(sorted);
            goto fail;
        }

        size_t active_count = 0;
        for (size_t i = 0; i < nps_count; ++i)
        {
            if (values[i] > 2.0)
                active[active_count++] = values[i];
        }

        candidate.nps_active_ratio =
            (double)active_count / (double)nps_count;

        if (active_count >= 2)
            candidate.nps_active_cv = coefficient_of_variation(active, active_count);

        free(active);
        free(values);
        free(sorted);
    }

    const double first_head_ms = notes[0].time_ms;
    const double last_head_ms = notes[map->note_count - 1].time_ms;
    candidate.duration_s = fmax(0.0, last_head_ms - first_head_ms) / 1000.0;

    if (candidate.duration_s > 0.0 && candidate.nps_p90 > 0.0)
    {
        const double sustained_fraction =
            candidate.nps_sustained_top30 / candidate.nps_p90;

        candidate.stamina_index =
            sustained_fraction *
            log(1.0 + candidate.duration_s / 90.0);
    }

    candidate.burst_ratio =
        candidate.nps_p95 / fmax(candidate.nps_p50, 0.01);

    candidate.pattern_irregularity =
        compute_pattern_irregularity(notes, map->note_count);

    if (row_count >= 3)
    {
        double *gaps = malloc((row_count - 1) * sizeof(double));
        if (!gaps)
            goto fail;

        for (size_t i = 1; i < row_count; ++i)
            gaps[i - 1] = rows[i].time_ms - rows[i - 1].time_ms;

        candidate.timing_irregularity =
            coefficient_of_variation(gaps, row_count - 1);

        free(gaps);
    }

    candidate.chord_complexity =
        compute_chord_complexity(
            rows,
            row_count,
            nps,
            nps_count,
            candidate.hand_ratio,
            candidate.quad_ratio
        );

    compute_ln_features(
        notes,
        map->note_count,
        candidate.duration_s,
        first_head_ms,
        last_head_ms,
        &candidate
    );

    candidate.stream_purity = round_to(candidate.stream_purity, 4);
    candidate.jump_ratio = round_to(candidate.jump_ratio, 4);
    candidate.hand_ratio = round_to(candidate.hand_ratio, 4);
    candidate.quad_ratio = round_to(candidate.quad_ratio, 4);
    candidate.jack_ratio = round_to(candidate.jack_ratio, 4);
    candidate.jack_density = round_to(candidate.jack_density, 4);
    candidate.vibro_density = round_to(candidate.vibro_density, 4);
    candidate.anchor_ratio = round_to(candidate.anchor_ratio, 4);
    candidate.minijack_ratio = round_to(candidate.minijack_ratio, 4);
    candidate.density_cv = round_to(candidate.density_cv, 4);
    candidate.transition_var = round_to(candidate.transition_var, 4);
    candidate.nps_p50 = round_to(candidate.nps_p50, 3);
    candidate.nps_p90 = round_to(candidate.nps_p90, 3);
    candidate.nps_p95 = round_to(candidate.nps_p95, 3);
    candidate.nps_sustained_top30 = round_to(candidate.nps_sustained_top30, 3);
    candidate.nps_active_ratio = round_to(candidate.nps_active_ratio, 4);
    candidate.nps_active_cv = round_to(candidate.nps_active_cv, 4);
    candidate.duration_s = round_to(candidate.duration_s, 2);
    candidate.stamina_index = round_to(candidate.stamina_index, 4);
    candidate.burst_ratio = round_to(candidate.burst_ratio, 4);
    candidate.pattern_irregularity = round_to(candidate.pattern_irregularity, 4);
    candidate.timing_irregularity = round_to(candidate.timing_irregularity, 4);
    candidate.chord_complexity = round_to(candidate.chord_complexity, 4);
    candidate.ln_ratio = round_to(candidate.ln_ratio, 4);
    candidate.hold_occupancy = round_to(candidate.hold_occupancy, 4);
    candidate.ln_duration_mean_ms = round_to(candidate.ln_duration_mean_ms, 1);
    candidate.ln_duration_cv = round_to(candidate.ln_duration_cv, 4);
    candidate.simultaneous_hold = round_to(candidate.simultaneous_hold, 4);
    candidate.release_density = round_to(candidate.release_density, 3);
    candidate.hold_chord_ratio = round_to(candidate.hold_chord_ratio, 4);

    free(nps);
    free(rows);
    free(notes);

    ChartFeaturesFree(out_features);
    *out_features = candidate;
    return true;

fail:
    free(nps);
    free(rows);
    free(notes);
    ChartFeaturesFree(&candidate);
    return false;
}
