#pragma once
#include<string_view>
#include<string>
using std::string_view;
using std::string;
template<typename T,const int S=96>
struct static_deque{
    T data[S];
    uint head,tail;
    inline T* begin(){
        return data+head;
    }
    inline T* end(){
        return data+tail;
    }
    inline void clear(){
        head=tail=0;
    }
    static_deque(){
        clear();
    }
    inline void push_back(const T& a){
        data[tail++]=a;
    }
    inline void emplace_back(const T& a){ //dummy
        data[tail++]=a;
    }
    inline T& top(){
        return data[head];
    }
    inline void pop_top(){
        head++;
    }
    inline T& back(){
        return data[tail-1];
    }
    inline void pop_back(){
        tail--;
    }
    inline int size() const{
        return tail-head;
    }
    inline bool empty(){return size()==0;}
    inline bool full(){return size()==S;}
    inline const T& operator[](const int idx) const{
        return data[idx];
    }
    inline T& operator[](const int idx){
        return data[idx];
    }
    inline int count(const T& x) const{
        int cnt;
        for(int i=head;i!=tail;++i){
            if(data[i]==x) cnt++;
        }
        return cnt;
    }
    inline int has(const T& x) const{
        for(int i=head;i!=tail;++i){
            if(data[i]==x) return true;
        }
        return false;
    }
};
template<typename T,int INIT_SIZE=8,int MAX_SIZE=64>
struct AllocPool{
    static_deque<T*,MAX_SIZE> sq;
    AllocPool(){
        for(int i=0;i<INIT_SIZE;++i){
            auto x=malloc(sizeof(T));
            sq.push_back((T*)x);
        }
    }
    T* __get(){
        if(sq.empty()){
            return (T*)malloc(sizeof(T));
        }else{
            auto r=sq.back();
            sq.pop_back();
            return r;
        }
    }
    void __free(T* x){
        if(sq.full()){
            free(x);
        }else{
            sq.push_back(x);
        }
    }
    template <typename... Params>
    T* get(Params &&... params){
        auto res=__get();
        return new (res)T(std::forward<Params>(params)...);
    }
    void release(T* x){
        x->~T();
        __free(x);
    }
};


int atoi(string_view sv){
    int res=0;
    int fg=0;
    const char* c=sv.data();
    int sz=sv.size();
    for(int i=0;i<sz;++i){
        if(c[i]=='-'){
            fg=1;
        }
        if(!(c[i]>='0' && c[i]<='9')) continue;
        res*=10;
        res+=c[i]-'0';
    }
    return fg?-res:res;
}
typedef unsigned long STRING_HASH;

STRING_HASH do_hash(string_view x){
    auto sz=x.size();
    auto c=x.data();
    unsigned int hash1=0;unsigned int hash2=0;
    for(int i=0;i<sz;++i){
        hash1=(hash1*47+c[i])%1000000007;
        hash2=(hash2*83+c[i])%1000000009;
    }
    return (((STRING_HASH)hash1)<<32)|hash2;
}
/*
struct MStr{

    union v
    {
        string str;
        string_view sv;
    };
    
}
template<typename K,typename V>
struct StrMap{
    std::unordered_map<string_view,V> mp;
    AllocPool<string,4,64> AP;
    StrMap(){}
    int count(string_view sv) const{
        return mp.find(sv)!=mp.end();
    }
    auto begin(){
        return mp.begin();
    }
    auto end(){
        return mp.end();
    }
    std::size_t size() const{
        return mp.size();
    }
    auto find(string_view sv){
        return mp.find(sv);
    }
    auto find(string_view sv) const{
        return mp.find(sv);
    }
    V&& operator(string_view sv){
        auto it=mp.find(sv);
        if(it==mp.end()){
            //build new
            auto strAP.get
        }
    }
};*/
/*
template<typename K,typename V>
class FastMap{
    typedef unsigned long HASH_T;
    struct Node{
        struct Node* prev;
        struct Node* next;
        K key;
        V val;
    };
    Node* pool;
    unsigned int pool_sz;
    unsigned int elem_sz;
    FastMap(){
        pool=malloc(4*sizeof(Node));
        
    }
}*/
