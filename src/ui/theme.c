#include "theme.h"

#include <math.h>
#include <stddef.h>


typedef struct
{
    double position;
    Color color;
} GradientStop;

static const GradientStop DIFFICULTY_SPECTRUM[] =
{
    {0.10, { 66, 144, 251, 255 }}, /* #4290FB */
    {1.25, { 79, 192, 255, 255 }}, /* #4FC0FF */
    {2.00, { 79, 255, 213, 255 }}, /* #4FFFD5 */
    {2.50, {124, 255,  79, 255 }}, /* #7CFF4F */
    {3.30, {246, 240,  92, 255 }}, /* #F6F05C */
    {4.20, {255, 128, 104, 255 }}, /* #FF8068 */
    {4.90, {255,  78, 111, 255 }}, /* #FF4E6F */
    {5.80, {198,  69, 184, 255 }}, /* #C645B8 */
    {6.70, {101,  99, 222, 255 }}, /* #6563DE */
    {7.70, { 24,  21, 142, 255 }}, /* #18158E */
    {9.00, {  0,   0,   0, 255 }}, /* black */
    {10.0, {  0,   0,   0, 255 }}
};


static const GradientStop DIFFICULTY_TEXT_SPECTRUM[] =
{
    { 9.00, {246, 240,  92, 255 }}, /* #F6F05C */
    { 9.90, {255, 128, 104, 255 }}, /* #FF8068 */
    {10.60, {255,  78, 111, 255 }}, /* #FF4E6F */
    {11.50, {198,  69, 184, 255 }}, /* #C645B8 */
    {12.40, {101,  99, 222, 255 }}  /* #6563DE */
};


