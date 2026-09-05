#include "engine/minacalc_bridge.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C"
{
#include "API.h"
}

namespace
{
    constexpr double MIN_NATIVE_RATE = 0.7;
    constexpr double MAX_NATIVE_RATE = 2.0;
    constexpr double NATIVE_RATE_STEP = 0.1;
    constexpr int NATIVE_RATE_COUNT = 14;

    CalcHandle *g_calc = nullptr;
    MsdForAllRates g_all_rates = {};
    bool g_ready = false;
    size_t g_row_count = 0;
    int g_calc_version = 0;

    struct RowBuild
    {
        double time_ms;
        unsigned int notes;
    };

    struct SkillValue
    {
        MinaCalcSkillset skillset;
        double value;
    };

    double clamp_double(
        double value,
        double minimum,
        double maximum
    )
    {
        if (value < minimum)
            return minimum;

        if (value > maximum)
            return maximum;

        return value;
    }

    double sanitize_score(
        double value
    )
    {
        if (!std::isfinite(value) || value < 0.0)
            return 0.0;

        return value;
    }

    double lerp(
        double a,
        double b,
        double amount
    )
    {
        return a + (b - a) * amount;
    }

    Ssr interpolate_ssr(
        const Ssr &a,
        const Ssr &b,
        double amount
    )
    {
        Ssr out = {};

        out.overall = (float)sanitize_score(
            lerp(a.overall, b.overall, amount)
        );

        out.stream = (float)sanitize_score(
            lerp(a.stream, b.stream, amount)
        );

        out.jumpstream = (float)sanitize_score(
            lerp(a.jumpstream, b.jumpstream, amount)
        );

        out.handstream = (float)sanitize_score(
            lerp(a.handstream, b.handstream, amount)
        );

        out.stamina = (float)sanitize_score(
            lerp(a.stamina, b.stamina, amount)
        );

        out.jackspeed = (float)sanitize_score(
            lerp(a.jackspeed, b.jackspeed, amount)
        );

        out.chordjack = (float)sanitize_score(
            lerp(a.chordjack, b.chordjack, amount)
        );

        out.technical = (float)sanitize_score(
            lerp(a.technical, b.technical, amount)
        );

        return out;
    }

    Ssr sample_table(
        double rate
    )
    {
        rate = clamp_double(
            rate,
            0.5,
            MAX_NATIVE_RATE
        );

        if (rate <= MIN_NATIVE_RATE)
        {
            const double amount =
                (rate - MIN_NATIVE_RATE) /
                NATIVE_RATE_STEP;

            return interpolate_ssr(
                g_all_rates.msds[0],
                g_all_rates.msds[1],
                amount
            );
        }

        if (rate >= MAX_NATIVE_RATE)
        {
            return g_all_rates.msds[
                NATIVE_RATE_COUNT - 1
            ];
        }

        const double table_position =
            (rate - MIN_NATIVE_RATE) /
            NATIVE_RATE_STEP;

        int lower_index =
            (int)std::floor(table_position);

        if (lower_index < 0)
            lower_index = 0;

        if (lower_index >= NATIVE_RATE_COUNT - 1)
            lower_index = NATIVE_RATE_COUNT - 2;

        const int upper_index =
            lower_index + 1;

        const double lower_rate =
            MIN_NATIVE_RATE +
            (double)lower_index *
            NATIVE_RATE_STEP;

        const double amount =
            (rate - lower_rate) /
            NATIVE_RATE_STEP;

        return interpolate_ssr(
            g_all_rates.msds[lower_index],
            g_all_rates.msds[upper_index],
            amount
        );
    }

    double score_for_skillset(
        const MinaCalcScores *scores,
        MinaCalcSkillset skillset
    )
    {
        if (!scores)
            return 0.0;

        switch (skillset)
        {
            case MINACALC_SKILL_STREAM:
                return scores->stream;

            case MINACALC_SKILL_JUMPSTREAM:
                return scores->jumpstream;

            case MINACALC_SKILL_HANDSTREAM:
                return scores->handstream;

            case MINACALC_SKILL_STAMINA:
                return scores->stamina;

            case MINACALC_SKILL_JACKSPEED:
                return scores->jackspeed;

            case MINACALC_SKILL_CHORDJACK:
                return scores->chordjack;

            case MINACALC_SKILL_TECHNICAL:
                return scores->technical;

            default:
                return 0.0;
        }
    }
}

extern "C" bool MinaCalcInit(void)
{
    if (g_calc)
        return true;

    g_calc = create_calc();

    if (!g_calc)
        return false;

    g_calc_version = calc_version();
    g_ready = false;
    g_row_count = 0;

    return true;
}


