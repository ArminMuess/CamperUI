#include "home_mp_activity.h"
constexpr bool activity_transitions() {
    HomeMpActivity a;
    if(a.update(true,false,false,0))return false; // absent startup data cannot show the tile
    if(!a.update(true,false,true,3))return false;
    if(!a.update(true,false,false,4))return false; // unknown feedback must not hide it
    if(!a.update(true,false,true,1))return false;
    if(a.update(true,false,true,4))return false;
    if(a.update(true,false,false,3))return false; // previously OFF remains OFF during missing data
    if(!a.update(true,false,true,2))return false;
    if(a.update(true,true,false,0))return false; // changing source discards prior visibility
    if(!a.update(true,true,true,1))return false;
    if(a.update(false,true,false,0))return false;
    return true;
}

static_assert(activity_transitions(),"Home MP tile follows confirmed modes and does not flicker on missing data");
