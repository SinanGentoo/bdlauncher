#include <cstdio>
#include <list>
#include <forward_list>
#include <string>
#include <unordered_map>
#include "../cmdhelper.h"
#include <vector>
#include <Loader.h>
#include <MC.h>
#include "seral.hpp"
#include "../serial/seral.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../money/money.h"
#include "../base/base.h"
#include <cmath>
#include <deque>
#include <dlfcn.h>
#include <tuple>
#include "rapidjson/document.h"
#include <fstream>
#include "../gui/gui.h"
#include "data.hpp"

using std::deque;
using std::string;
#define dbg_printf(...) \
    {                   \
    }
//#define dbg_printf printf

extern "C"
{
    BDL_EXPORT void mod_init(std::list<string> &modlist);
}
extern void load_helper(std::list<string> &modlist);
struct LPOS
{
    int x, z;
};
static std::unordered_map<STRING_HASH, LPOS> startpos, endpos;
static unordered_map<Shash_t, int> choose_state;

int LAND_PRICE, LAND_PRICE2;
static bool land_tip = true;
using namespace rapidjson;
static void loadcfg()
{
    Document dc;
    char *buf;
    int siz;
    file2mem("config/land.json", &buf, siz);
    if (dc.ParseInsitu(buf).HasParseError())
    {
        printf("[LAND] Config JSON ERROR!\n");
        exit(1);
    }
    LAND_PRICE = dc["buy_price"].GetInt();
    LAND_PRICE2 = dc["sell_price"].GetInt();
    if (dc.HasMember("land_tip"))
        land_tip = dc["land_tip"].GetBool();
    if(getenv("NO_LTIP")) land_tip=false;
    free(buf);
}
static void oncmd(argVec &a, CommandOrigin const &b, CommandOutput &outp)
{
    auto nm = b.getName();
    auto sp = getSP(b.getEntity());
    bool op = isOp(b);
    SPBuf sb;
    ARGSZ(1)
    if(a[0]=="fix"){
        db.Del("land_fixed_v3");
        outp.success("scheduled data fix at next server start");
        return;
    }
    if (a[0] == "exit")
    {
        if (!sp)
            return;
        choose_state.erase(sp->getNameTagAsHash());
        outp.success("§bExit selection mode, please input /land buy");
    }
    if (a[0] == "a")
    {
        if (!sp)
            return;
        choose_state[sp->getNameTagAsHash()] = 1;
        outp.success("§bEnter selection mode, please click on the ground to select point A");
    }
    if (a[0] == "b")
    {
        if (!sp)
            return;
        choose_state[sp->getNameTagAsHash()] = 2;
        outp.success("§bPlease click on the ground to select point B");
    }
    if (a[0] == "query")
    {
        auto &pos = b.getEntity()->getPos();
        int dim = b.getEntity()->getDimensionId();
        auto lp = getFastLand(pos.x, pos.z, dim);
        if (lp)
        {
            char buf[1000];
            snprintf(buf, 1000, "§bThis is %s's land", lp->owner);
            outp.success(buf);
        }
        else
        {
            outp.error("No land here");
            return;
        }
    }
    if (a[0] == "buy")
    {
        auto hash = sp->getNameTagAsHash();
        int x, z, dx, dz;
        int dim = sp->getDimensionId();
        if (startpos.count(hash) + endpos.count(hash) != 2)
        {
            outp.error("Choose 2 points please.");
            return;
        }
        choose_state.erase(hash);
        x = min(startpos[hash].x, endpos[hash].x);
        z = min(startpos[hash].z, endpos[hash].z);
        dx = max(startpos[hash].x, endpos[hash].x);
        dz = max(startpos[hash].z, endpos[hash].z);
        //step -1 :sanitize pos

        //step 1 check money
        int deltax = dx - x + 1, deltaz = dz - z + 1;
        uint siz = deltax * deltaz;
        if (deltax >= 4096 || deltaz >= 4096 || siz >= 5000000)
        {
            outp.error("Too big land");
            return;
        }
        int price = siz * LAND_PRICE;
        if (price <= 0 || price >= 500000000)
        {
            outp.error("Too big land");
            return;
        }
        auto mon = get_money(nm);
        if (mon < price)
        {
            outp.error("Money not enough");
            return;
        }
        //step 2 check coll
        for (int i = x; i <= dx; ++i)
            for (int j = z; j <= dz; ++j)
            {
                auto lp = getFastLand(i, j, dim);
                if (lp)
                {
                    sb.write("Land collision detected! hint: "sv);
                    sb.write(lp->getOwner());
                    sb.write("'s land"sv);
                    outp.error(sb.getstr());
                    return;
                }
            }
        //step 3 add land
        set_money(nm, mon - price);
        addLand((x)^0x80000000, (dx)^0x80000000, (z)^0x80000000, (dz)^0x80000000, dim, nm);
        outp.success("§bSuccessful land purchase");
    }
    if (a[0] == "sell")
    {
        auto &pos = sp->getPos();
        int dim = sp->getDimensionId();
        auto lp = getFastLand(pos.x, pos.z, dim);
        if (lp && (lp->chkOwner(nm) == 2 || op))
        {
            int siz = (lp->dx - lp->x) * (lp->dz - lp->z);
            add_money(nm, siz * LAND_PRICE2);
            removeLand(lp);
            outp.success("§bYour land has been sold");
        }
        else
        {
            outp.error("No land here or not your land");
            return;
        }
    }
    if (a[0] == "trust")
    {
        ARGSZ(2)
        auto &pos = sp->getPos();
        int dim = sp->getDimensionId();
        auto lp = getFastLand(pos.x, pos.z, dim);
        if (lp && (lp->chkOwner(nm) == 2 || op))
        {
            DataLand dl;
            Fland2Dland(lp, dl);
            dl.addOwner(a[1]);
            updLand(dl);
            sb.write("§bMake ");
            sb.write(a[1]);
            sb.write(" a trusted player");
            outp.success(sb.getstr());
        }
        else
        {
            outp.error("No land here or not your land");
            return;
        }
    }
    if (a[0] == "untrust")
    {
        ARGSZ(2)
        auto &pos = sp->getPos();
        int dim = sp->getDimensionId();
        auto lp = getFastLand(pos.x, pos.z, dim);
        if (lp && (lp->chkOwner(nm) == 2 || op))
        {
            DataLand dl;
            Fland2Dland(lp, dl);
            dl.delOwner(a[1]);
            updLand(dl);
            sb.write("§bMake ");sb.write(a[1]);sb.write(" a distrust player");
            outp.success(sb.getstr());
        }
        else
        {
            outp.error("No land here or not your land");
            return;
        }
    }
    if (a[0] == "perm")
    {
        ARGSZ(2)
        auto &pos = sp->getPos();
        int dim = sp->getDimensionId();
        auto lp = getFastLand(pos.x, pos.z, dim);
        if (lp && (lp->chkOwner(nm) == 2 || op))
        {
            DataLand dl;
            Fland2Dland(lp, dl);
            dl.perm = (LandPerm)atoi(a[1]);
            updLand(dl);
            outp.success("§bChange permissions successfully");
        }
        else
        {
            outp.error("No land here or not your land");
            return;
        }
    }
    if (a[0] == "give")
    {
        ARGSZ(2)
        auto &pos = sp->getPos();
        int dim = sp->getDimensionId();
        auto lp = getFastLand(pos.x, pos.z, dim);
        if (lp && (lp->chkOwner(nm) == 2 || op))
        {
            DataLand dl;
            Fland2Dland(lp, dl);
            dl.addOwner(a[1], true);
            dl.delOwner(nm);
            updLand(dl);
            sb.write("§bSuccessfully give your territory to ");sb.write(a[1]);
            outp.success(sb.getstr());
        }
        else
        {
            outp.error("No land here or not your land");
            return;
        }
    }
    if (a[0] == "forceperm")
    {
        ARGSZ(2)
        if (!op)
            return;
        auto newperm = (LandPerm)atoi(a[1]);
        iterLands([newperm](DataLand &dl) {
            dl.perm = newperm;
        });
        outp.success("okay");
    }
    if (a[0] == "dumpall")
    {
        if (!op)
            return;
        iterLands_const([&](const DataLand &dl) {
            char buf[1000];
            snprintf(buf, 1000, "owner %s pos %d %d -> %d %d dim %d perm %d", dl.owner.c_str(), dl.x^0x80000000, dl.z^0x80000000, dl.dx^0x80000000, dl.dz^0x80000000, dl.dim, dl.perm);
            outp.addMessage(buf);
        });
        outp.success("okay");
    }
    if (a[0] == "trustgui")
    {
        string name = b.getName();
        if (sp)
            gui_ChoosePlayer(sp, "Choose players to trust", "Trust", [](ServerPlayer* xx,string_view dest) {
                    SPBuf sb;
                    sb.write("land trust \"");
                    sb.write(dest);
                    sb.write("\"");
                    runcmdAs(sb.get(), xx);
            });
        else
        {
            outp.error("Error");
        }
    }
}

