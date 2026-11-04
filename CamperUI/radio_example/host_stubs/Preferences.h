#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
class Preferences {
public:
    inline static std::map<std::string,std::vector<unsigned char>> store;
    inline static bool fail_write=false;
    bool begin(const char*,bool=false){return true;}
    void end(){}
    bool isKey(const char *key){return store.count(key)>0;}
    size_t getBytesLength(const char *key){return store[key].size();}
    size_t getBytes(const char *key,void *out,size_t size){auto &v=store[key];if(v.size()>size)return 0;memcpy(out,v.data(),v.size());return v.size();}
    size_t putBytes(const char *key,const void *p,size_t n){if(fail_write)return 0;store[key]=std::vector<unsigned char>((const unsigned char*)p,(const unsigned char*)p+n);return n;}
};
