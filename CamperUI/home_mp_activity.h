#pragma once
// Remember confirmed activity during temporary loss of GX data; never guess on boot.
struct HomeMpActivity {
    bool visible=false,source_known=false,demo=false;
    constexpr bool update(bool enabled,bool source_demo,bool mode_known,int mode) {
        if(!enabled){visible=false;source_known=false;return false;}
        if(source_known && demo!=source_demo)visible=false;
        source_known=true;demo=source_demo;
        if(mode_known && mode>=1 && mode<=4)visible=mode!=4;
        return visible;
    }
};
