#include "energy_history_buffer.h"
constexpr bool startup_and_gaps() {
    EnergyHistoryBuffer h;
    if(h.ago(0,0)!=h.missing)return false;
    h.push({{88,-50,120}});
    if(h.bucket(0,60,60,59)!=88 || h.bucket(0,60,60,58)!=h.missing ||
        h.bucket(2,1440,120,119)!=120)return false;
    h.push({{h.missing,h.missing,h.missing}});
    return h.ago(0,0)==h.missing && h.ago(1,1)==-50 &&
        h.bucket(2,60,30,29)==h.missing;
}
constexpr bool aggregation_and_signs() {
    EnergyHistoryBuffer h;
    h.push({{40,-100,120}}); h.push({{60,100,200}});
    return h.bucket(0,60,30,29)==50 && h.bucket(1,60,30,29)==0 &&
        h.bucket(2,60,30,29)==160 && h.bucket(0,60,30,28)==h.missing;
}
constexpr bool ring_and_reset() {
    EnergyHistoryBuffer h;
    for(int i=0;i<1500;++i)h.push({{(int16_t)(i%101),(int16_t)i,(int16_t)i}});
    if(h.count!=1440 || h.ago(1,0)!=1499 || h.ago(1,1439)!=60 || h.ago(1,1440)!=h.missing)return false;
    h.reset(); h.push({{7,8,9}});
    return h.ago(0,0)==7 && h.ago(0,1)==h.missing;
}
constexpr bool bucket_coverage() {
    EnergyHistoryBuffer h;
    for(int i=0;i<1440;++i)h.push({{50,(int16_t)i,200}});
    return h.bucket(1,1440,120,0)==5 && h.bucket(1,1440,120,119)==1433 &&
        h.bucket(1,60,60,0)==1380 && h.bucket(1,60,60,59)==1439;
}
static_assert(startup_and_gaps(),"startup must remain empty, missing data must break lines");
static_assert(aggregation_and_signs(),"aggregation must retain signed watts and averages");
static_assert(ring_and_reset(),"24-hour rollover and source reset must not expose stale samples");
static_assert(bucket_coverage(),"short and long windows must cover the requested minutes exactly");
