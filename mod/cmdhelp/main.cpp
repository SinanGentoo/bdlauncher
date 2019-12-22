#include<Loader.h>
#include<MC.h>
#include"../base/base.h"
#include"rapidjson/document.h"
#include"rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include<fstream>
#include<cstdarg>
#include"../gui/gui.h"
#include"../serial/seral.hpp"
#include<queue>
using namespace std;

extern "C" {
   BDL_EXPORT void mod_init(std::list<string>& modlist);
}
extern void load_helper(std::list<string>& modlist);
struct CMD{
    string cond,run;
    bool execute(const string& name,bool chained=true){
        if(execute_cmdchain(cond,name,chained)) {execute_cmdchain(run,name,false);return true;}
        return false;
    }
    CMD(const string& a,const string& b){
        cond=a,run=b;
    }
    CMD(){}
};
struct Timer{
    int tim,sft;
    CMD run;
    bool chk(int tk){
        return tk%tim==sft;
    }
    Timer(int a,int b,const CMD& c){
        tim=a,sft=b,run=c;
    }
    Timer(){}
};

struct CMDForm{
    unordered_map<STRING_HASH,CMD> cmds;
    vector<string> ordered_cmds;
    string content;
    string title;
    Form base;
    StaticForm sbase;
    void sendTo(ServerPlayer& sp){
        string nm=sp.getNameTag(); //!!! fix BUG: setname("newname")
        StaticForm* sf=new StaticForm(sbase);
        sf->cb=[this,nm](string_view s)->void{
            auto hs=do_hash(s);
            auto it=cmds.find(hs);
            if(it!=cmds.end())
                it->second.execute(nm);
        };
        sendForm(sp,sf);
    }
    void init(){
        base.setContent(content)->setTitle(title);
        for(auto& i:ordered_cmds)
            base.addButton(i,i);
        sbase.load(base);
    }
};
unordered_map<STRING_HASH,CMDForm> forms;
list<Timer> timers;
struct Oneshot{
    int time;
    string name,cmd;
    bool operator <(Oneshot const& a) const{
        return time<a.time;
    }
    Oneshot(int a,const string& b,const string& c){
        time=a,name=b;cmd=c;
    }
};
priority_queue<Oneshot> oneshot_timers;
CMD joinHook;
static void oncmd(std::vector<string_view>& a,CommandOrigin const & b,CommandOutput &outp) {
    if(a.size()==0) a.emplace_back("root");
    auto hs=do_hash(a[0]);
    if(forms.count(hs)){
        auto& fm=forms[hs];
        ServerPlayer* sp=getSP(b.getEntity());
        if(sp){
            fm.sendTo(*sp);
            outp.success();
        }else{
            outp.error("fucku");
        }
        //outp.success("gui showed");
    }else{
        outp.error("cant find");
    }
}
static void join(ServerPlayer* sp){
    joinHook.execute(sp->getNameTag(),false);
}
static void tick(int tk){
    for(auto& i:timers){
        if(i.chk(tk)) i.run.execute("",false);
    }
    while(!oneshot_timers.empty()){
        auto a=oneshot_timers.top();
        if(a.time<=tk){
            //printf("exec %s %s\n",a.cmd.c_str(),a.name.c_str());
            for(int i=0;i<a.cmd.size();++i)
                if(a.cmd[i]=='$') a.cmd[i]='%';
            execute_cmdchain(a.cmd,a.name,false);
            oneshot_timers.pop();
        }else{
            break;
        }
    }
}
static int tkl;
THook(void*,_ZN5Level4tickEv,Level& a){
    tkl++;
    if(tkl%20==0) tick(tkl/20);
    return original(a);
}
int menuitem;
using namespace rapidjson;
//vector<string> gStrPool;
static void load(){
    timers.clear();
    forms.clear();
    //gStrPool.clear();
    Document dc;
    char* buf;
    int sz;
    file2mem("config/cmd.json",&buf,sz);
    if(dc.ParseInsitu(buf).HasParseError()){
        printf("[CMDHelper] Config JSON ERROR!\n");
        exit(1);
    }
    for(auto& i:dc.GetArray()){
        auto&& x=i.GetObject();
        auto typ=string(x["type"].GetString());
        if(typ=="menuitem"){
            menuitem=x["itemid"].GetInt();
        }
        if(typ=="join"){
            joinHook={"",string(x["cmd"].GetString())};
        }
        if(typ=="form"){
            auto&& buttons=x["buttons"];
            auto&& cont=x["text"];
            auto&& title=x["title"];
	        auto&& name=x["name"];
            assert(buttons.IsArray());
            auto& cf=forms[do_hash(name.GetString())];
            cf.title=title.GetString();
            cf.content=cont.GetString();
            for(auto& i:buttons.GetArray()){
                assert(i.IsArray());
                auto&& but=i.GetArray();
                if(but.Size()!=3){
                    printf("[CMD] wtf!cmdchain size mismatch.3 required,%d found\n",but.Size());
                    printf("[CMD] Hint:\n");
                    for(auto& j:but){
                        printf("[CMD] %s\n",j.GetString());
                    }
                    exit(0);
                }
                cf.cmds.emplace(do_hash(but[0].GetString()),CMD(but[1].GetString(),but[2].GetString()));
                cf.ordered_cmds.emplace_back(but[0].GetString());
            }
            cf.init();
        }
        if(typ=="timer"){
            timers.emplace_back(x["time"].GetInt(),x["shift"].GetInt(),CMD("",x["cmd"].GetString()));
        }
    }
    free(buf);
}
clock_t lastclk;
ServerPlayer* lastp;
static bool handle_u(GameMode* a0,ItemStack * a1,BlockPos const* a2,BlockPos const* dstPos,Block const* a5){
    if(menuitem!=0 && a1->getId()==menuitem){
        auto sp=a0->getPlayer();
        if(lastp==sp){
            if(clock()-lastclk<CLOCKS_PER_SEC/10){
                return 0;
            }else{
                lastclk=clock();
                runcmdAs("c",sp);
            }
        }else{
                lastclk=clock();
                lastp=sp;
                runcmdAs("c",sp);
        }
        
        return 0;
    }
    return 1;
}
static void oncmd_sch(std::vector<string_view>& a,CommandOrigin const & b,CommandOutput &outp) {
    if((int)b.getPermissionsLevel()<1) return;
    ARGSZ(3)
    int dtime=atoi(a[0]);
    string name=string(a[1]);
    string chain=string(a[2]);
    oneshot_timers.emplace(tkl/20+dtime,name,chain);
    outp.success();
}
static void oncmd_runas(std::vector<string_view>& a,CommandOrigin const & b,CommandOutput &outp) {
    if((int)b.getPermissionsLevel()<1) return;
    ARGSZ(1)
    auto sp=getplayer_byname(a[0]);
    if(sp){
        int vsz=a.size();
        char buf[1000];
        int ptr=0;
        for(int i=1;i<vsz;++i){
            memcpy(buf+ptr,a[i].data(),a[i].size());
            ptr+=a[i].size();
            buf[ptr++]=' ';
        }
        runcmdAs({buf,ptr},sp).isSuccess()?outp.success():outp.error("error");
    }else{
        outp.error("Can't find player");
    }
}
void mod_init(std::list<string>& modlist){
    load();
    register_cmd("c",oncmd,"open gui");
    register_cmd("reload_cmd",load,"reload cmds",1);
    register_cmd("sched",oncmd_sch,"schedule a delayed cmd",1);
    register_cmd("runas",oncmd_runas,"run cmd as",1);
    reg_player_join(join);
    reg_useitemon(handle_u);
    printf("[CMDHelp] loaded! " BDL_TAG "\n");
    load_helper(modlist);
}
