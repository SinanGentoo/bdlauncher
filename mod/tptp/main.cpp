#include<cstdio>
#include<list>
#include<forward_list>
#include<string>
#include<unordered_map>
#include"../cmdhelper.h"
#include<vector>
#include<Loader.h>
#include<MC.h>
#include"seral.hpp"
#include"../serial/seral.hpp"
#include <sys/stat.h>
#include<unistd.h>
#include <sys/stat.h>
#include"../base/base.h"
#include<cmath>
#include<cstdlib>
#include<ctime>
#include"../gui/gui.h"

using std::string;
using std::unordered_map;
using std::forward_list;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
//#define dbg_printf(...) {}
#define dbg_printf printf
extern "C" {
   BDL_EXPORT void mod_init(std::list<string>& modlist);
}

extern void load_helper(std::list<string>& modlist);
bool CanBack=true,CanHome=true,CanTP=true;
int MaxHomes=5;
struct Vpos {
    int x,y,z,dim;
    string name;
    Vpos() {}
    Vpos(int a,int b,int c,int d,string e) {
        x=a,y=b,z=c,dim=d,name=e;
    }
    void packto(DataStream & ds) const{
        ds<<x<<y<<z<<dim<<name;
    }
    void unpack(DataStream& ds){
        ds>>x>>y>>z>>dim>>name;
    }
    void tele(Actor& ply) const {
        TeleportA(ply, {x,y,z}, {dim});
    }
};
struct home {
    int cnt=0;
    Vpos vals[10];
    void packto(DataStream& ds) const{
        ds<<cnt;
        for(int i=0;i<cnt;++i){
            ds<<vals[i];
        }
    }
    void unpack(DataStream& ds){
        ds>>cnt;
        for(int i=0;i<cnt;++i){
            ds>>vals[i];
        }
    }
};
static list<string> warp_list;
static unordered_map<string,Vpos> warp;
LDBImpl tp_db("data_v2/tp");
void CONVERT_WARP(){
    char* buf;
    int siz;
    file2mem("data/tp/wps.db",&buf,siz);
    DataStream ds;
    ds.dat=string(buf,siz);
    int cnt;
    ds>>cnt;
    vector<string> warps;
    for(int i=0;i<cnt;++i){
        int vposlen;
        ds>>vposlen;
        Vpos pos;
        ds>>pos;
        DataStream tmpds;
        tmpds<<pos;
        tp_db.Put("warp_"+pos.name,tmpds.dat);
        printf("warp %s found\n",pos.name.c_str());
        warps.emplace_back(pos.name);
    }
    DataStream tmpds;
    tmpds<<warps;
    tp_db.Put("warps",tmpds.dat);
    free(buf);
}
void CONVERT_HOME(){
    char* buf;
    int siz;
    file2mem("data/tp/tp.db",&buf,siz);
    int cnt;
    DataStream ds;
    ds.dat=string(buf,siz);
    ds>>cnt;
    printf("sz %d\n",cnt);
    for(int i=0;i<cnt;++i){
        string key;
        home hom;
        int homelen;
        ds>>key;
        ds>>homelen;
        printf("key %s len %d\n",key.c_str(),homelen);
        int homecnt;
        ds>>homecnt;
        hom.cnt=homecnt;
        for(int j=0;j<homecnt;++j){
            ds.curpos+=4; //skip sizeof(vpos)
            ds>>hom.vals[j];
        }
        DataStream tmpds;
        tmpds<<hom;
        tp_db.Put("home_"+key,tmpds.dat);
    }
    free(buf);
}
void add_warp(int x,int y,int z,int dim,const string& name){
    warp_list.push_back(name);
    Vpos ps;
    ps.x=x,ps.y=y,ps.z=z,ps.dim=dim,ps.name=name;
    warp[name]=ps;
    DataStream ds;
    ds<<ps;
    tp_db.Put("warp_"+name,ds.dat);
    ds.reset();
    ds<<warp_list;
    tp_db.Put("warps",ds.dat);
}
void del_warp(const string& name){
    warp_list.remove(name);
    DataStream ds;
    ds<<warp_list;
    tp_db.Put("warps",ds.dat);
    tp_db.Del("warp_"+name);
}
void load_warps_new(){
    DataStream ds;
    if(!tp_db.Get("warps",ds.dat)) return; //fix crash
    ds>>warp_list;
    printf("%d warps found\n",warp_list.size());
    for(auto& i:warp_list){
        DataStream tmpds;
        tp_db.Get("warp_"+i,tmpds.dat);
        tmpds>>warp[i];
    }
}
unordered_map<string,home> home_cache;
home& getHome(const string& key){
    auto it=home_cache.find(key);
    if(it!=home_cache.end()){
        return it->second;
    }
    if(home_cache.size()>256){
        home_cache.clear();
    }
    DataStream ds;
    home hm;
    if(tp_db.Get("home_"+key,ds.dat)){
        ds>>hm;
    }
    home_cache[key]=hm;
    return home_cache[key];
}
void putHome(const string& key,home& hm){
    DataStream ds;
    ds<<hm;
    tp_db.Put("home_"+key,ds.dat);
}
static void load() {
    struct stat tmp;
    if(stat("data/tp/tp.db",&tmp)!=-1) {
        printf("[TP] CONVERTING DATA.\n");
        CONVERT_HOME();
        CONVERT_WARP();
        link("data/tp/tp.db","data/tp/tp.db_old");
        unlink("data/tp/tp.db");
    }
}
struct tpreq {
    int dir; //0=f
    string name;
};
static unordered_map<string,tpreq> tpmap;
static void oncmd_suic(std::vector<string_view>& a,CommandOrigin const & b,CommandOutput &outp) {
    KillActor(b.getEntity());
    outp.success("You are dead");
}
void sendTPForm(const string& from,int type,ServerPlayer* sp){
    string cont=from+(type?" wants to teleport you to his location":"want to teleport to your location")+"\n";
    string name=sp->getName();
    auto lis=new list<pair<string,std::function<void()> > >();
    lis->emplace_back(
        "Accept",[name]{
            auto x=getSrvLevel()->getPlayer(name);
            if(x)
                runcmdAs("tpa ac",x);
        }
    );
    lis->emplace_back(
        "Refuse",[name]{
            auto x=getSrvLevel()->getPlayer(name);
            if(x)
                runcmdAs("tpa de",x);
        }
    );
    gui_Buttons(sp,cont,"Teleport Request",lis);
}
void sendTPChoose(ServerPlayer* sp,const char* type){
    string name=sp->getName();
    gui_ChoosePlayer(sp,"Select target player","Send teleport request",[name,type](string_view dest){
        auto xx=getSrvLevel()->getPlayer(name);
            if(xx){
                SPBuf sb;
                sb.write("tpa %s \"",type);
                sb.write(dest);
                sb.write("\"");
                runcmdAs(sb.get(),xx);
                //runcmdAs("tpa "+type+" "+SafeStr(dest),xx);
            }
    });
}
static void oncmd(std::vector<string_view>& a,CommandOrigin const & b,CommandOutput &outp) {
    if(!CanTP){
        outp.error("Teleport not enabled on this server!");
        return;
    }
    int pl=(int)b.getPermissionsLevel();
    string name=b.getName();
    string dnm=a.size()==2?string(a[1]):"";
    Player* dst=NULL;
    if(dnm!="")
        dst=getplayer_byname2(dnm);
    if(dst) dnm=dst->getName();
    ARGSZ(1)
    if(a[0]=="f") {
        //from
        if(dst==NULL) {
            outp.error("Player is offline");
            return;
        }
        if(tpmap.count(dnm)) {
            outp.error("You have already initiated the request");
            return;
        }
        tpmap[dnm]= {0,name};
        outp.success("§bYou sent a teleport request to the target player");
        sendText(dst,"§b"+name+" a wants you to teleport to his location,you can enter \"/tpa ac\" to accept or \"/tpa de\" to reject");
        sendTPForm(name,1,(ServerPlayer*)dst);
    }
    if(a[0]=="t") {
        //to
        if(dst==NULL) {
            outp.error("Player is offline");
            return;
        }
        if(tpmap.count(dnm)) {
            outp.error("You have already initiated the request");
            return;
        }
        tpmap[dnm]= {1,name};
        outp.success("§bYou sent a teleport request to the target player");
        sendText(dst,"§b"+name+" a wants to teleport to your location,you can enter \"/tpa ac\" to accept or \"/tpa de\" to reject");
        sendTPForm(name,0,(ServerPlayer*)dst);
    }
    if(a[0]=="gui"){
        auto lis=new list<pair<string,std::function<void()> > >();
        lis->emplace_back(
            "Teleport to a player",[name]{
                auto x=getSrvLevel()->getPlayer(name);
                if(x)
                {
                    sendTPChoose((ServerPlayer*)x,"t");
                }
            }
        );
        lis->emplace_back(
            "Teleport a player to you",[name]{
                auto x=getSrvLevel()->getPlayer(name);
                if(x)
                {
                    sendTPChoose((ServerPlayer*)x,"f");
                }
            }
        );
        gui_Buttons((ServerPlayer*)b.getEntity(),"Send teleport request","Send teleport request",lis);
    }
    if(a[0]=="ac") {
        //accept
        if(tpmap.count(name)==0) return;
        tpreq req=tpmap[name];
        tpmap.erase(name);
        outp.success("§bYou have accepted the send request from the other party");
        dst=getplayer_byname(req.name);
        if(dst) {
            sendText(dst,"§b "+name+" accepted the transmission request");
            if(req.dir==0) {
                //f
                TeleportA(*getplayer_byname(name),dst->getPos(), {dst->getDimensionId()});
            } else {
                TeleportA(*dst,b.getWorldPosition(), {b.getEntity()->getDimensionId()});
            }
        }
    }
    if(a[0]=="de") {
        //deny
        if(tpmap.count(name)==0) return;
        tpreq req=tpmap[name];
        tpmap.erase(name);
        outp.success("§bYou have rejected the send request");
        dst=getplayer_byname(req.name);
        if(dst)
            sendText(dst,"§b "+name+" rejected the transmission request");
    }
}

