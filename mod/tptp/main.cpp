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
static void oncmd_suic(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    KillActor(b.getEntity());
    outp.success("§e你死了");
}
void sendTPForm(const string& from,int type,ServerPlayer* sp){
    string cont="§e"+from+(type?"想传送你到对方":"想传送到你")+"\n";
    string name=sp->getName();
    auto lis=new list<pair<string,std::function<void()> > >();
    lis->emplace_back(
        "同意",[name]{
            auto x=getSrvLevel()->getPlayer(name);
            if(x)
                runcmdAs("tpa ac",x);
        }
    );
    lis->emplace_back(
        "拒绝",[name]{
            auto x=getSrvLevel()->getPlayer(name);
            if(x)
                runcmdAs("tpa de",x);
        }
    );
    gui_Buttons(sp,cont,"TP 请求",lis);
}
void sendTPChoose(ServerPlayer* sp,const string& type){
    string name=sp->getName();
    gui_ChoosePlayer(sp,"§e选择目标玩家","TP Req",[name,type](const string& dest){
        auto xx=getSrvLevel()->getPlayer(name);
            if(xx)
            runcmdAs("tpa "+type+" "+SafeStr(dest),xx);
    });
}
static void oncmd(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    if(!CanTP){
        outp.error("tp not enabled on this server!");
        return;
    }
    int pl=(int)b.getPermissionsLevel();
    string name=b.getName();
    string dnm=a.size()==2?a[1]:"";
    Player* dst=NULL;
    if(dnm!="")
        dst=getplayer_byname2(dnm);
    if(dst) dnm=dst->getName();
    ARGSZ(1)
    if(a[0]=="f") {
        //from
        if(dst==NULL) {
            outp.error("§e[Teleport] 玩家不在线");
            return;
        }
        if(tpmap.count(dnm)) {
            outp.error("§e[Teleport] 请求中");
            return;
        }
        tpmap[dnm]= {0,name};
        outp.success("§e[Teleport] 你向对方发送了传送请求");
        sendText(dst,"§e[Teleport] "+name+" 想让你传送到他/她所在的位置\n§e输入“/tpa ac” 接受 或 “/tpa de” 拒绝");
        sendTPForm(name,1,(ServerPlayer*)dst);
    }
    if(a[0]=="t") {
        //to
        if(dst==NULL) {
            outp.error("§e[Teleport] 玩家不在线");
            return;
        }
        if(tpmap.count(dnm)) {
            outp.error("§e[Teleport] 请求中");
            return;
        }
        tpmap[dnm]= {1,name};
        outp.success("§e[Teleport] 你向对方发送了传送请求");
        sendText(dst,"§e[Teleport] "+name+" 想传送到你所在位置\n§e输入“/tpa ac” 接受 或 “/tpa de” 拒绝");
        sendTPForm(name,0,(ServerPlayer*)dst);
    }
    if(a[0]=="gui"){
        auto lis=new list<pair<string,std::function<void()> > >();
        lis->emplace_back(
            "传送到玩家",[name]{
                auto x=getSrvLevel()->getPlayer(name);
                if(x)
                {
                    sendTPChoose((ServerPlayer*)x,"t");
                }
            }
        );
        lis->emplace_back(
            "传送玩家到你",[name]{
                auto x=getSrvLevel()->getPlayer(name);
                if(x)
                {
                    sendTPChoose((ServerPlayer*)x,"f");
                }
            }
        );
        gui_Buttons((ServerPlayer*)b.getEntity(),"发送传送请求","发送传送请求",lis);
    }
    if(a[0]=="ac") {
        //accept
        if(tpmap.count(name)==0) return;
        tpreq req=tpmap[name];
        tpmap.erase(name);
        outp.success("§e[Teleport] 你已接受对方的传送请求");
        dst=getplayer_byname(req.name);
        if(dst) {
            sendText(dst,"§e[Teleport] "+name+" 接受了请求");
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
        outp.success("§e[Teleport] 你已拒绝对方的传送请求");
        dst=getplayer_byname(req.name);
        if(dst)
            sendText(dst,"§c[Teleport] "+name+" 已拒绝传送请求");
    }
    if(a[0]=="help") {
        outp.error("传送系统指令列表:\n/tpa gui gui传送\n/tpa f 玩家名 ——让玩家传送到你\n/tpa t 玩家名 ——传送你到玩家\n/tpa ac ——同意\n/tpa de ——拒绝");
    }
}

static void oncmd_home(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    if(!CanHome){
        outp.error("home not enabled on this server!");
        return;
    }
    int pl=(int)b.getPermissionsLevel();
    string name=b.getName();
    string homen=a.size()==2?a[1]:"hape";
    Vec3 pos=b.getWorldPosition();
    //printf("%f %f %f\n",pos.x,pos.y,pos.z);
    ARGSZ(1)
    if(a[0]=="add") {
        home& myh=getHome(name);
        if(myh.cnt>=MaxHomes) {
            outp.error("[Teleport] 无法再添加更多的家");
            return;
        }
        myh.vals[myh.cnt]=Vpos(pos.x,pos.y,pos.z,b.getEntity()->getDimensionId(),homen);
        myh.cnt++;
        putHome(name,myh);
        outp.success("§e[Teleport] 已添加家");
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
        outp.success("§e[Teleport] 已删除家");
    }
    if(a[0]=="go") {
        home& myh=getHome(name);
        for(int i=0; i<myh.cnt; ++i) {
            if(myh.vals[i].name==homen) {
                myh.vals[i].tele(*b.getEntity());
                outp.success("§e[Teleport] 已传送到家");
            }
        }
    }
    if(a[0]=="ls") {
        home& myh=getHome(name);
        sendText((Player*)b.getEntity(),"§e家列表:");
        for(int i=0; i<myh.cnt; ++i) {
            sendText((Player*)b.getEntity(),myh.vals[i].name);
        }
        outp.success("§e============");
    }
    if(a[0]=="gui"){
        home& myh=getHome(name);
        auto lis=new list<pair<string,std::function<void()> > >();
        for(int i=0; i<myh.cnt; ++i) {
            string warpname=myh.vals[i].name;
            lis->emplace_back(
                "到 "+warpname,
                [warpname,name]()->void{
                    auto x=getSrvLevel()->getPlayer(name);
                    if(x)
                        runcmdAs("home go "+SafeStr(warpname),x);
                }
            );
        }
        gui_Buttons((ServerPlayer*)b.getEntity(),"回家\n","家",lis);
    }
    if(a[0]=="help") {
        outp.error("家指令列表:\n/home gui gui回家\n/home add 名字 ——添加一个家\n/home ls ——查看你的所有家\n/home go 名字 ——回家");
    }
}
static void oncmd_warp(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    int pl=(int)b.getPermissionsLevel();
    // printf("pl %d\n",pl);
    string name=b.getName();
    Vec3 pos=b.getWorldPosition();
    ARGSZ(1)
    if(a[0]=="add") {
        if(pl<1) return;
        ARGSZ(2)
        add_warp(pos.x,pos.y,pos.z,b.getEntity()->getDimensionId(),a[1]);
        outp.success("§e§e[Teleport] 已添加Warp");
        return;
    }
    if(a[0]=="del") {
        if(pl<1) return;
        ARGSZ(2)
        del_warp(a[1]);
        outp.success("§e§e[Teleport] 已删除Warp");
        return;
    }
    if(a[0]=="ls") {
        outp.addMessage("§eWarp列表:");
        for(auto const& i:warp_list) {
            outp.addMessage(i);
        }
        outp.success("§e===========");
        return;
    }
    if(a[0]=="help") {
        outp.error("Warp指令列表:\n/warp ls ——查看地标列表\n/warp 地标名 ——前往一个地标");
    }
    if(a[0]=="gui"){
        auto lis=new list<pair<string,std::function<void()> > >();
        string nam=b.getName();
        for(auto const& i:warp_list) {
            string warpname=i;
            lis->emplace_back(
                "前往 "+warpname,
                [warpname,nam]()->void{
                    auto x=getSrvLevel()->getPlayer(nam);
                    if(x)
                        runcmdAs("warp "+SafeStr(warpname),x);
                }
            );
        }
        gui_Buttons((ServerPlayer*)b.getEntity(),"前往地标\n","warp",lis);
    }
    //go
    auto it=warp.find(a[0]);
        if(it!=warp.end()) {
            it->second.tele(*b.getEntity());
            outp.success("§e[Teleport] 已传送到Warp");
            return;
        }
}

static unordered_map<string,pair<Vec3,int> > deathpoint;
static void oncmd_back(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    if(!CanBack) {outp.error("back not enabled on this server"); return;}
    auto it=deathpoint.find(b.getName());
    if(it==deathpoint.end()){
        outp.error("cant find deathpoint");
        return;
    }
    ServerPlayer* sp=(ServerPlayer*)b.getEntity();
    TeleportA(*sp,it->second.first,{it->second.second});
    deathpoint.erase(it);
    outp.success("okay!back to deathpoint");
}
static void handle_mobdie(Mob& mb,const ActorDamageSource&){
    if(!CanBack) return;
    auto sp=getSP(mb);
    if(sp){
        ServerPlayer* sp=(ServerPlayer*)&mb;
        sendText(sp,"use /back to return last deathpoint");
        deathpoint[sp->getName()]={sp->getPos(),sp->getDimensionId()};
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
    printf("[TPs] loaded! V2019-12-14\n");
    load();
    load_warps_new();
    load_cfg();
    register_cmd("suicide",(void*)oncmd_suic,"自杀");
    register_cmd("tpa",(void*)oncmd,"传送系统");
    register_cmd("home",(void*)oncmd_home,"家");
    register_cmd("warp",(void*)oncmd_warp,"地标");
    register_cmd("back",(void*)oncmd_back,"back to deathpoint");
    reg_mobdie(handle_mobdie);
    load_helper(modlist);
}
