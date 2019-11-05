#include<list>
#include<unordered_map>
#include"../base/db.hpp"
#include"../serial/seral.hpp"
#include<sys/cdefs.h>
#define access(ptr,type,off) (*((type*)(((uintptr_t)ptr)+off)))

using std::swap;
using std::list;
using std::unordered_map;
#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)<(b))?(a):(b))
#define unlikely(cond) __glibc_unlikely(!!(cond))
#define likely(cond) __glibc_likely(!!(cond))
LDBImpl db("data_v2/land");
enum LandPerm:char{
    PERM_NULL=0,
    PERM_OWNER=1,
    PERM_USE=2,
    PERM_ATK=4,
    PERM_BUILD=8,
    PERM_POPITEM=16,
    PERM_INTERWITHACTOR=32
};
typedef unsigned int lpos_t;
#pragma pack(1)
struct FastLand{
    lpos_t x,z,dx,dz;
    uint lid;
    short refcount;
    char dim;
    LandPerm perm;
    int owner_sz;
    char owner[0];
    int chkOwner(string x){
        //x="|"+x+"|";
        int sz=x.size();
        auto dat=x.data();
        //printf("owner sz %d\n",owner_sz);
        for(int i=1;i<owner_sz-sz;++i){
            if(memcmp(dat,owner+i,sz)==0 && owner[i-1]=='|' && owner[i+sz]=='|'){
                return i==1?2:1;
            }
        }
        return 0;
    }
    bool hasPerm(const string& x,LandPerm PERM){
        if(PERM==PERM_OWNER){
            return chkOwner(x)==2;
        }
        return chkOwner(x)?true:(perm&PERM);
    }
    int memsz(){
        return sizeof(FastLand)+owner_sz+1;
    }
};
#pragma pack(8)
static_assert(sizeof(FastLand)==28);
static_assert(offsetof(FastLand,dim)==22);
static_assert(offsetof(FastLand,refcount)==20);
struct DataLand{
    lpos_t x,z,dx,dz;
    uint lid;
    short refcount;
    char dim;
    LandPerm perm;
    string owner;
    void addOwner(string x,bool SuperOwner=false){
        x="|"+x+"|";
        if(SuperOwner){
            owner=x+owner;
            return;
        }
        owner+=x;
    }
    void delOwner(const string& x){
        auto pos=owner.find("|"+x+"|");
        if(pos!=string::npos){
            owner.erase(pos,x.size()+2);
        }
    }
    void packto(DataStream& ds) const{
        ds<<x<<z<<dx<<dz<<lid<<(short)0<<dim<<perm<<owner;
    }
    void unpack(DataStream& ds){
        short pad;
        ds>>x>>z>>dx>>dz>>lid>>pad>>dim>>perm>>owner;
    }
};
static_assert(sizeof(DataLand)==24+sizeof(string));
static_assert(offsetof(DataLand,dim)==22);
struct LandCacheManager{
    unordered_map<int,FastLand*> cache;
    void noticeFree(FastLand* fl){
        //printf("notice free %d rcnt %d\n",fl->lid,fl->refcount);
        fl->refcount--;
        if(fl->refcount<=0){
            cache.erase(fl->lid);
            delete fl;
        }
    }
    FastLand* requestLand(int id){
        //printf("req %d\n",id);
        auto it=cache.find(id);
        FastLand* res;
        if(it==cache.end()){
            string landstr;
            db.Get("l_"+string((char*)&id,4),landstr);
            res=(FastLand*)malloc(landstr.size()+1);
            memcpy(res,landstr.data(),landstr.size());
            res->owner[res->owner_sz]=0;
            res->refcount=0;
            cache[id]=res;
        }else{
            res=it->second;
        }
        res->refcount++;
        //printf("req %d rcnt %d lp %p\n",id,res->refcount,res);
        return res;
    }
} LCMan;
struct ChunkLandManager{
    lpos_t xx,zz;
    char dim;
    FastLand* lands[16][16];
    list<FastLand*> managed_lands;
    void reset(){
        for(auto i:managed_lands){
            LCMan.noticeFree(i);
        }
        managed_lands.clear();
    }
    void init(int* landlist,int siz){
        memset(lands,0,sizeof(lands));
        for(int i=0;i<siz;++i){
            auto fl=LCMan.requestLand(landlist[i]);
            managed_lands.push_back(fl);
            
            /*for(int dx=0;dx<16;++dx){
                for(int dz=0;dz<16;++dz){
                    if(((xx<<4)|dx)>=fl->x && ((xx<<4)|dx)<=fl->dx && ((zz<<4)|dz)<=fl->dz && ((zz<<4)|dz)>=fl->z){
                        printf("push %d %d %s\n",dx,dz,fl->owner);
                        lands[dx][dz]=fl;
                    }
                }
            }*/
            lpos_t sx,dx,sz,dz;
            if((fl->x>>4)==xx) sx=fl->x&15; else sx=0;
            if((fl->z>>4)==zz) sz=fl->z&15; else sz=0;
            if((fl->dx>>4)==xx) dx=fl->dx&15; else dx=15;
            if((fl->dz>>4)==zz) dz=fl->dz&15; else dz=15;
            //printf("%d %d %d %d %d %d %d %d\n",fl->x,fl->z,fl->dx,fl->dz,sx,sz,dx,dz);
            for(lpos_t i=sx;i<=dx;++i){
                for(lpos_t j=sz;j<=dz;++j){
                    lands[i][j]=fl;
                    //printf("push %d %d %s\n",i,j,fl->owner);
                }
            }
        }
    }
    void load(lpos_t x,lpos_t z,int di){
        string val;
        char buf[100];
        xx=x;zz=z;dim=di;
        memcpy(buf,&x,4);
        memcpy(buf+4,&z,4);
        buf[8]=dim;
        auto suc=db.Get(string(buf,9),val);
        //printf("cpos %d %d %d\n",x,z,suc);
        init((int*)val.data(),val.size()/4);
        //printf("load with %d\n",managed_lands.size());
    }
};
list<ChunkLandManager*> lru_cman;
ChunkLandManager* getChunkMan(lpos_t x,lpos_t z,int dim){
    ChunkLandManager* res;
    for(auto it=lru_cman.begin();it!=lru_cman.end();++it){
        if((*it)->xx==x && (*it)->zz==z && (*it)->dim==dim){
            res=*it;
            lru_cman.erase(it);
            lru_cman.push_front(res);
            return res;
        }
    }
    res=(lru_cman.back());
    lru_cman.pop_back();
    res->reset();
    res->load(x,z,dim);
    //printf("reset back %d %d\n",lru_cman.front()->managed_lands.size(),res->managed_lands.size());
    lru_cman.push_front(res);
    return res;
}
inline lpos_t to_lpos(int p){
    return p+200000;
} 
FastLand* getFastLand(int x,int z,int dim){
    lpos_t xx,zz;
    xx=to_lpos(x);
    zz=to_lpos(z);
    auto cm=getChunkMan(xx>>4,zz>>4,dim);
    return cm->lands[xx&15][zz&15];
}