static void oncmd_home(std::vector<string_view>& a,CommandOrigin const & b,CommandOutput &outp) {
    if(!CanHome){
        outp.error("Home not enabled on this server!");
        return;
    }
    int pl=(int)b.getPermissionsLevel();
    string name=b.getName();
    string homen=a.size()==2?string(a[1]):"hape";
    Vec3 pos=b.getWorldPosition();
    //printf("%f %f %f\n",pos.x,pos.y,pos.z);
    ARGSZ(1)
    if(a[0]=="add") {
        home& myh=getHome(name);
        if(myh.cnt>=MaxHomes) {
            outp.error("Can't add more homes");
            return;
        }
        myh.vals[myh.cnt]=Vpos(pos.x,pos.y,pos.z,b.getEntity()->getDimensionId(),homen);
        myh.cnt++;
        putHome(name,myh);
        outp.success("§bSuccessfully added a home");
    }
    if(a[0]=="del") {
        home& myh=getHome(name);
        home tmpn;
        tmpn=myh;
        tmpn.cnt=0;
        for(int i=0; i<myh.cnt; ++i) {
            if(myh.vals[i].name!=homen) {
                tmpn.vals[tmpn.cnt++]=myh.vals[i];
            }
        }
        myh=tmpn;
        putHome(name,myh);
        outp.success("§bHome has been deleted");
    }
    if(a[0]=="go") {
        home& myh=getHome(name);
        for(int i=0; i<myh.cnt; ++i) {
            if(myh.vals[i].name==homen) {
                myh.vals[i].tele(*b.getEntity());
                outp.success("§bTeleported you to home");
            }
        }
    }
    if(a[0]=="ls") {
        home& myh=getHome(name);
        sendText((Player*)b.getEntity(),"§b====Home list====");
        for(int i=0; i<myh.cnt; ++i) {
            sendText((Player*)b.getEntity(),myh.vals[i].name);
        }
        outp.success("§b============");
    }
    if(a[0]=="gui"){
        home& myh=getHome(name);
        auto lis=new list<pair<string,std::function<void()> > >();
        for(int i=0; i<myh.cnt; ++i) {
            string warpname=myh.vals[i].name;
            lis->emplace_back(
                warpname,
                [warpname,name]()->void{
                    auto x=getSrvLevel()->getPlayer(name);
                    if(x)
                        runcmdAs("home go "+SafeStr(warpname),x);
                }
            );
        }
        gui_Buttons((ServerPlayer*)b.getEntity(),"Please choose a home","Home",lis);
    }
}
static void oncmd_warp(std::vector<string_view>& a,CommandOrigin const & b,CommandOutput &outp) {
    int pl=(int)b.getPermissionsLevel();
    // printf("pl %d\n",pl);
    string name=b.getName();
    Vec3 pos=b.getWorldPosition();
    ARGSZ(1)
    if(a[0]=="add") {
        if(pl<1) return;
        ARGSZ(2)
        add_warp(pos.x,pos.y,pos.z,b.getEntity()->getDimensionId(),string(a[1]));
        outp.success("§bSuccessfully added a warp");
        return;
    }
    if(a[0]=="del") {
        if(pl<1) return;
        ARGSZ(2)
        del_warp(string(a[1]));
        outp.success("§bSuccessfully deleted a Warp");
        return;
    }
    if(a[0]=="ls") {
        outp.addMessage("§b====Warp list====");
        for(auto const& i:warp_list) {
            outp.addMessage(i);
        }
        outp.success("§b===========");
        return;
    }
    if(a[0]=="gui"){
        auto lis=new list<pair<string,std::function<void()> > >();
        string nam=b.getName();
        for(auto const& i:warp_list) {
            string warpname=i;
            lis->emplace_back(
                warpname,
                [warpname,nam]()->void{
                    auto x=getSrvLevel()->getPlayer(nam);
                    if(x)
                        runcmdAs("warp "+SafeStr(warpname),x);
                }
            );
        }
        gui_Buttons((ServerPlayer*)b.getEntity(),"Please choose a warp","Warp",lis);
    }
    //go
    auto it=warp.find(string(a[0]));
        if(it!=warp.end()) {
            it->second.tele(*b.getEntity());
            outp.success("§bTeleported you to warp");
            return;
        }
}

