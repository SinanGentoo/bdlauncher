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
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
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
static std::unordered_map<string, LPOS> startpos, endpos;
static unordered_map<string, int> choose_state;

int LAND_PRICE, LAND_PRICE2;
using namespace rapidjson;
void loadcfg()
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
    free(buf);
}
static void oncmd(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    auto nm=b.getName();
    bool op=isOp(b);
    ARGSZ(1)
    if(a[0]=="exit"){
        choose_state.erase(nm);
        outp.success("§bExit selection mode, please input /land buy");
    }
    if(a[0]=="a"){
        choose_state[nm]=1;
        outp.success("§bEnter selection mode, please click on the ground to select point A");
    }
    if(a[0]=="b"){
        choose_state[nm]=2;
        outp.success("§bPlease click on the ground to select point B");
    }
    if(a[0]=="query"){
        auto& pos=b.getEntity()->getPos();
        int dim=b.getEntity()->getDimensionId();
        auto lp=getFastLand(pos.x,pos.z,dim);
        if(lp){
            char buf[1000];
            //sprintf(buf,"§bThis is %s's land",lp->owner);
            snprintf(buf,1000,"§bThis is %s's land",lp->owner);
            outp.success(buf);
        }else{
            outp.error("No land here");
            return;
        }
    }
    if(a[0]=="buy"){
        int x,z,dx,dz;
        int dim=b.getEntity()->getDimensionId();
        if(startpos.count(nm)+endpos.count(nm)!=2){
            outp.error("Choose 2 points please.");
            return;
        }
	    choose_state.erase(nm);
        x=min(startpos[nm].x,endpos[nm].x);
        z=min(startpos[nm].z,endpos[nm].z);
        dx=max(startpos[nm].x,endpos[nm].x);
        dz=max(startpos[nm].z,endpos[nm].z);
        //step -1 :sanitize pos
        if(x < -200000 || z< -200000){
            outp.error("Pos too small !> -200000 needed");
            return;
        }
        //step 1 check money
        int deltax=dx-x+1,deltaz=dz-z+1;
        uint siz=deltax*deltaz;
        if(deltax>=4096 || deltaz>=4096 || siz>=5000000){
            outp.error("Too big land");
            return;
        }
        int price=siz*LAND_PRICE;
        if(price<=0 || price>=500000000){
            outp.error("Too big land");
            return;
        }
        auto mon=get_money(nm);
        if(mon<price){
            outp.error("Money not enough");
            return;
        }
        //step 2 check coll
        for(int i=x;i<=dx;++i)
        for(int j=z;j<=dz;++j)
        {
            auto lp=getFastLand(i,j,dim);
            if(lp){
                outp.error("Land collision detected!");
                return;
            }
        }
        //step 3 add land
        set_money(nm,mon-price);
        //printf("last step\n");
        addLand(to_lpos(x),to_lpos(dx),to_lpos(z),to_lpos(dz),dim,nm);
        outp.success("§bSuccessful land purchase");
    }
    if(a[0]=="sell"){
        auto& pos=b.getEntity()->getPos();
        int dim=b.getEntity()->getDimensionId();
        auto lp=getFastLand(pos.x,pos.z,dim);
        if(lp && (lp->chkOwner(nm)==2||op)){
            int siz=(lp->dx-lp->x)*(lp->dz-lp->z);
            add_money(nm,siz*LAND_PRICE2);
            removeLand(lp);
            outp.success("§bYour land has been sold");
        }else{
            outp.error("No land here or not your land");
            return;
        }
    }
    if(a[0]=="trust"){
        ARGSZ(2)
        auto& pos=b.getEntity()->getPos();
        int dim=b.getEntity()->getDimensionId();
        auto lp=getFastLand(pos.x,pos.z,dim);
        if(lp && (lp->chkOwner(nm)==2||op)){
            DataLand dl;
            Fland2Dland(lp,dl);
            dl.addOwner(a[1]);
            updLand(dl);
            outp.success("§bMake "+a[1]+" a trusted player");
        }else{
            outp.error("No land here or not your land");
            return;
        }
    }
    if(a[0]=="untrust"){
        ARGSZ(2)
        auto& pos=b.getEntity()->getPos();
        int dim=b.getEntity()->getDimensionId();
        auto lp=getFastLand(pos.x,pos.z,dim);
        if(lp && (lp->chkOwner(nm)==2||op)){
            DataLand dl;
            Fland2Dland(lp,dl);
            dl.delOwner(a[1]);
            updLand(dl);
            outp.success("§bMake "+a[1]+" a distrust player");
        }else{
            outp.error("No land here or not your land");
            return;
        }
    }
    if(a[0]=="perm"){
        ARGSZ(2)
        auto& pos=b.getEntity()->getPos();
        int dim=b.getEntity()->getDimensionId();
        auto lp=getFastLand(pos.x,pos.z,dim);
        if(lp && (lp->chkOwner(nm)==2 || op)){
            DataLand dl;
            Fland2Dland(lp,dl);
            dl.perm=(LandPerm)atoi(a[1].c_str());
            updLand(dl);
            outp.success("§bChange permissions successfully");
        }else{
            outp.error("No land here or not your land");
            return;
        }
    }
    if(a[0]=="give"){
        ARGSZ(2)
        auto& pos=b.getEntity()->getPos();
        int dim=b.getEntity()->getDimensionId();
        auto lp=getFastLand(pos.x,pos.z,dim);
        if(lp && (lp->chkOwner(nm)==2||op)){
            DataLand dl;
            Fland2Dland(lp,dl);
            dl.addOwner(a[1],true);
            dl.delOwner(nm);
            updLand(dl);
            outp.success("§bSuccessfully give your territory to "+a[1]);
        }else{
            outp.error("No land here or not your land");
            return;
        }
    }
    if (a[0] == "trustgui")
    {
        string name = b.getName();
        auto sp = getSP(b.getEntity());
        if (sp)
            gui_ChoosePlayer(sp, "Choose players to trust", "Trust", [name](const string &dest) {
                auto xx = getplayer_byname(name);
                if (xx)
                    runcmdAs("land trust " + SafeStr(dest), xx);
            });
        else
        {
            outp.error("Error");
        }
    }
}

