#pragma once
#include<string_view>
#include <string>
using std::string_view;
template<int sz=1024>
struct SPBuf{
    char buf[sz];
    int ptr;
    SPBuf():ptr(0){}
    std::string getstr(){
        return std::string(buf,ptr);
    }
    inline string_view get(){
        return {buf,(size_t)ptr};
    }
    void clear(){
        ptr=0;
    }
    inline string_view write(string_view sv){
        if(sv.size()>sz-ptr) return {buf,(size_t)ptr};
        memcpy(buf+ptr,sv.data(),sv.size());
        ptr+=sv.size();
        return {buf,(size_t)ptr};
    }
    string_view write(const char* cs){
        auto csz=strlen(cs);
        if(csz>sz-ptr) return {buf,(size_t)ptr};
        memcpy(buf+ptr,cs,csz);
        ptr+=csz;
        return {buf,(size_t)ptr};
    }
    template <typename... Params>
    inline string_view write(Params &&... params){
        ptr+=snprintf(buf+ptr,sz-ptr,std::forward<Params>(params)...);
        return {buf,(size_t)ptr};
    }
};