static double clamp_double(
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


static unsigned char lerp_byte(
    unsigned char a,
    unsigned char b,
    double amount
)
{
    double value =
        (double)a +
        (
            (double)b -
            (double)a
        ) *
        amount;


    value =
        clamp_double(
            value,
            0.0,
            255.0
        );


    return (unsigned char)
        lround(value);
}


static Color lerp_color(
    Color a,
    Color b,
    double amount
)
{
    amount =
        clamp_double(
            amount,
            0.0,
            1.0
        );


    Color result =
    {
        lerp_byte(a.r, b.r, amount),
        lerp_byte(a.g, b.g, amount),
        lerp_byte(a.b, b.b, amount),
        lerp_byte(a.a, b.a, amount)
    };


    return result;
}


static Color sample_gradient(
    const GradientStop *stops,
    size_t stop_count,
    double position
)
{
    if (
        !stops ||
        stop_count == 0
    )
    {
        return WHITE;
    }


    if (
        position <=
        stops[0].position
    )
    {
        return stops[0].color;
    }


    if (
        position >=
        stops[
            stop_count - 1
        ].position
    )
    {
        return stops[
            stop_count - 1
        ].color;
    }


    for (
        size_t i = 0;
        i + 1 < stop_count;
        i++
    )
    {
        const GradientStop *left =
            &stops[i];

        const GradientStop *right =
            &stops[i + 1];


        if (
            position <
            left->position ||
            position >
            right->position
        )
        {
            continue;
        }


        double range =
            right->position -
            left->position;


        if (range <= 0.0)
            return right->color;


        double amount =
            (
                position -
                left->position
            ) /
            range;


        return lerp_color(
            left->color,
            right->color,
            amount
        );
    }


    return stops[
        stop_count - 1
    ].color;
}


static Color readable_text_color(
    Color background
)
{
    double brightness =
        0.299 *
        (double)background.r +

        0.587 *
        (double)background.g +

        0.114 *
        (double)background.b;


    if (brightness > 145.0)
    {
        return (Color)
        {
            18,
            19,
            20,
            255
        };
    }


    return (Color)
    {
        242,
        240,
        235,
        255
    };
}


const char *DanRankName(
    DanRank rank
)
{
    static const char *NAMES[
        DAN_RANK_COUNT
    ] =
    {
        "1ST",
        "2ND",
        "3RD",
        "4TH",
        "5TH",
        "6TH",
        "7TH",
        "8TH",
        "9TH",
        "10TH",

        "ALPHA",
        "BETA",
        "GAMMA",
        "DELTA",
        "EPSILON",
        "ZETA",
        "ETA",
        "THETA",
        "IOTA",
        "KAPPA"
    };


    if (
        rank < 0 ||
        rank >= DAN_RANK_COUNT
    )
    {
        return "UNKNOWN";
    }


    return NAMES[rank];
}


DanRank DanRankNext(
    DanRank rank
)
{
    int next =
        (int)rank + 1;


    if (
        next >=
        DAN_RANK_COUNT
    )
    {
        next = 0;
    }


    return (DanRank)next;
}


DanRank DanRankPrevious(
    DanRank rank
)
{
    int previous =
        (int)rank - 1;


    if (previous < 0)
    {
        previous =
            DAN_RANK_COUNT - 1;
    }


    return (DanRank)previous;
}


double DanRankPalettePosition(
    DanRank rank
)
{
    switch (rank)
    {
        case DAN_RANK_1ST:
            return 1.00;

        case DAN_RANK_2ND:
            return 2.00;

        case DAN_RANK_3RD:
            return 3.00;

        case DAN_RANK_4TH:
            return 4.00;

        case DAN_RANK_5TH:
            return 5.00;

        case DAN_RANK_6TH:
            return 6.00;

        case DAN_RANK_7TH:
            return 7.00;

        case DAN_RANK_8TH:
            return 8.00;

        case DAN_RANK_9TH:
            return 8.45;

        case DAN_RANK_10TH:
            return 8.90;

        default:
            break;
    }


    if (
        rank >= DAN_RANK_ALPHA &&
        rank <= DAN_RANK_KAPPA
    )
    {
        int index =
            (int)rank -
            (int)DAN_RANK_ALPHA;


        int count =
            (int)DAN_RANK_KAPPA -
            (int)DAN_RANK_ALPHA;


        if (count <= 0)
            return 9.0;


        double amount =
            (double)index /
            (double)count;


        return
            9.0 +
            amount *
            (
                12.4 -
                9.0
            );
    }


    return 1.0;
}


Color DanLazerDifficultyColor(
    double star_rating
)
{
    star_rating =
        round(
            star_rating *
            100.0
        ) /
        100.0;


    return sample_gradient(
        DIFFICULTY_SPECTRUM,
        sizeof(
            DIFFICULTY_SPECTRUM
        ) /
        sizeof(
            DIFFICULTY_SPECTRUM[0]
        ),
        star_rating
    );
}


Color DanLazerDifficultyTextColor(
    double star_rating
)
{
    star_rating =
        round(
            star_rating *
            100.0
        ) /
        100.0;


    return sample_gradient(
        DIFFICULTY_TEXT_SPECTRUM,
        sizeof(
            DIFFICULTY_TEXT_SPECTRUM
        ) /
        sizeof(
            DIFFICULTY_TEXT_SPECTRUM[0]
        ),
        star_rating
    );
}


DanTheme DanThemeForRank(
    DanRank rank
)
{
    double palette_position =
        DanRankPalettePosition(
            rank
        );


    DanTheme theme =
    {
        .accent =
        {
            232,
            229,
            223,
            255
        },

        .badge_background =
        {
            25,
            27,
            29,
            255
        },

        .badge_border =
        {
            0,
            0,
            0,
            255
        },

        .badge_text =
        {
            232,
            229,
            223,
            255
        },

        .graph =
        {
            232,
            229,
            223,
            255
        },

        .palette_position =
            palette_position,

        .elite_style =
            false
    };

    if (
        rank >= DAN_RANK_1ST &&
        rank <= DAN_RANK_7TH
    )
    {
        Color rank_color =
            DanLazerDifficultyColor(
                palette_position
            );


        theme.accent =
            rank_color;


        theme.badge_background =
            rank_color;


        theme.badge_border =
            (Color)
            {
                4,
                5,
                6,
                255
            };


        theme.badge_text =
            readable_text_color(
                rank_color
            );


        theme.graph =
            rank_color;


        theme.elite_style =
            false;


        return theme;
    }

    if (
        rank >= DAN_RANK_8TH &&
        rank <= DAN_RANK_10TH
    )
    {
        Color rank_color =
            DanLazerDifficultyColor(
                palette_position
            );


        theme.badge_background =
            rank_color;


        theme.badge_border =
            (Color)
            {
                5,
                6,
                7,
                255
            };


        theme.badge_text =
            (Color)
            {
                242,
                240,
                235,
                255
            };

        Color light_target =
        {
            215,
            218,
            255,
            255
        };


        double lighten_amount;


        switch (rank)
        {
            case DAN_RANK_8TH:
                lighten_amount =
                    0.38;

                break;


            case DAN_RANK_9TH:
                lighten_amount =
                    0.52;

                break;


            case DAN_RANK_10TH:
                lighten_amount =
                    0.64;

                break;


            default:
                lighten_amount =
                    0.50;

                break;
        }


        theme.accent =
            lerp_color(
                rank_color,
                light_target,
                lighten_amount
            );


        theme.graph =
            theme.accent;


        theme.elite_style =
            false;


        return theme;
    }

    theme.accent =
        DanLazerDifficultyTextColor(
            palette_position
        );


    theme.badge_background =
        (Color)
        {
            5,
            6,
            7,
            255
        };


    theme.badge_border =
        (Color)
        {
            0,
            0,
            0,
            255
        };


    theme.badge_text =
        theme.accent;


    theme.graph =
        theme.accent;


    theme.elite_style =
        true;


    return theme;
}