static void CONVERT(char *b, int s)
{
    DataStream ds;
    ds.dat = string(b, s);
    int cnt;
    ds >> cnt;
    printf("%d lands found\n", cnt);
    for (int i = 0; i < cnt; ++i)
    {
        DataStream ds2;
        ds >> ds2.dat;
        int x, y, dx, dy, dim;
        string owner;
        ds2 >> dim >> x >> y >> dx >> dy >> owner;
        dx = x + dx - 1;
        dy = y + dy - 1;
        dim >>= 4;
        printf("land %d %d %d %d %d %s\n", x, y, dx, dy, dim, owner.c_str());
        if (x < -200000 || y < -200000 || dx < -200000 || dy < -200000)
        {
            printf("refuse to add land %s\n", owner.c_str());
            continue;
        }
        addLand((x)^0x80000000, (dx)^0x80000000, (y)^0x80000000, (dy)^0x80000000, dim, owner);
    }
}
static void load()
{
    char *buf;
    int sz;
    struct stat tmp;
    if (stat("data/land/land.db", &tmp) != -1)
    {
        file2mem("data/land/land.db", &buf, sz);
        printf("CONVERTING LANDS\n");
        CONVERT(buf, sz);
        link("data/land/land.db", "data/land/land.db.old");
        unlink("data/land/land.db");
        free(buf);
    }
}

