#include "sunny_sr.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SUNNY_KEYS 4

typedef struct
{
    int column;
    double time;
} SunnyNote;

typedef struct
{
    double *values;
    size_t count;
} DoubleArray;

typedef struct
{
    double difficulty;
    double weight;
} WeightedDifficulty;

static int compare_double(const void *a, const void *b)
{
    const double da = *(const double *)a;
    const double db = *(const double *)b;
    return (da > db) - (da < db);
}

static int compare_note(const void *a, const void *b)
{
    const SunnyNote *na = (const SunnyNote *)a;
    const SunnyNote *nb = (const SunnyNote *)b;

    if (na->time < nb->time) return -1;
    if (na->time > nb->time) return 1;
    return na->column - nb->column;
}

static int compare_weighted_difficulty(const void *a, const void *b)
{
    const WeightedDifficulty *da = (const WeightedDifficulty *)a;
    const WeightedDifficulty *db = (const WeightedDifficulty *)b;
    return (da->difficulty > db->difficulty) - (da->difficulty < db->difficulty);
}

static double clamp_double(double value, double minimum, double maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static size_t lower_bound_double(const double *values, size_t count, double target)
{
    size_t left = 0;
    size_t right = count;

    while (left < right)
    {
        const size_t mid = left + (right - left) / 2;
        if (values[mid] < target)
            left = mid + 1;
        else
            right = mid;
    }

    return left;
}

static size_t upper_bound_double(const double *values, size_t count, double target)
{
    size_t left = 0;
    size_t right = count;

    while (left < right)
    {
        const size_t mid = left + (right - left) / 2;
        if (values[mid] <= target)
            left = mid + 1;
        else
            right = mid;
    }

    return left;
}

static DoubleArray sorted_unique(double *values, size_t count)
{
    DoubleArray result = {0};

    if (!values || count == 0)
        return result;

    qsort(values, count, sizeof(double), compare_double);

    size_t unique_count = 1;
    for (size_t i = 1; i < count; ++i)
    {
        if (values[i] != values[unique_count - 1])
            values[unique_count++] = values[i];
    }

    double *shrunk = realloc(values, unique_count * sizeof(double));
    result.values = shrunk ? shrunk : values;
    result.count = unique_count;
    return result;
}

static double piecewise_query(
    const double *x,
    const double *f,
    const double *cumulative,
    size_t count,
    double query
)
{
    if (count < 2)
        return 0.0;

    size_t insertion = lower_bound_double(x, count, query);
    size_t index;

    if (insertion == 0)
        index = 0;
    else if (insertion >= count)
        index = count - 2;
    else
        index = insertion - 1;

    if (index > count - 2)
        index = count - 2;

    return cumulative[index] + f[index] * (query - x[index]);
}

static bool smooth_on_corners(
    const double *x,
    const double *f,
    size_t count,
    double window,
    double scale,
    bool average,
    double *out
)
{
    if (!x || !f || !out || count < 2)
        return false;

    double *cumulative = calloc(count, sizeof(double));
    if (!cumulative)
        return false;

    for (size_t i = 1; i < count; ++i)
        cumulative[i] = cumulative[i - 1] + f[i - 1] * (x[i] - x[i - 1]);

    for (size_t i = 0; i < count; ++i)
    {
        const double a = clamp_double(x[i] - window, x[0], x[count - 1]);
        const double b = clamp_double(x[i] + window, x[0], x[count - 1]);

        double value =
            piecewise_query(x, f, cumulative, count, b) -
            piecewise_query(x, f, cumulative, count, a);

        if (average)
        {
            const double span = b - a;
            value = span > 0.0 ? value / span : 0.0;
        }
        else
        {
            value *= scale;
        }

        out[i] = value;
    }

    free(cumulative);
    return true;
}

static double interp_linear(
    const double *x,
    const double *values,
    size_t count,
    double query
)
{
    if (count == 0)
        return 0.0;

    if (query <= x[0])
        return values[0];

    if (query >= x[count - 1])
        return values[count - 1];

    size_t hi = upper_bound_double(x, count, query);
    if (hi == 0)
        return values[0];
    if (hi >= count)
        return values[count - 1];

    const size_t lo = hi - 1;
    const double width = x[hi] - x[lo];

    if (width <= 0.0)
        return values[lo];

    const double t = (query - x[lo]) / width;
    return values[lo] + (values[hi] - values[lo]) * t;
}

static double interp_step(
    const double *x,
    const double *values,
    size_t count,
    double query
)
{
    if (count == 0)
        return 0.0;

    size_t index = upper_bound_double(x, count, query);
    if (index == 0)
        return values[0];

    --index;
    if (index >= count)
        index = count - 1;

    return values[index];
}

static double rescale_high(double sr)
{
    if (sr <= 9.0)
        return sr;

    return 9.0 + (sr - 9.0) / 1.2;
}

static bool build_notes(
    const AnalysisMap *map,
    SunnyNote **out_notes,
    size_t *out_count,
    double **out_column_times,
    size_t column_counts[SUNNY_KEYS],
    double *out_T
)
{
    if (!map || !out_notes || !out_count || !out_column_times || !out_T)
        return false;

    const size_t note_count = map->note_count;
    if (note_count == 0)
        return false;

    SunnyNote *notes = malloc(note_count * sizeof(SunnyNote));
    if (!notes)
        return false;

    memset(column_counts, 0, SUNNY_KEYS * sizeof(size_t));

    for (size_t i = 0; i < note_count; ++i)
    {
        int column = (int)map->notes[i].column;
        if (column < 0 || column >= SUNNY_KEYS)
        {
            free(notes);
            return false;
        }

        notes[i].column = column;
        notes[i].time = floor(map->notes[i].start_ms);
        column_counts[column]++;
    }

    qsort(notes, note_count, sizeof(SunnyNote), compare_note);

    for (int k = 0; k < SUNNY_KEYS; ++k)
    {
        out_column_times[k] = NULL;

        if (column_counts[k] > 0)
        {
            out_column_times[k] = malloc(column_counts[k] * sizeof(double));
            if (!out_column_times[k])
            {
                for (int j = 0; j < k; ++j)
                    free(out_column_times[j]);

                free(notes);
                return false;
            }
        }
    }

    size_t write_index[SUNNY_KEYS] = {0};

    for (size_t i = 0; i < note_count; ++i)
    {
        const int column = notes[i].column;
        out_column_times[column][write_index[column]++] = notes[i].time;
    }

    *out_T = notes[note_count - 1].time + 1.0;
    *out_notes = notes;
    *out_count = note_count;
    return true;
}

static bool build_corners(
    const SunnyNote *notes,
    size_t note_count,
    double T,
    DoubleArray *all_corners,
    DoubleArray *base_corners,
    DoubleArray *a_corners
)
{
    if (!notes || note_count == 0 || !all_corners || !base_corners || !a_corners)
        return false;

    const size_t base_capacity = note_count * 4 + 2;
    const size_t a_capacity = note_count * 3 + 2;

    double *base = malloc(base_capacity * sizeof(double));
    double *ac = malloc(a_capacity * sizeof(double));

    if (!base || !ac)
    {
        free(base);
        free(ac);
        return false;
    }

    size_t base_count = 0;
    size_t a_count = 0;

    for (size_t i = 0; i < note_count; ++i)
    {
        const double h = notes[i].time;

        const double base_values[4] =
        {
            h,
            h + 501.0,
            h - 499.0,
            h + 1.0
        };

        for (size_t j = 0; j < 4; ++j)
        {
            if (base_values[j] >= 0.0 && base_values[j] <= T)
                base[base_count++] = base_values[j];
        }

        const double a_values[3] =
        {
            h,
            h + 1000.0,
            h - 1000.0
        };

        for (size_t j = 0; j < 3; ++j)
        {
            if (a_values[j] >= 0.0 && a_values[j] <= T)
                ac[a_count++] = a_values[j];
        }
    }

    base[base_count++] = 0.0;
    base[base_count++] = T;
    ac[a_count++] = 0.0;
    ac[a_count++] = T;

    *base_corners = sorted_unique(base, base_count);
    *a_corners = sorted_unique(ac, a_count);

    if (base_corners->count < 2 || a_corners->count < 2)
    {
        free(base_corners->values);
        free(a_corners->values);
        memset(base_corners, 0, sizeof(*base_corners));
        memset(a_corners, 0, sizeof(*a_corners));
        return false;
    }

    double *all = malloc(
        (base_corners->count + a_corners->count) * sizeof(double)
    );

    if (!all)
        return false;

    memcpy(
        all,
        base_corners->values,
        base_corners->count * sizeof(double)
    );

    memcpy(
        all + base_corners->count,
        a_corners->values,
        a_corners->count * sizeof(double)
    );

    *all_corners = sorted_unique(
        all,
        base_corners->count + a_corners->count
    );

    return all_corners->count >= 2;
}

static bool build_key_usage(
    double T,
    double *column_times[SUNNY_KEYS],
    const size_t column_counts[SUNNY_KEYS],
    const DoubleArray *base,
    uint8_t **out_key_usage,
    double **out_key_usage_400
)
{
    const size_t m = base->count;

    uint8_t *key_usage = calloc(SUNNY_KEYS * m, sizeof(uint8_t));
    double *key_usage_400 = calloc(SUNNY_KEYS * m, sizeof(double));

    if (!key_usage || !key_usage_400)
    {
        free(key_usage);
        free(key_usage_400);
        return false;
    }

    const double inv_400_sq = 3.75 / 160000.0;

    for (int k = 0; k < SUNNY_KEYS; ++k)
    {
        for (size_t n = 0; n < column_counts[k]; ++n)
        {
            const double h = column_times[k][n];

            const double start = fmax(h - 150.0, 0.0);
            const double end = fmin(h + 150.0, T - 1.0);

            size_t li = lower_bound_double(base->values, m, start);
            size_t ri = lower_bound_double(base->values, m, end);

            if (li > m) li = m;
            if (ri > m) ri = m;

            for (size_t i = li; i < ri; ++i)
                key_usage[(size_t)k * m + i] = 1;

            li = lower_bound_double(base->values, m, h - 400.0);
            ri = lower_bound_double(base->values, m, h + 400.0);
            size_t mid = lower_bound_double(base->values, m, h);

            if (mid < m)
                key_usage_400[(size_t)k * m + mid] += 3.75;

            for (size_t i = li; i < mid && i < m; ++i)
            {
                const double diff = base->values[i] - h;
                key_usage_400[(size_t)k * m + i] +=
                    3.75 - inv_400_sq * diff * diff;
            }

            if (mid < m)
            {
                for (size_t i = mid + 1; i < ri && i < m; ++i)
                {
                    const double diff = base->values[i] - h;
                    key_usage_400[(size_t)k * m + i] +=
                        3.75 - inv_400_sq * diff * diff;
                }
            }
        }
    }

    *out_key_usage = key_usage;
    *out_key_usage_400 = key_usage_400;
    return true;
}

static bool compute_anchor(
    const double *key_usage_400,
    size_t m,
    double **out_anchor
)
{
    double *anchor = malloc(m * sizeof(double));
    if (!anchor)
        return false;

    for (size_t i = 0; i < m; ++i)
    {
        double counts[SUNNY_KEYS];

        for (int k = 0; k < SUNNY_KEYS; ++k)
            counts[k] = key_usage_400[(size_t)k * m + i];

        for (int a = 0; a < SUNNY_KEYS - 1; ++a)
        {
            for (int b = a + 1; b < SUNNY_KEYS; ++b)
            {
                if (counts[b] > counts[a])
                {
                    const double temp = counts[a];
                    counts[a] = counts[b];
                    counts[b] = temp;
                }
            }
        }

        int nonzero = 0;
        double walk = 0.0;
        double max_walk = 0.0;

        for (int k = 0; k < SUNNY_KEYS; ++k)
        {
            if (counts[k] > 0.0)
                nonzero++;
        }

        for (int k = 0; k < SUNNY_KEYS - 1; ++k)
        {
            const double c0 = counts[k];
            const double c1 = counts[k + 1];

            if (c0 <= 0.0 || c1 <= 0.0)
                continue;

            const double ratio = c1 / c0;
            const double weight =
                1.0 - 4.0 * pow(0.5 - ratio, 2.0);

            walk += c0 * weight;
            max_walk += c0;
        }

        const double raw_anchor =
            nonzero > 1
            ? walk / fmax(max_walk, 1e-9)
            : 0.0;

        anchor[i] =
            1.0 +
            fmin(
                raw_anchor - 0.18,
                5.0 * pow(raw_anchor - 0.22, 3.0)
            );
    }

    *out_anchor = anchor;
    return true;
}

static double jack_nerfer(double delta)
{
    return
        1.0 -
        7e-5 *
        pow(
            0.15 + fabs(delta - 0.08),
            -4.0
        );
}

static bool compute_jbar(
    double x,
    double *column_times[SUNNY_KEYS],
    const size_t column_counts[SUNNY_KEYS],
    const DoubleArray *base,
    double **out_delta_ks,
    double **out_jbar
)
{
    const size_t m = base->count;

    double *j_ks = calloc(SUNNY_KEYS * m, sizeof(double));
    double *delta_ks = malloc(SUNNY_KEYS * m * sizeof(double));
    double *jbar_ks = calloc(SUNNY_KEYS * m, sizeof(double));
    double *jbar = calloc(m, sizeof(double));

    if (!j_ks || !delta_ks || !jbar_ks || !jbar)
    {
        free(j_ks);
        free(delta_ks);
        free(jbar_ks);
        free(jbar);
        return false;
    }

    for (size_t i = 0; i < SUNNY_KEYS * m; ++i)
        delta_ks[i] = 1e9;

    const double x_quarter = pow(x, 0.25);

    for (int k = 0; k < SUNNY_KEYS; ++k)
    {
        if (column_counts[k] < 2)
            continue;

        for (size_t n = 0; n + 1 < column_counts[k]; ++n)
        {
            const double start = column_times[k][n];
            const double end = column_times[k][n + 1];
            const double delta = 0.001 * (end - start);

            if (delta <= 0.0)
                continue;

            const double value =
                (1.0 / delta) *
                (1.0 / (delta + 0.11 * x_quarter)) *
                jack_nerfer(delta);

            size_t li = lower_bound_double(base->values, m, start);
            size_t ri = lower_bound_double(base->values, m, end);

            if (li > m) li = m;
            if (ri > m) ri = m;

            for (size_t i = li; i < ri; ++i)
            {
                j_ks[(size_t)k * m + i] = value;
                delta_ks[(size_t)k * m + i] = delta;
            }
        }

        if (!smooth_on_corners(
                base->values,
                j_ks + (size_t)k * m,
                m,
                500.0,
                0.001,
                false,
                jbar_ks + (size_t)k * m
            ))
        {
            free(j_ks);
            free(delta_ks);
            free(jbar_ks);
            free(jbar);
            return false;
        }
    }

    for (size_t i = 0; i < m; ++i)
    {
        double numerator = 0.0;
        double denominator = 0.0;

        for (int k = 0; k < SUNNY_KEYS; ++k)
        {
            const double delta = delta_ks[(size_t)k * m + i];
            const double weight = 1.0 / delta;
            const double local = fmax(
                jbar_ks[(size_t)k * m + i],
                0.0
            );

            numerator += pow(local, 5.0) * weight;
            denominator += weight;
        }

        jbar[i] =
            pow(
                numerator / fmax(denominator, 1e-9),
                0.2
            );
    }

    free(j_ks);
    free(jbar_ks);

    *out_delta_ks = delta_ks;
    *out_jbar = jbar;
    return true;
}

static bool active_at(
    const uint8_t *key_usage,
    size_t m,
    int column,
    size_t corner_index
)
{
    if (column < 0 || column >= SUNNY_KEYS || corner_index >= m)
        return false;

    return key_usage[(size_t)column * m + corner_index] != 0;
}

static bool compute_xbar(
    double x,
    const SunnyNote *notes,
    size_t note_count,
    const uint8_t *key_usage,
    const DoubleArray *base,
    double **out_xbar
)
{
    const size_t m = base->count;
    static const double cross_coeff[SUNNY_KEYS + 1] =
    {
        0.175,
        0.25,
        0.05,
        0.25,
        0.175
    };

    double *x_ks = calloc((SUNNY_KEYS + 1) * m, sizeof(double));
    double *fast_cross = calloc((SUNNY_KEYS + 1) * m, sizeof(double));
    double *x_base = calloc(m, sizeof(double));
    double *xbar = calloc(m, sizeof(double));
    double *pair_times = malloc(note_count * sizeof(double));

    if (!x_ks || !fast_cross || !x_base || !xbar || !pair_times)
    {
        free(x_ks);
        free(fast_cross);
        free(x_base);
        free(xbar);
        free(pair_times);
        return false;
    }

    for (int k = 0; k <= SUNNY_KEYS; ++k)
    {
        size_t pair_count = 0;

        for (size_t n = 0; n < note_count; ++n)
        {
            const int column = notes[n].column;
            bool include;

            if (k == 0)
                include = column == 0;
            else if (k == SUNNY_KEYS)
                include = column == SUNNY_KEYS - 1;
            else
                include = column == k - 1 || column == k;

            if (include)
                pair_times[pair_count++] = notes[n].time;
        }

        if (pair_count < 2)
            continue;

        for (size_t n = 0; n + 1 < pair_count; ++n)
        {
            const double start = pair_times[n];
            const double end = pair_times[n + 1];
            const double delta = 0.001 * (end - start);

            if (delta <= 0.0)
                continue;

            const double max_delta = fmax(x, delta);
            double value = 0.16 / (max_delta * max_delta);

            const double fast_floor = fmax(0.06, 0.75 * x);
            const double fast_delta = fmax(delta, fast_floor);
            const double fc_value =
                fmax(
                    0.0,
                    0.4 / (fast_delta * fast_delta) - 80.0
                );

            size_t li = lower_bound_double(base->values, m, start);
            size_t ri = lower_bound_double(base->values, m, end);

            if (li >= m || ri > m || ri <= li)
                continue;

            const size_t endpoint = ri < m ? ri : m - 1;

            const bool left_inactive =
                !active_at(key_usage, m, k - 1, li) &&
                !active_at(key_usage, m, k - 1, endpoint);

            const bool right_inactive =
                !active_at(key_usage, m, k, li) &&
                !active_at(key_usage, m, k, endpoint);

            if (left_inactive || right_inactive)
                value *= 1.0 - cross_coeff[k];

            for (size_t i = li; i < ri && i < m; ++i)
            {
                x_ks[(size_t)k * m + i] = value;
                fast_cross[(size_t)k * m + i] = fc_value;
            }
        }
    }

    for (size_t i = 0; i < m; ++i)
    {
        double value = 0.0;

        for (int k = 0; k <= SUNNY_KEYS; ++k)
            value += x_ks[(size_t)k * m + i] * cross_coeff[k];

        for (int k = 0; k < SUNNY_KEYS; ++k)
        {
            const double left =
                fast_cross[(size_t)k * m + i] * cross_coeff[k];

            const double right =
                fast_cross[(size_t)(k + 1) * m + i] *
                cross_coeff[k + 1];

            value += sqrt(fmax(left * right, 0.0));
        }

        x_base[i] = value;
    }

    const bool ok =
        smooth_on_corners(
            base->values,
            x_base,
            m,
            500.0,
            0.001,
            false,
            xbar
        );

    free(x_ks);
    free(fast_cross);
    free(x_base);
    free(pair_times);

    if (!ok)
    {
        free(xbar);
        return false;
    }

    *out_xbar = xbar;
    return true;
}

static double stream_booster(double delta)
{
    double bpm = 7.5 / delta;
    bpm = clamp_double(bpm, 0.0, 420.0);

    const double primary =
        0.10 /
        (1.0 + exp(-0.06 * (bpm - 175.0)));

    double secondary = 0.0;

    if (bpm >= 200.0 && bpm <= 350.0)
    {
        secondary =
            0.30 *
            (1.0 - exp(-0.02 * (bpm - 200.0)));
    }

    return 1.0 + primary + secondary;
}

static bool compute_pbar(
    double x,
    const SunnyNote *notes,
    size_t note_count,
    const double *anchor,
    const DoubleArray *base,
    double **out_pbar
)
{
    const size_t m = base->count;

    double *p_step = calloc(m, sizeof(double));
    double *pbar = calloc(m, sizeof(double));

    if (!p_step || !pbar)
    {
        free(p_step);
        free(pbar);
        return false;
    }

    const double base_inner =
        0.08 / x *
        (
            1.0 -
            24.0 / x *
            pow(x / 6.0, 2.0)
        );

    const double base_inc =
        pow(base_inner, 0.25);

    const double spike_val =
        1000.0 *
        pow(
            0.02 *
            (4.0 / x - 24.0),
            0.25
        );

    for (size_t n = 0; n + 1 < note_count; ++n)
    {
        const double left_time = notes[n].time;
        const double right_time = notes[n + 1].time;
        const double dt = right_time - left_time;

        size_t li =
            lower_bound_double(base->values, m, left_time);

        if (dt < 1e-9)
        {
            const size_t ri_spike =
                upper_bound_double(base->values, m, left_time);

            for (size_t i = li; i < ri_spike && i < m; ++i)
                p_step[i] += spike_val;

            continue;
        }

        size_t ri =
            lower_bound_double(base->values, m, right_time);

        if (ri <= li)
            continue;

        const double delta = 0.001 * dt;
        const double booster = fmax(stream_booster(delta), 1.0);

        double inc;

        if (delta < 2.0 * x / 3.0)
        {
            const double inner =
                0.08 / x *
                (
                    1.0 -
                    24.0 / x *
                    pow(delta - x / 2.0, 2.0)
                );

            if (inner <= 0.0)
                continue;

            inc =
                (1.0 / delta) *
                pow(inner, 0.25) *
                booster;
        }
        else
        {
            inc =
                (1.0 / delta) *
                base_inc *
                booster;
        }

        for (size_t i = li; i < ri && i < m; ++i)
        {
            const double scaled = inc * anchor[i];
            const double cap = fmax(inc, inc * 2.0 - 10.0);
            p_step[i] += fmin(scaled, cap);
        }
    }

    const bool ok =
        smooth_on_corners(
            base->values,
            p_step,
            m,
            500.0,
            0.001,
            false,
            pbar
        );

    free(p_step);

    if (!ok)
    {
        free(pbar);
        return false;
    }

    *out_pbar = pbar;
    return true;
}

static int collect_active_columns(
    const uint8_t *key_usage,
    size_t m,
    size_t index,
    int columns[SUNNY_KEYS]
)
{
    int count = 0;

    for (int k = 0; k < SUNNY_KEYS; ++k)
    {
        if (active_at(key_usage, m, k, index))
            columns[count++] = k;
    }

    return count;
}

static bool compute_abar(
    const uint8_t *key_usage,
    const double *delta_ks,
    const DoubleArray *a,
    const DoubleArray *base,
    double **out_abar
)
{
    const size_t m = base->count;
    const size_t ac = a->count;

    double *dks = calloc((SUNNY_KEYS - 1) * m, sizeof(double));
    double *a_step = malloc(ac * sizeof(double));
    double *abar = calloc(ac, sizeof(double));

    if (!dks || !a_step || !abar)
    {
        free(dks);
        free(a_step);
        free(abar);
        return false;
    }

    for (size_t i = 0; i < m; ++i)
    {
        int columns[SUNNY_KEYS];
        const int count =
            collect_active_columns(key_usage, m, i, columns);

        for (int j = 0; j + 1 < count; ++j)
        {
            const int k0 = columns[j];
            const int k1 = columns[j + 1];

            const double dk0 =
                delta_ks[(size_t)k0 * m + i];

            const double dk1 =
                delta_ks[(size_t)k1 * m + i];

            const double value =
                fabs(dk0 - dk1) +
                0.4 *
                fmax(
                    0.0,
                    fmax(dk0, dk1) - 0.11
                );

            if (k0 >= 0 && k0 < SUNNY_KEYS - 1)
                dks[(size_t)k0 * m + i] = value;
        }
    }

    for (size_t i = 0; i < ac; ++i)
    {
        a_step[i] = 1.0;

        size_t base_index =
            lower_bound_double(
                base->values,
                m,
                a->values[i]
            );

        if (base_index >= m)
            base_index = m - 1;

        int columns[SUNNY_KEYS];
        const int count =
            collect_active_columns(
                key_usage,
                m,
                base_index,
                columns
            );

        for (int j = 0; j + 1 < count; ++j)
        {
            const int k0 = columns[j];
            const int k1 = columns[j + 1];

            if (k0 < 0 || k0 >= SUNNY_KEYS - 1)
                continue;

            const double d_value =
                dks[(size_t)k0 * m + base_index];

            const double dk0 =
                delta_ks[(size_t)k0 * m + base_index];

            const double dk1 =
                delta_ks[(size_t)k1 * m + base_index];

            const double max_delta = fmax(dk0, dk1);

            if (d_value < 0.02)
            {
                a_step[i] *=
                    fmin(
                        0.75 + 0.5 * max_delta,
                        1.0
                    );
            }
            else if (d_value < 0.07)
            {
                a_step[i] *=
                    fmin(
                        0.65 +
                        5.0 * d_value +
                        0.5 * max_delta,
                        1.0
                    );
            }
        }
    }

    const bool ok =
        smooth_on_corners(
            a->values,
            a_step,
            ac,
            250.0,
            1.0,
            true,
            abar
        );

    free(dks);
    free(a_step);

    if (!ok)
    {
        free(abar);
        return false;
    }

    *out_abar = abar;
    return true;
}

static bool compute_c_and_ks(
    const SunnyNote *notes,
    size_t note_count,
    const uint8_t *key_usage,
    const DoubleArray *base,
    double **out_c,
    double **out_ks
)
{
    const size_t m = base->count;

    double *c = calloc(m, sizeof(double));
    double *ks = calloc(m, sizeof(double));
    double *note_times = malloc(note_count * sizeof(double));

    if (!c || !ks || !note_times)
    {
        free(c);
        free(ks);
        free(note_times);
        return false;
    }

    for (size_t i = 0; i < note_count; ++i)
        note_times[i] = notes[i].time;

    for (size_t i = 0; i < m; ++i)
    {
        const size_t lo =
            lower_bound_double(
                note_times,
                note_count,
                base->values[i] - 500.0
            );

        const size_t hi =
            lower_bound_double(
                note_times,
                note_count,
                base->values[i] + 500.0
            );

        c[i] = (double)(hi - lo);

        int active_count = 0;
        for (int k = 0; k < SUNNY_KEYS; ++k)
        {
            if (active_at(key_usage, m, k, i))
                active_count++;
        }

        ks[i] = (double)(active_count > 0 ? active_count : 1);
    }

    free(note_times);

    *out_c = c;
    *out_ks = ks;
    return true;
}

static double percentile_value(
    const double *normalized_cumulative,
    const WeightedDifficulty *sorted,
    size_t count,
    double target
)
{
    size_t left = 0;
    size_t right = count;

    while (left < right)
    {
        const size_t mid = left + (right - left) / 2;

        if (normalized_cumulative[mid] < target)
            left = mid + 1;
        else
            right = mid;
    }

    if (left >= count)
        left = count - 1;

    return sorted[left].difficulty;
}

bool SunnySrCalculate(
    const AnalysisMap *map,
    SunnySrResult *out_result
)
{
    if (!map || !out_result || !map->notes || map->note_count < 2)
        return false;

    memset(out_result, 0, sizeof(*out_result));

    SunnyNote *notes = NULL;
    size_t note_count = 0;

    double *column_times[SUNNY_KEYS] = {0};
    size_t column_counts[SUNNY_KEYS] = {0};

    double T = 0.0;

    DoubleArray all = {0};
    DoubleArray base = {0};
    DoubleArray a = {0};

    uint8_t *key_usage = NULL;
    double *key_usage_400 = NULL;
    double *anchor = NULL;

    double *delta_ks = NULL;
    double *jbar_base = NULL;
    double *xbar_base = NULL;
    double *pbar_base = NULL;
    double *abar_a = NULL;
    double *c_base = NULL;
    double *ks_base = NULL;

    double *jbar_all = NULL;
    double *xbar_all = NULL;
    double *pbar_all = NULL;
    double *abar_all = NULL;
    double *c_all = NULL;
    double *ks_all = NULL;
    double *d_all = NULL;

    WeightedDifficulty *weighted = NULL;
    double *normalized_cumulative = NULL;

    bool success = false;

    const double od = 9.0;

    double x =
        0.3 *
        sqrt(
            (
                64.5 -
                ceil(od * 3.0)
            ) /
            500.0
        );

    x =
        fmin(
            x,
            0.6 * (x - 0.09) + 0.09
        );

    if (!build_notes(
            map,
            &notes,
            &note_count,
            column_times,
            column_counts,
            &T
        ))
        goto cleanup;

    if (!build_corners(
            notes,
            note_count,
            T,
            &all,
            &base,
            &a
        ))
        goto cleanup;

    if (!build_key_usage(
            T,
            column_times,
            column_counts,
            &base,
            &key_usage,
            &key_usage_400
        ))
        goto cleanup;

    if (!compute_anchor(
            key_usage_400,
            base.count,
            &anchor
        ))
        goto cleanup;

    if (!compute_jbar(
            x,
            column_times,
            column_counts,
            &base,
            &delta_ks,
            &jbar_base
        ))
        goto cleanup;

    if (!compute_xbar(
            x,
            notes,
            note_count,
            key_usage,
            &base,
            &xbar_base
        ))
        goto cleanup;

    if (!compute_pbar(
            x,
            notes,
            note_count,
            anchor,
            &base,
            &pbar_base
        ))
        goto cleanup;

    if (!compute_abar(
            key_usage,
            delta_ks,
            &a,
            &base,
            &abar_a
        ))
        goto cleanup;

    if (!compute_c_and_ks(
            notes,
            note_count,
            key_usage,
            &base,
            &c_base,
            &ks_base
        ))
        goto cleanup;

    const size_t l = all.count;

    jbar_all = malloc(l * sizeof(double));
    xbar_all = malloc(l * sizeof(double));
    pbar_all = malloc(l * sizeof(double));
    abar_all = malloc(l * sizeof(double));
    c_all = malloc(l * sizeof(double));
    ks_all = malloc(l * sizeof(double));
    d_all = malloc(l * sizeof(double));

    weighted = malloc(l * sizeof(WeightedDifficulty));
    normalized_cumulative = malloc(l * sizeof(double));

    if (
        !jbar_all ||
        !xbar_all ||
        !pbar_all ||
        !abar_all ||
        !c_all ||
        !ks_all ||
        !d_all ||
        !weighted ||
        !normalized_cumulative
    )
        goto cleanup;

    for (size_t i = 0; i < l; ++i)
    {
        const double time = all.values[i];

        jbar_all[i] =
            interp_linear(
                base.values,
                jbar_base,
                base.count,
                time
            );

        xbar_all[i] =
            interp_linear(
                base.values,
                xbar_base,
                base.count,
                time
            );

        pbar_all[i] =
            interp_linear(
                base.values,
                pbar_base,
                base.count,
                time
            );

        abar_all[i] =
            interp_linear(
                a.values,
                abar_a,
                a.count,
                time
            );

        c_all[i] =
            interp_step(
                base.values,
                c_base,
                base.count,
                time
            );

        ks_all[i] =
            interp_step(
                base.values,
                ks_base,
                base.count,
                time
            );

        const double ks =
            fmax(ks_all[i], 1.0);

        const double abar =
            fmax(abar_all[i], 0.0);

        const double jbar =
            fmax(jbar_all[i], 0.0);

        const double pbar =
            fmax(pbar_all[i], 0.0);

        const double xbar =
            fmax(xbar_all[i], 0.0);

        const double same_column_term =
            pow(abar, 3.0 / ks) *
            fmin(
                jbar,
                8.0 + 0.85 * jbar
            );

        const double pressing_term =
            pow(abar, 2.0 / 3.0) *
            (0.8 * pbar);

        const double s_all =
            pow(
                0.4 * pow(same_column_term, 1.5) +
                0.6 * pow(pressing_term, 1.5),
                2.0 / 3.0
            );

        const double t_all =
            (
                pow(abar, 3.0 / ks) *
                xbar
            ) /
            (
                xbar +
                s_all +
                1.0
            );

        d_all[i] =
            2.7 *
            sqrt(fmax(s_all, 0.0)) *
            pow(fmax(t_all, 0.0), 1.5) +
            s_all * 0.27;

        double gap;

        if (i == 0)
        {
            gap =
                (
                    all.values[1] -
                    all.values[0]
                ) /
                2.0;
        }
        else if (i == l - 1)
        {
            gap =
                (
                    all.values[l - 1] -
                    all.values[l - 2]
                ) /
                2.0;
        }
        else
        {
            gap =
                (
                    all.values[i + 1] -
                    all.values[i - 1]
                ) /
                2.0;
        }

        weighted[i].difficulty =
            d_all[i];

        weighted[i].weight =
            c_all[i] * gap;
    }

    qsort(
        weighted,
        l,
        sizeof(WeightedDifficulty),
        compare_weighted_difficulty
    );

    double total_weight = 0.0;

    for (size_t i = 0; i < l; ++i)
        total_weight += weighted[i].weight;

    if (!(total_weight > 0.0) || !isfinite(total_weight))
        goto cleanup;

    double cumulative = 0.0;

    for (size_t i = 0; i < l; ++i)
    {
        cumulative += weighted[i].weight;
        normalized_cumulative[i] =
            cumulative / total_weight;
    }

    static const double targets[8] =
    {
        0.945,
        0.935,
        0.925,
        0.915,
        0.845,
        0.835,
        0.825,
        0.815
    };

    double top_values[8];

    for (size_t i = 0; i < 8; ++i)
    {
        top_values[i] =
            percentile_value(
                normalized_cumulative,
                weighted,
                l,
                targets[i]
            );
    }

    const double percentile_93 =
        (
            top_values[0] +
            top_values[1] +
            top_values[2] +
            top_values[3]
        ) /
        4.0;

    const double percentile_83 =
        (
            top_values[4] +
            top_values[5] +
            top_values[6] +
            top_values[7]
        ) /
        4.0;

    double fifth_moment = 0.0;

    for (size_t i = 0; i < l; ++i)
    {
        fifth_moment +=
            pow(
                fmax(
                    weighted[i].difficulty,
                    0.0
                ),
                5.0
            ) *
            weighted[i].weight;
    }

    const double weighted_mean =
        pow(
            fifth_moment /
            total_weight,
            0.2
        );

    double sr =
        0.88 *
        percentile_93 *
        0.25 +

        0.94 *
        percentile_83 *
        0.20 +

        weighted_mean *
        0.55;

    sr *=
        (double)note_count /
        (
            (double)note_count +
            60.0
        );

    sr =
        rescale_high(sr) *
        0.975;

    if (!isfinite(sr))
        goto cleanup;

    double jbar_max = 0.0;
    double pbar_max = 0.0;
    double xbar_max = 0.0;
    double abar_sum = 0.0;

    for (size_t i = 0; i < l; ++i)
    {
        if (jbar_all[i] > jbar_max) jbar_max = jbar_all[i];
        if (pbar_all[i] > pbar_max) pbar_max = pbar_all[i];
        if (xbar_all[i] > xbar_max) xbar_max = xbar_all[i];
        abar_sum += abar_all[i];
    }

    const double abar_mean =
        l > 0
            ? abar_sum / (double)l
            : 0.0;

    const double component_total =
        jbar_max +
        pbar_max +
        xbar_max +
        1.0;

    const double jack_denominator =
        jbar_max +
        pbar_max +
        xbar_max;

    out_result->star_rating = sr;
    out_result->percentile_93 = percentile_93;
    out_result->percentile_83 = percentile_83;
    out_result->weighted_mean = weighted_mean;

    out_result->jbar_max = jbar_max;
    out_result->pbar_max = pbar_max;
    out_result->xbar_max = xbar_max;
    out_result->abar_mean = abar_mean;

    out_result->jack_ratio =
        jack_denominator > 1e-9
            ? jbar_max / jack_denominator
            : 0.0;

    out_result->jbar_share = jbar_max / component_total;
    out_result->pbar_share = pbar_max / component_total;
    out_result->xbar_share = xbar_max / component_total;

    out_result->note_count = note_count;
    out_result->corner_count = l;
    out_result->total_notes_eff = l;

    success = true;

cleanup:
    free(notes);

    for (int k = 0; k < SUNNY_KEYS; ++k)
        free(column_times[k]);

    free(all.values);
    free(base.values);
    free(a.values);

    free(key_usage);
    free(key_usage_400);
    free(anchor);

    free(delta_ks);
    free(jbar_base);
    free(xbar_base);
    free(pbar_base);
    free(abar_a);
    free(c_base);
    free(ks_base);

    free(jbar_all);
    free(xbar_all);
    free(pbar_all);
    free(abar_all);
    free(c_all);
    free(ks_all);
    free(d_all);

    free(weighted);
    free(normalized_cumulative);

    return success;
}
