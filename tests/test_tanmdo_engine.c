#include "engine/ln_course.h"
#include "engine/reform_rank.h"
#include "engine/ruler_sanity.h"
#include "calibration/mania4k_calibration.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

/* 
 * rate-only changes retain the same structural features and chart checksum.
 * verify the played-time release contribution independently of the UI/Tosu. 
 */
int main(void)
{
    const ChartFeatures chart = {
        .ln_ratio = 0.75,
        .hold_occupancy = 0.72,
        .simultaneous_hold = 0.43,
        .release_density = 8.0,
        .ln_duration_cv = 0.4
    };
    LnCourseResult original = {0}, same_rate = {0}, dt = {0}, ht = {0};
    assert(LnCourseEvaluate(6.04, &chart, NULL, &original));
    assert(LnCourseEvaluateAtRate(6.04, &chart, NULL, 1.0, &same_rate));
    assert(LnCourseEvaluateAtRate(6.04, &chart, NULL, 1.4, &dt));
    assert(LnCourseEvaluateAtRate(6.04, &chart, NULL, 0.75, &ht));
    assert(fabs(original.dp - same_rate.dp) < 1e-9);
    assert(dt.dp > original.dp && ht.dp < original.dp);
    assert(fabs((dt.dp - original.dp) -
        MANIA4K_CALIBRATION.ln_course.regression.release_density * 8.0 * 0.4) < 1e-5);
    assert(!LnCourseEvaluateAtRate(6.04, &chart, NULL, NAN, &dt));

    const RulerSanityResult supports_jack = {
        .msd_top_ruler = REFORM_RULER_JACK,
        .jack_support = 22,
        .speed_support = 20,
        .top_support = 22,
        .second_support = 20,
        .top_to_second_ratio = 1.10
    };
    assert(TanMdoRulerSelect(REFORM_RULER_GENERAL, CHART_FAMILY_JACK,
        0.34, &supports_jack) == REFORM_RULER_JACK);
    assert(TanMdoRulerSelect(REFORM_RULER_GENERAL, CHART_FAMILY_JACK,
        0.28, &supports_jack) == REFORM_RULER_GENERAL);
    assert(TanMdoRulerSelect(REFORM_RULER_GENERAL, CHART_FAMILY_HYBRID,
        0.70, &supports_jack) == REFORM_RULER_GENERAL);
    const RulerSanityResult contradicts_jack = {
        .msd_top_ruler = REFORM_RULER_SPEED,
        .jack_support = 10,
        .speed_support = 20,
        .top_support = 20,
        .second_support = 10,
        .top_to_second_ratio = 2.0
    };
    assert(TanMdoRulerSelect(REFORM_RULER_GENERAL, CHART_FAMILY_JACK,
        0.60, &contradicts_jack) == REFORM_RULER_GENERAL);
    assert(TanMdoRulerSelect(REFORM_RULER_STAMINA, CHART_FAMILY_JACK,
        0.60, &supports_jack) == REFORM_RULER_STAMINA);

    ReformRankResult rice_base = {0}, rice_tanmdo = {0};
    assert(ReformRankEvaluate(7.0, REFORM_RULER_GENERAL, &rice_base));
    assert(ReformRankEvaluate(7.0,
        TanMdoRulerSelect(REFORM_RULER_GENERAL, CHART_FAMILY_JACK, 0.34,
            &supports_jack), &rice_tanmdo));
    assert(rice_base.input_sr == rice_tanmdo.input_sr);
    assert(rice_tanmdo.ruler == REFORM_RULER_JACK);
    puts("TanMDO LN rate and ruler selection checks passed");
    return 0;
}