extern "C" void MinaCalcShutdown(void)
{
    if (g_calc)
    {
        destroy_calc(g_calc);
        g_calc = nullptr;
    }

    g_all_rates = {};
    g_ready = false;
    g_row_count = 0;
    g_calc_version = 0;
}


extern "C" bool MinaCalcLoadBeatmap(
    const Beatmap *beatmap
)
{
    if (
        !g_calc ||
        !beatmap ||
        beatmap->keys != 4 ||
        !beatmap->notes ||
        beatmap->note_count == 0
    )
    {
        g_ready = false;
        return false;
    }

    /* 
     * MinaCalc NoteInfo has no LN release event. 
     * feed note heads only, then merge simultaneous heads into the four-bit row mask it expects. 
     */

    std::vector<RowBuild> objects;
    objects.reserve(beatmap->note_count);

    for (size_t i = 0; i < beatmap->note_count; ++i)
    {
        const ManiaNote &note = beatmap->notes[i];

        if (note.column >= 4)
            continue;

        if (!std::isfinite(note.start_ms))
            continue;

        RowBuild row = {};
        row.time_ms = note.start_ms;
        row.notes = 1u << note.column;

        objects.push_back(row);
    }

    if (objects.empty())
    {
        g_ready = false;
        return false;
    }

    std::sort(
        objects.begin(),
        objects.end(),
        [](const RowBuild &a, const RowBuild &b)
        {
            if (a.time_ms != b.time_ms)
                return a.time_ms < b.time_ms;

            return a.notes < b.notes;
        }
    );

    std::vector<NoteInfo> rows;
    rows.reserve(objects.size());

    for (const RowBuild &object : objects)
    {
        const double time_seconds =
            std::round(
                (object.time_ms / 1000.0) *
                10000.0
            ) /
            10000.0;

        const float row_time =
            (float)time_seconds;

        if (
            !rows.empty() &&
            std::fabs(
                (double)rows.back().rowTime -
                (double)row_time
            ) < 0.00005
        )
        {
            rows.back().notes |=
                object.notes;
        }
        else
        {
            NoteInfo row = {};
            row.notes = object.notes;
            row.rowTime = row_time;

            rows.push_back(row);
        }
    }

    if (rows.size() < 2)
    {
        g_ready = false;
        return false;
    }

    g_all_rates = calc_msd(
        g_calc,
        rows.data(),
        rows.size()
    );

    bool has_any_valid_score = false;

    for (int i = 0; i < NATIVE_RATE_COUNT; ++i)
    {
        const Ssr &score =
            g_all_rates.msds[i];

        if (
            std::isfinite(score.overall) &&
            score.overall > 0.0f
        )
        {
            has_any_valid_score = true;
            break;
        }
    }

    g_row_count = rows.size();
    g_ready = has_any_valid_score;

    return g_ready;
}


extern "C" bool MinaCalcSample(
    double rate,
    MinaCalcScores *out_scores
)
{
    if (
        !g_ready ||
        !out_scores ||
        !std::isfinite(rate) ||
        rate <= 0.0
    )
    {
        return false;
    }

    const double sampled_rate =
        clamp_double(
            rate,
            0.5,
            MAX_NATIVE_RATE
        );

    const Ssr sampled =
        sample_table(sampled_rate);

    std::memset(
        out_scores,
        0,
        sizeof(*out_scores)
    );

    out_scores->rate = sampled_rate;
    out_scores->overall = sanitize_score(sampled.overall);
    out_scores->stream = sanitize_score(sampled.stream);
    out_scores->jumpstream = sanitize_score(sampled.jumpstream);
    out_scores->handstream = sanitize_score(sampled.handstream);
    out_scores->stamina = sanitize_score(sampled.stamina);
    out_scores->jackspeed = sanitize_score(sampled.jackspeed);
    out_scores->chordjack = sanitize_score(sampled.chordjack);
    out_scores->technical = sanitize_score(sampled.technical);
    out_scores->row_count = g_row_count;
    out_scores->calc_version = g_calc_version;

    return true;
}


extern "C" bool MinaCalcIsReady(void)
{
    return g_ready;
}


extern "C" int MinaCalcVersion(void)
{
    return g_calc_version;
}


extern "C" const char *MinaCalcSkillsetName(
    MinaCalcSkillset skillset
)
{
    switch (skillset)
    {
        case MINACALC_SKILL_STREAM:
            return "Stream";

        case MINACALC_SKILL_JUMPSTREAM:
            return "Jumpstream";

        case MINACALC_SKILL_HANDSTREAM:
            return "Handstream";

        case MINACALC_SKILL_STAMINA:
            return "Stamina";

        case MINACALC_SKILL_JACKSPEED:
            return "JackSpeed";

        case MINACALC_SKILL_CHORDJACK:
            return "Chordjack";

        case MINACALC_SKILL_TECHNICAL:
            return "Technical";

        default:
            return "Unknown";
    }
}


