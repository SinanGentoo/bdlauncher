#pragma once
#include"stkbuf.hpp"
#include "../cmdhelper.h"
#include <string>
#include <unordered_map>
#include <MC.h>
#include <functional>
#include <vector>
//#include "db.hpp"
#include "stl.hpp"
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
typedef static_deque<string_view,64> argVec;

BDL_EXPORT void sendText(Player *a, string_view ct, TextType type = RAW);
BDL_EXPORT void broadcastText(Player* a,string_view ct,TextType type = RAW);
#define sendText2(a, b) sendText(a, b, JUKEBOX_POPUP)
BDL_EXPORT void TeleportA(Actor &a, Vec3 b, AutomaticID<Dimension, int> c);
//BDL_EXPORT Player* getplayer_byname(const string& name);
BDL_EXPORT ServerPlayer *getplayer_byname2(string_view name);
BDL_EXPORT void get_player_names(vector<string> &a);
BDL_EXPORT void KillActor(Actor *a);
BDL_EXPORT MCRESULT runcmd(string_view a);
BDL_EXPORT MCRESULT runcmdAs(string_view a, Player *sp);
BDL_EXPORT Minecraft *_getMC();

BDL_EXPORT void split_string(string_view s, std::vector<std::string_view> &v, string_view c);
BDL_EXPORT void split_string(string_view s, static_deque<std::string_view> &v, string_view c);
BDL_EXPORT bool execute_cmdchain(string_view chain_, string_view sp, bool chained = true);
BDL_EXPORT void register_cmd(const std::string &name, void (*fn)(argVec&,CommandOrigin const &,CommandOutput &outp), const std::string &desc = "mod command", int permlevel = 0);
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

#include <leveldb/db.h>
struct LDBImpl
{
    //#ifdef BASE
    leveldb::DB *db;
    leveldb::ReadOptions rdopt;
    leveldb::WriteOptions wropt;
    //#else
    //char filler[sizeof(leveldb::ReadOptions)+sizeof(leveldb::WriteOptions)+sizeof(leveldb::DB *)+8];
    //#endif
    void load(const char *name, bool read_cache=true);
    LDBImpl(const char *name, bool read_cache=true);
    void close();
    ~LDBImpl();
    bool Get(string_view key, string &val) const;
    void Put(string_view key, string_view val);
    bool Del(string_view key);
    void Iter(function<void(string_view, string_view)> fn) const;
    void CompactAll();
};

using std::pair;
#define BDL_TAG "V2019-12-27"