static void NoticePerm(FastLand* fl,ServerPlayer* sp){
    SPBuf sb;
    sb.write("§cThis is "sv);
    sb.write(fl->getOwner());
    sb.write("'s land"sv);
    sendText(sp, sb.get(), POPUP);
}
static bool handle_dest(GameMode *a0, BlockPos const *a1)
{
    ServerPlayer *sp = a0->getPlayer();
    if (isOp(sp))
        return 1;
    int x(a1->x), z(a1->z), dim(sp->getDimensionId());
    FastLand *fl = getFastLand(x, z, dim);
    if (!fl || fl->hasPerm(sp->getName(), PERM_BUILD))
    {
        return 1;
    }
    else
    {
        NoticePerm(fl,sp);
        return 0;
    }
}
static bool handle_attack(Actor &vi, ActorDamageSource const &src, int &val)
{
    if (src.isChildEntitySource() || src.isEntitySource())
    {
        auto id = src.getEntityUniqueID();
        auto ent = getSrvLevel()->fetchEntity(id, false);
        ServerPlayer *sp = getSP(ent);
        if (!sp || isOp(sp))
            return 1;
        auto &pos = vi.getPos();
        int x(pos.x), z(pos.z), dim(vi.getDimensionId());
        FastLand *fl = getFastLand(x, z, dim);
        if (!fl || fl->hasPerm(sp->getName(), PERM_ATK))
        {
            return 1;
        }
        else
        {
            NoticePerm(fl,sp);
            return 0;
        }
    }
    return 1;
}
static bool handle_inter(GameMode *a0, Actor &a1)
{
    ServerPlayer *sp = a0->getPlayer();
    if (isOp(sp))
        return 1;
    auto &pos = a1.getPos();
    int x(pos.x), z(pos.z), dim(a1.getDimensionId());
    FastLand *fl = getFastLand(x, z, dim);
    if (!fl || fl->hasPerm(sp->getName(), PERM_INTERWITHACTOR))
    {
        return 1;
    }
    else
    {
        NoticePerm(fl,sp);
        return 0;
    }
}
static bool handle_useion(GameMode *a0, ItemStack *a1, BlockPos const *a2, BlockPos const *dstPos, Block const *a5)
{
    ServerPlayer *sp = a0->getPlayer();
    auto hash = sp->getNameTagAsHash();//do_hash(sp->getName());
    if (choose_state[hash] != 0)
    {
        if (choose_state[hash] == 1)
        {
            startpos[hash] = {a2->x, a2->z};
            sendText(sp, "§bPoint A selected");
        }
        if (choose_state[hash] == 2)
        {
            endpos[hash] = {a2->x, a2->z};
            char buf[1000];
            auto siz = (abs(startpos[hash].x - endpos[hash].x) + 1) * (abs(startpos[hash].z - endpos[hash].z) + 1);
            //sprintf(buf,"§bPoint B selected,size: %d price: %d",siz,siz*LAND_PRICE);
            snprintf(buf, 1000, "§bPoint B selected,size: %d price: %d", siz, siz * LAND_PRICE);
            sendText(sp, buf);
        }
        return 0;
    }
    if (isOp(sp))
        return 1;
    int x(a2->x), z(a2->z), dim(sp->getDimensionId());
    LandPerm pm;
    if (a1->isNull())
    {
        pm = PERM_USE;
    }
    else
    {
        pm = PERM_BUILD;
    }
    FastLand *fl = getFastLand(x, z, dim), *fl2 = getFastLand(dstPos->x, dstPos->z, dim);
    auto& name = sp->getName();
    if(fl && !fl->hasPerm(name,pm)){
        NoticePerm(fl,sp);
        return 0;
    }
    if(fl2 && !fl2->hasPerm(name,pm)){
        NoticePerm(fl2,sp);
        return 0;
    }
    return 1;
}
static bool handle_popitem(ServerPlayer &sp, BlockPos &bpos)
{
    if (isOp(&sp))
        return 1;
    int x(bpos.x), z(bpos.z), dim(sp.getDimensionId());
    FastLand *fl = getFastLand(x, z, dim);
    if (!fl || fl->hasPerm(sp.getName(), PERM_POPITEM))
    {
        return 1;
    }
    else
    {
        NoticePerm(fl,&sp);
        return 0;
    }
}
/*
static unordered_map<STRING_HASH, string> lastland;
THook(void *, _ZN12ServerPlayer9tickWorldERK4Tick, ServerPlayer *sp, unsigned long const *tk)
{
    if (!land_tip)
        return original(sp, tk);
    if (*tk%16==0)
    {
        auto hash = do_hash(sp->getName());
        auto &oldname = lastland[hash];
        auto &pos = sp->getPos();
        int dim = sp->getDimensionId();
        FastLand *fl = getFastLand(pos.x, pos.z, dim);
        string_view newname;
        if (fl)
        {
            newname = string_view(fl->owner,fl->owner_sz);
        }
        if (oldname != newname)
        {
            SPBuf sb;
            if(newname==""){
                sb.write("You lefted "sv);sb.write(string_view(oldname));sb.write("'s land!"sv);
            }else{
                sb.write("You entered "sv);sb.write(newname);sb.write("'s land!"sv);
            }
            sendText(sp, sb.get(), TextType::TIP);
            oldname = newname;
        }
    }
    return original(sp, tk);
}*/
void mod_init(std::list<string> &modlist)
{
    printf("[LAND] loaded! " BDL_TAG "\n");
    load();
    loadcfg();
    string dummy;
    if(!db.Get("land_fixed_v3",dummy)){
        CHECK_AND_FIX_ALL();
        db.Put("land_fixed_v3",dummy);
    }
    register_cmd("land", oncmd, "land command");
    register_cmd("reload_land", loadcfg, "reload land cfg", 1);
    reg_destroy(handle_dest);
    reg_useitemon(handle_useion);
    reg_actorhurt(handle_attack);
    reg_popItem(handle_popitem);
    reg_interact(handle_inter);
    load_helper(modlist);
}