extern "C" void MinaCalcGetPatternSummary(
    const MinaCalcScores *scores,
    MinaCalcPatternSummary *out_summary
)
{
    if (!out_summary)
        return;

    std::memset(
        out_summary,
        0,
        sizeof(*out_summary)
    );

    if (!scores)
        return;

    std::array<SkillValue, MINACALC_SKILL_COUNT> values =
    {{
        {MINACALC_SKILL_STREAM, scores->stream},
        {MINACALC_SKILL_JUMPSTREAM, scores->jumpstream},
        {MINACALC_SKILL_HANDSTREAM, scores->handstream},
        {MINACALC_SKILL_STAMINA, scores->stamina},
        {MINACALC_SKILL_JACKSPEED, scores->jackspeed},
        {MINACALC_SKILL_CHORDJACK, scores->chordjack},
        {MINACALC_SKILL_TECHNICAL, scores->technical}
    }};

    std::sort(
        values.begin(),
        values.end(),
        [](const SkillValue &a, const SkillValue &b)
        {
            return a.value > b.value;
        }
    );

    out_summary->primary =
        values[0].skillset;

    out_summary->secondary =
        values[1].skillset;

    out_summary->tertiary =
        values[2].skillset;

    out_summary->primary_score =
        score_for_skillset(
            scores,
            values[0].skillset
        );

    out_summary->secondary_score =
        score_for_skillset(
            scores,
            values[1].skillset
        );

    out_summary->tertiary_score =
        score_for_skillset(
            scores,
            values[2].skillset
        );

    out_summary->has_secondary =
        out_summary->secondary_score > 0.0;

    out_summary->has_tertiary =
        out_summary->tertiary_score > 0.0;
}


extern "C" void MinaCalcFormatPatterns(
    const MinaCalcScores *scores,
    char *output,
    size_t output_size
)
{
    if (!output || output_size == 0)
        return;

    output[0] = '\0';

    if (!scores)
        return;

    MinaCalcPatternSummary summary = {};

    MinaCalcGetPatternSummary(
        scores,
        &summary
    );

    if (summary.primary_score <= 0.0)
    {
        std::snprintf(
            output,
            output_size,
            "Unknown"
        );

        return;
    }

    if (summary.has_tertiary)
    {
        std::snprintf(
            output,
            output_size,
            "%s  /  %s  /  %s",
            MinaCalcSkillsetName(summary.primary),
            MinaCalcSkillsetName(summary.secondary),
            MinaCalcSkillsetName(summary.tertiary)
        );
    }
    else if (summary.has_secondary)
    {
        std::snprintf(
            output,
            output_size,
            "%s  /  %s",
            MinaCalcSkillsetName(summary.primary),
            MinaCalcSkillsetName(summary.secondary)
        );
    }
    else
    {
        std::snprintf(
            output,
            output_size,
            "%s",
            MinaCalcSkillsetName(summary.primary)
        );
    }
}


extern "C" void MinaCalcFormatPatternScores(
    const MinaCalcScores *scores,
    char *output,
    size_t output_size
)
{
    if (!output || output_size == 0)
        return;

    output[0] = '\0';

    if (!scores)
        return;

    MinaCalcPatternSummary summary = {};

    MinaCalcGetPatternSummary(
        scores,
        &summary
    );

    if (summary.primary_score <= 0.0)
    {
        std::snprintf(
            output,
            output_size,
            "No MSD skillset data"
        );

        return;
    }

    if (summary.has_tertiary)
    {
        std::snprintf(
            output,
            output_size,
            "%s %.2f  |  %s %.2f  |  %s %.2f",
            MinaCalcSkillsetName(summary.primary),
            summary.primary_score,
            MinaCalcSkillsetName(summary.secondary),
            summary.secondary_score,
            MinaCalcSkillsetName(summary.tertiary),
            summary.tertiary_score
        );
    }
    else if (summary.has_secondary)
    {
        std::snprintf(
            output,
            output_size,
            "%s %.2f  |  %s %.2f",
            MinaCalcSkillsetName(summary.primary),
            summary.primary_score,
            MinaCalcSkillsetName(summary.secondary),
            summary.secondary_score
        );
    }
    else
    {
        std::snprintf(
            output,
            output_size,
            "%s %.2f",
            MinaCalcSkillsetName(summary.primary),
            summary.primary_score
        );
    }
}