void CONVERT(char *b, int s)
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
        dx=x+dx-1;dy=y+dy-1;
        dim >>= 4;
        printf("land %d %d %d %d %d %s\n",x,y,dx,dy,dim,owner.c_str());
        if(x< -200000 || y< -200000 || dx< -200000 || dy< -200000){
            printf("refuse to add land %s\n",owner.c_str());
            continue;
        }
        addLand(to_lpos(x), to_lpos(dx), to_lpos(y), to_lpos(dy), dim, owner);
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

static int handle_dest(GameMode *a0, BlockPos const *a1)
{
    ServerPlayer *sp = a0->getPlayer();
    if (isOp(sp))
        return 1;
    int x(a1->x), z(a1->z), dim(sp->getDimensionId());
    string name = sp->getName();
    if (likely(generic_perm(x, z, dim, PERM_BUILD, name)))
    {
        return 1;
    }
    else
    {
        char buf[1000];
        FastLand *fl = getFastLand(x, z, dim);
        //sprintf(buf, "§cThis is %s's land", fl->owner);
        snprintf(buf,1000,"§cThis is %s's land",fl->owner);
        sendText(sp, buf, POPUP);
        return 0;
    }
}
static bool handle_attack(Actor &vi, ActorDamageSource const &src, int &val)
{
    int x, y;
    if (src.isChildEntitySource() || src.isEntitySource())
    {
        auto id = src.getEntityUniqueID();
        auto ent = getSrvLevel()->fetchEntity(id, false);
        ServerPlayer *sp = getSP(ent);
        if (!sp || isOp(sp))
            return 1;
        auto &pos = vi.getPos();
        int x(pos.x),z(pos.z),dim(vi.getDimensionId());
        auto name=sp->getName();
        if (likely(generic_perm(x, z, dim, PERM_ATK, name)))
        {
            return 1;
        }
        else
        {
            char buf[1000];
            FastLand *fl = getFastLand(x, z, dim);
            //sprintf(buf, "§cThis is %s's land", fl->owner);
            snprintf(buf,1000,"§cThis is %s's land",fl->owner);
            sendText(sp, buf, POPUP);
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
    auto& pos = a1.getPos();
    int x(pos.x), z(pos.z), dim(a1.getDimensionId());
    string name = sp->getName();
    if (likely(generic_perm(x, z, dim, PERM_INTERWITHACTOR, name)))
    {
        return 1;
    }
    else
    {
        char buf[1000];
        FastLand *fl = getFastLand(x, z, dim);
        //sprintf(buf, "§cThis is %s's land", fl->owner);
        snprintf(buf,1000,"§cThis is %s's land", fl->owner);
        sendText(sp, buf, POPUP);
        return 0;
    }
}
static bool handle_useion(GameMode *a0, ItemStack *a1, BlockPos const *a2, BlockPos const *dstPos, Block const *a5)
{
    ServerPlayer* sp=a0->getPlayer();
    auto name=sp->getName();
    if (choose_state[name] != 0)
    {
        if (choose_state[name] == 1)
        {
            startpos[name] = {a2->x,a2->z};
            sendText(sp, "§bPoint A selected");
        }
        if (choose_state[name] == 2)
        {
            endpos[name] = {a2->x, a2->z};
            char buf[1000];
            auto siz=abs(startpos[name].x-endpos[name].x+1)*abs(startpos[name].z-endpos[name].z+1);
            //sprintf(buf,"§bPoint B selected,size: %d price: %d",siz,siz*LAND_PRICE);
            snprintf(buf,1000,"§bPoint B selected,size: %d price: %d",siz,siz*LAND_PRICE);
            sendText(sp, buf);
        }
        return 0;
    }
    if(isOp(sp)) return 1;
    int x(a2->x),z(a2->z),dim(sp->getDimensionId());
    LandPerm pm;
    if(a1->isNull()){
        pm=PERM_USE;
    }else{
        pm=PERM_BUILD;
    }
    if(generic_perm(x,z,dim,pm,name) && generic_perm(dstPos->x,dstPos->z,dim,pm,name)){
        return 1;
    }else{
        char buf[1000];
        FastLand *fl = getFastLand(x, z, dim);
        //sprintf(buf, "§cThis is %s's land", fl->owner);
        snprintf(buf,1000,"§cThis is %s's land",fl->owner);
        sendText(sp, buf, POPUP);
        return 0;
    }
}
static bool handle_popitem(ServerPlayer &sp, BlockPos &bpos)
{
    if (isOp(&sp))
        return 1;
    int x(bpos.x), z(bpos.z), dim(sp.getDimensionId());
    string name = sp.getName();
    if (likely(generic_perm(x, z, dim, PERM_POPITEM, name)))
    {
        return 1;
    }
    else
    {
        char buf[1000];
        FastLand *fl = getFastLand(x, z, dim);
        //sprintf(buf, "§bThis is %s's land", fl->owner);
        snprintf(buf,1000,"§bThis is %s's land",fl->owner);
        sendText(&sp, buf, POPUP);
        return 0;
    }
}
void mod_init(std::list<string> &modlist)
{
    printf("[LAND] loaded! V2019-12-14\n");
    load();
    loadcfg();
    init_cache();
    register_cmd("land", (void *)oncmd, "land command");
    register_cmd("reload_land", fp(loadcfg), "reload land cfg", 1);
    reg_destroy(handle_dest);
    reg_useitemon(handle_useion);
    reg_actorhurt(handle_attack);
    reg_popItem(handle_popitem);
    reg_interact(handle_inter);
    load_helper(modlist);
}