#define LRUCM_SZ 256
void init_cache(){
    for(int i=0;i<LRUCM_SZ;++i) {auto cm=new ChunkLandManager;cm->xx=0xffffffff;lru_cman.push_back(cm);}
}
void purge_cache(){
    for(auto i:lru_cman){
        i->xx=0xffffffff;
        i->reset();
    }
    assert(LCMan.cache.size()==0);
}
bool generic_perm(int x,int z,int dim,LandPerm perm,const string& name){
    auto ld=getFastLand(x,z,dim);
    if(unlikely(ld)){
        return ld->hasPerm(name,perm);
    }
    return true;
}
uint getLandUniqid(){
    string val;
    uint id=0;
    if(!db.Get("land_id",val)){
        db.Put("land_id",string((char*)&id,4));
        return 0;
    }else{
        id=access(val.data(),uint,0);
        id++;
        db.Put("land_id",string((char*)&id,4));
        return id;
    }
}
void proc_chunk_add(lpos_t x,lpos_t dx,lpos_t z,lpos_t dz,int dim,uint lid){
    char buf[9];
    x>>=4;
    dx>>=4;
    z>>=4;
    dz>>=4;
    buf[8]=dim;
    for(int i=x;i<=dx;++i){
        for(int j=z;j<=dz;++j){
            //printf("proc add %d %d %d\n",i,j,dim);
            memcpy(buf,&i,4);
            memcpy(buf+4,&j,4);
            string key(buf,9);
            string val;
            db.Get(key,val);
            val.append((char*)&lid,4);
            //printf("put key %d %d\n",key.size(),val.size());
            db.Put(key,val);
        }
    }
}
void proc_chunk_del(lpos_t x,lpos_t dx,lpos_t z,lpos_t dz,int dim,uint lid){
    char buf[9];
    x>>=4;
    dx>>=4;
    z>>=4;
    dz>>=4;
    buf[8]=dim;
    for(int i=x;i<=dx;++i){
        for(int j=z;j<=dz;++j){
            memcpy(buf,&i,4);
            memcpy(buf+4,&j,4);
            string key(buf,9);
            //printf("proc del %d %d %d\n",i,j,dim);
            string val;
            db.Get(key,val);
            //printf("size %d access %u\n",val.size(),access(val.data(),uint,0));
            for(int i=0;i<val.size();i+=4){
                if(access(val.data(),uint,i)==lid){
                    //printf("erase %d\n",i);
                    val.erase(i,4);
                    break;
                }
            }
            db.Put(key,val);
        }
    }
}
void addLand(lpos_t x,lpos_t dx,lpos_t z,lpos_t dz,int dim,const string& owner,LandPerm perm=PERM_NULL){
    DataLand ld;
    ld.x=x,ld.z=z,ld.dx=dx,ld.dz=dz,ld.dim=dim;
    ld.owner=owner[0]=='|'?owner:('|'+owner+'|');
    ld.perm=perm;
    auto lid=getLandUniqid();
    ld.lid=lid;
    string key="l_"+string((char*)&lid,4);
    DataStream ds;
    ds<<ld;
    //printf("sss %d\n",ds.dat.size());
    db.Put(key,ds.dat);
    proc_chunk_add(x,dx,z,dz,dim,lid);
    purge_cache();
}
void updLand(DataLand& ld){
    auto lid=ld.lid;
    DataStream ds;
    string key="l_"+string((char*)&lid,4);
    ds<<ld;
    db.Put(key,ds.dat);
    purge_cache();
}
void removeLand(FastLand* land){
    string key="l_"+string((char*)&(land->lid),4);
    db.Del(key);
    proc_chunk_del(land->x,land->dx,land->z,land->dz,land->dim,land->lid); //BUG HERE
    purge_cache();
}
void Fland2Dland(FastLand* ld,DataLand& d){
    memcpy(&d,ld,24);
    d.owner=string(ld->owner,ld->owner_sz);
}
/*
int main(){
    init_cache();
    addLand(to_lpos(1),to_lpos(1),to_lpos(2),to_lpos(2),1,"nm");
    addLand(to_lpos(3),to_lpos(5),to_lpos(3),to_lpos(4),1,"sl");
    auto lp=getFastLand(1,2,1);
    removeLand(lp);
    //printf("%d\n",generic_perm(3,4,1,PERM_OWNER,"sl"));
    for(int i=5;i<=5+1000000;++i){
        generic_perm(5,i,1,PERM_OWNER,"sl");
    }
    //printf("%d\n",generic_perm(5,4,1,PERM_OWNER,"sl"));
    //purge_cache();
    while(1){
        int a,b,c,f;
        scanf("%d %d %d",&a,&b,&c);
        printf("%d\n",generic_perm(a,b,c,PERM_OWNER,"nm"));
    }
}*/
