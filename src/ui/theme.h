#ifndef DANOVERLAY_THEME_H
#define DANOVERLAY_THEME_H

#include <stdbool.h>

#include "raylib.h"


typedef enum
{
    DAN_RANK_1ST = 0,
    DAN_RANK_2ND,
    DAN_RANK_3RD,
    DAN_RANK_4TH,
    DAN_RANK_5TH,
    DAN_RANK_6TH,
    DAN_RANK_7TH,
    DAN_RANK_8TH,
    DAN_RANK_9TH,
    DAN_RANK_10TH,

    DAN_RANK_ALPHA,
    DAN_RANK_BETA,
    DAN_RANK_GAMMA,
    DAN_RANK_DELTA,
    DAN_RANK_EPSILON,
    DAN_RANK_ZETA,
    DAN_RANK_ETA,
    DAN_RANK_THETA,
    DAN_RANK_IOTA,
    DAN_RANK_KAPPA,

    DAN_RANK_COUNT
} DanRank;


typedef struct
{
    Color accent;

    Color badge_background;
    Color badge_border;
    Color badge_text;

    Color graph;

    double palette_position;

    bool elite_style;
} DanTheme;


const char *DanRankName(
    DanRank rank
);


DanRank DanRankNext(
    DanRank rank
);


DanRank DanRankPrevious(
    DanRank rank
);


double DanRankPalettePosition(
    DanRank rank
);


Color DanLazerDifficultyColor(
    double star_rating
);


Color DanLazerDifficultyTextColor(
    double star_rating
);


DanTheme DanThemeForRank(
    DanRank rank
);


#endif