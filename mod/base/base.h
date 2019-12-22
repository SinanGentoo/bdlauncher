#pragma once
#include"stkbuf.hpp"
#include "../cmdhelper.h"
#include <string>
#include <unordered_map>
#include <MC.h>
#include <functional>
#include <vector>
#include "db.hpp"
#include<string_view>
using namespace std::literals;
using std::string_view;
using std::function;
using std::string;
using std::vector;

class Player;
class Actor;
class Vec3;
//class AutomaticID<Dimension,int>;
class Dimension;
struct MCRESULT;

enum TextType : char
{
    RAW = 0,
    POPUP = 3,
    JUKEBOX_POPUP = 4,
    TIP = 5
};
BDL_EXPORT void sendText(Player *a, string_view ct, TextType type = RAW);
#define sendText2(a, b) sendText(a, b, JUKEBOX_POPUP)
BDL_EXPORT void TeleportA(Actor &a, Vec3 b, AutomaticID<Dimension, int> c);
//BDL_EXPORT Player* getplayer_byname(const string& name);
BDL_EXPORT Player *getplayer_byname2(string_view name);
BDL_EXPORT void get_player_names(vector<string> &a);
BDL_EXPORT void KillActor(Actor *a);
BDL_EXPORT MCRESULT runcmd(string_view a);
BDL_EXPORT MCRESULT runcmdAs(string_view a, Player *sp);
BDL_EXPORT Minecraft *_getMC();

BDL_EXPORT void split_string(string_view s, std::vector<std::string_view> &v, string_view c);
BDL_EXPORT bool execute_cmdchain(string_view chain_, string_view sp, bool chained = true);
BDL_EXPORT void register_cmd(const std::string &name, void (*fn)(std::vector<string_view>&,CommandOrigin const &,CommandOutput &outp), const std::string &desc = "mod command", int permlevel = 0);
BDL_EXPORT void register_cmd(const std::string &name,void (*fn)(void), const std::string &desc = "mod command", int permlevel = 0);
BDL_EXPORT void reg_useitemon(bool(*)(GameMode *a0, ItemStack *a1, BlockPos const *a2, BlockPos const *dstPos, Block const *a5));
BDL_EXPORT void reg_destroy(bool(*)(GameMode *a0, BlockPos const *));
BDL_EXPORT void reg_attack(bool(*)(ServerPlayer *a0, Actor *a1));
BDL_EXPORT void reg_chat(bool(*)(ServerPlayer const *a0, string &payload));
BDL_EXPORT void reg_player_join(void(*)(ServerPlayer *));
BDL_EXPORT void reg_player_left(void(*)(ServerPlayer *));
BDL_EXPORT void reg_pickup(bool(*)(Actor *a0, ItemActor *a1));
BDL_EXPORT void reg_popItem(bool(*)(ServerPlayer &, BlockPos &));
BDL_EXPORT void reg_mobhurt(bool(*)(Mob &, ActorDamageSource const &, int &));
BDL_EXPORT void reg_mobdie(void(*)(Mob &, ActorDamageSource const &));
BDL_EXPORT void reg_actorhurt(bool(*)(Actor &, ActorDamageSource const &, int &));
BDL_EXPORT void reg_interact(bool(*)(GameMode *, Actor &));

BDL_EXPORT int getPlayerCount();
BDL_EXPORT int getMobCount();

BDL_EXPORT NetworkIdentifier *getPlayerIden(ServerPlayer &sp);
BDL_EXPORT ServerPlayer *getuser_byname(string_view a);
#define getplayer_byname(x) (getuser_byname(x))

BDL_EXPORT void forceKickPlayer(ServerPlayer &sp);

#define ARGSZ(b)      \
    if (a.size() < b) \
    { \
char buf[256]; \
sprintf(buf,"check your arg.%d required ,%d found\n", b, a.size()); \
outp.error(buf); \
return; \
}

#define SafeStr(a) ("\"" + (a) + "\"")
static Minecraft *McCache;
static Level *LvCache;
static inline Minecraft *getMC()
{
    return McCache ? McCache : (McCache = _getMC());
}
static inline Level *getSrvLevel()
{
    return LvCache ? LvCache : (LvCache = _getMC()->getLevel());
}
static inline bool isOp(ServerPlayer const *sp)
{
    return (int)sp->getPlayerPermissionLevel() > 1;
}
static inline bool isOp(CommandOrigin const &sp)
{
    return (int)sp.getPermissionsLevel() > 0;
}
template<typename T,int S=128>
struct static_deque{
    T data[S];
    uint head,tail;
    T* begin(){
        return data+head;
    }
    T* end(){
        return data+tail;
    }
    void clear(){
        head=tail=0;
    }
    static_deque(){
        clear();
    }
    void push_back(const T& a){
        data[tail++]=a;
    }
    void emplace_back(const T& a){ //dummy
        data[tail++]=a;
    }
    T& top(){
        return data[head];
    }
    void pop_top(){
        head++;
    }
    int size(){
        return head-tail;
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
        if(!isdigit(c[i])) continue;
        res*=10;
        res+=c[i]-'0';
    }
    return fg?-res:res;
}
typedef unsigned long STRING_HASH;

STRING_HASH do_hash(string_view x){
    auto sz=x.size();
    auto c=x.data();
    uint hash1=0;uint hash2=0;
    for(int i=0;i<sz;++i){
        hash1=(hash1*47+c[i])%1000000007;
        hash2=(hash2*83+c[i])%1000000009;
    }
    return (((STRING_HASH)hash1)<<32)|hash2;
}
#define BDL_TAG "V2019-12-22"