static unordered_map<Shash_t,pair<Vec3,int> > deathpoint;
static void oncmd_back(std::vector<string_view>& a,CommandOrigin const & b,CommandOutput &outp) {
    if(!CanBack) {outp.error("Back not enabled on this server"); return;}
    ServerPlayer* sp=(ServerPlayer*)b.getEntity();
    if(!sp) return;
    auto it=deathpoint.find(sp->getNameTagAsHash());
    if(it==deathpoint.end()){
        outp.error("Can't find deathpoint");
        return;
    }
    TeleportA(*sp,it->second.first,{it->second.second});
    deathpoint.erase(it);
    outp.success("§bBack to deathpoint");
}
static void handle_mobdie(Mob& mb,const ActorDamageSource&){
    if(!CanBack) return;
    auto sp=getSP(mb);
    if(sp){
        ServerPlayer* sp=(ServerPlayer*)&mb;
        sendText(sp,"§bYou can use /back to return last deathpoint");
        deathpoint[sp->getNameTagAsHash()]={sp->getPos(),sp->getDimensionId()};
    }
}
#include"rapidjson/document.h"
using namespace rapidjson;
static void load_cfg(){
    Document d;
    char* buf;
    int sz;
    file2mem("config/tp.json",&buf,sz);
    if(d.ParseInsitu(buf).HasParseError()){
        printf("[TP] JSON ERROR!\n");
        exit(1);
    }
    CanBack=d["can_back"].GetBool();
    CanHome=d["can_home"].GetBool();
    CanTP=d["can_tp"].GetBool();
    MaxHomes=d["max_homes"].GetInt();
    free(buf);
}

void mod_init(std::list<string>& modlist) {
    printf("[TPs] loaded! " BDL_TAG "\n");
    load();
    load_warps_new();
    load_cfg();
    register_cmd("suicide",oncmd_suic,"kill yourself");
    register_cmd("tpa",oncmd,"teleport command");
    register_cmd("home",oncmd_home,"home command");
    register_cmd("warp",oncmd_warp,"warp command");
    register_cmd("back",oncmd_back,"back to deathpoint");
    reg_mobdie(handle_mobdie);
    load_helper(modlist);
}
