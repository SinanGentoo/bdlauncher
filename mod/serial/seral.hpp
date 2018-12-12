#pragma once
#include <unistd.h>
#include<string>
#include<iostream>
#include<cstring>
#include<vector>
#include<list>
#include<cstdlib>
#include<iterator>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
using std::string;
using std::vector;
using std::list;
class DataStream{
    public:
    std::string dat;
    int curpos=0;
    void reset(){
        dat="";
        curpos=0;
    }
    const char* getData() const{
        return dat.c_str();
    }
    DataStream& operator<<(string const& c){
        (*this)<<(int)c.size();
        dat.append(c);
        return *this;
    }
    DataStream& operator<<(DataStream const& c){
        dat.append(c.getData());
        return *this;
    }
    template<typename T>
    DataStream& operator<<(vector<T> const& c){
        int size=c.size();
        (*this)<<size;
        for(auto const& i:c){
            (*this)<<i;
        }
        return *this;
    }
    template<typename T>
    DataStream& operator<<(list<T> const& c){
        int size=c.size();
        (*this)<<size;
        for(auto const& i:c){
            (*this)<<i;
        }
        return *this;
    }
    template<typename T,typename std::enable_if<!std::is_class<T>{},int>::type=0>
    DataStream& operator<<(T const& c){
        dat.append((const char*)&c,sizeof(c));
        return *this;
    }
    template<typename T,typename std::enable_if<std::is_class<T>{},int>::type=0>
    DataStream& operator<<(T const& c){
        c.packto(*this);
        return *this;
    }
    template<typename T,typename T2>
    DataStream& operator<<(unordered_map<T,T2> const & c){
        operator<<((int)c.size());
        for(auto& i:c){
            operator<<(i.first);
            operator<<(i.second);
        }
        return *this;
    }
    template<typename T,typename T2>
    DataStream& operator>>(unordered_map<T,T2> & c){
        int sz;
        operator>>(sz);
        c.reserve(sz);
        for(int i=0;i<sz;++i){
            T tmp;
            T2 tmp2;
            operator>>(tmp);
            operator>>(tmp2);
            c[tmp]=tmp2;
        }
        return *this;
    }
    DataStream& operator>>(string& c){
        int sz;
        (*this)>>sz;
        c.reserve(sz);
        c.assign(dat.data()+curpos,sz);
        curpos+=sz;
        return *this;
    }
    template<typename T,typename std::enable_if<!std::is_class<T>{},int>::type=0>
    DataStream& operator>>(T& c){
        memcpy(&c,dat.data()+curpos,sizeof(c));
        curpos+=sizeof(c);
        return *this;
    }
    template<typename T,typename std::enable_if<std::is_class<T>{},int>::type=0>
    DataStream& operator>>(T& c){
        c.unpack(*this);
        return *this;
    }
    template<typename T>
    DataStream& operator>>(vector<T> & c){
        int size;
        (*this)>>size;
        c.reserve(size);
        for(int i=0;i<size;++i){
            T tmp;
            (*this)>>tmp;
            c.emplace_back(tmp);
        }
        return *this;
    }
    template<typename T>
    DataStream& operator>>(list<T> & c){
        int size;
        (*this)>>size;
        for(int i=0;i<size;++i){
            T tmp;
            (*this)>>tmp;
            c.emplace_back(tmp);
        }
        return *this;
    }
};
static void file2mem(const char* fn,char** mem,int& sz){
    int fd=open(fn,O_RDONLY);
    if(fd==-1){
        printf("[ERROR] cannot open %s\n",fn);
        perror("open");
        *mem=nullptr;
        return;
    }
    sz=lseek(fd,0,SEEK_END);
    *mem=(char*)malloc(sz+1);
    pread(fd,*mem,sz,0);
    (*mem)[sz]=0; //auto zero-terminated string
    close(fd);
}
