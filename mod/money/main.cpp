#include<cstdio>
#include<list>
#include<string>
#include<unordered_map>
#include"../cmdhelper.h"
#include<vector>
#include<Loader.h>
#include<MC.h>
#include"../base/base.h"
#include"seral.hpp"
#include <sys/stat.h>
#include<unistd.h>
#include <sys/stat.h>
#include<memory>
#include"money.h"
#include<dlfcn.h>
#include"rapidjson/document.h"
#include<fstream>
#include"../gui/gui.h"
#include"../serial/seral.hpp"
using std::string;
using std::to_string;
extern "C" {
    BDL_EXPORT void mod_init(std::list<string>& modlist);
}
extern void load_helper(std::list<string>& modlist);
LDBImpl db("data/new/money");
static void DO_DATA_CONVERT(const char* buf,int sz){
    printf("size %d\n",sz);
    DataStream ds;
    ds.dat=string(buf,sz);
    int length;
    ds>>length;
    for(int i=0;i<length;++i){
        string key,val;
        ds>>key>>val;
        printf("key %s val %d\n",key.c_str(),val.size());
        db.Put(key,val);
    }
}
static void load() {
    char* buf;
    int sz;
    struct stat tmp;
    if(stat("data/money/money.db",&tmp)!=-1) {
        file2mem("data/money/money.db",&buf,sz);
        DO_DATA_CONVERT(buf,sz);
        printf("[MONEY] DATA CONVERT DONE;old:data/money/money.db.old\n");
        link("data/money/money.db","data/money/money.db.old");
        unlink("data/money/money.db");
        free(buf);
    }
}
int INIT_MONEY;
using namespace rapidjson;
void loadcfg(){
    Document dc;
    char* buf;
    int siz;
    file2mem("config/money.json",&buf,siz);
    if(dc.ParseInsitu(buf).HasParseError()){
        printf("[MONEY] Config JSON ERROR!\n");
        exit(1);
    }
    INIT_MONEY=dc["init_money"].GetInt();
    free(buf);
}
int get_money(const string& pn) {
    //lazy init
    string val;
    auto succ=db.Get(pn,val);
    if(!succ) return INIT_MONEY;
    return access(val.data(),int,0);
}
void set_money(const string& pn,int am) {
    string val((char*)&am,4);
    db.Put(pn,val);
}
bool red_money(const string& pn,int am) {
    int mo=get_money(pn);
    if(mo>=am) {
        set_money(pn,mo-am);
        return true;
    } else
    {
        return false;
    }

}
void add_money(const string& pn,int am) {
    red_money(pn,-am);
}
static void oncmd(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    ARGSZ(1)
    if(a[0]=="query") {
        string dst;
        if(a.size()==2) {
            dst=a[1];
        } else {
            dst=b.getName();
        }
        char buf[1024];
        sprintf(buf,"§e[Money system] %s 的余额为 %d",dst.c_str(),get_money(dst));
        outp.addMessage(string(buf));
        outp.success();
        return;
    }
    if(a[0]=="set") {
        if((int)b.getPermissionsLevel()<1) return;
        ARGSZ(2)
        string dst;
        int amo;
        if(a.size()==3) {
            dst=a[1];
            amo=atoi(a[2].c_str());
        } else {
            dst=b.getName();
            amo=atoi(a[1].c_str());
        }
        set_money(dst,amo);
        char buf[1024];
        sprintf(buf,"§e[Money system] 成功将 %s 的余额设置为 %d",dst.c_str(),get_money(dst));
        outp.success(string(buf));
    }
    if(a[0]=="add") {
        if((int)b.getPermissionsLevel()<1) return;
        ARGSZ(2)
        string dst;
        int amo;
        if(a.size()==3) {
            dst=a[1];
            amo=atoi(a[2].c_str());
        } else {
            dst=b.getName();
            amo=atoi(a[1].c_str());
        }
        add_money(dst,amo);
        char buf[1024];
        sprintf(buf,"§e[Money system] 往 %s 的账户上添加了 %d",dst.c_str(),amo);
        outp.success(string(buf));
                auto dstp=getplayer_byname(dst);
                if(dstp)
                    sendText(dstp,"you get "+std::to_string(amo)+" money");
    }
    if(a[0]=="reduce" || a[0]=="rd") {
        if((int)b.getPermissionsLevel()<1) return;
        ARGSZ(2)
        string dst;
        int amo;
        if(a.size()==3) {
            dst=a[1];
            amo=atoi(a[2].c_str());
        } else {
            dst=b.getName();
            amo=atoi(a[1].c_str());
        }
        if(red_money(dst,amo)) {
            outp.success("§e[Money system] 扣除完毕");
                auto dstp=getplayer_byname(dst);
                if(dstp)
                    sendText(dstp,"you used "+std::to_string(amo)+" money");
        }
        else {
            outp.error("[Money system] 对方余额不足");
        }
    }
    if(a[0]=="pay") {
        ARGSZ(3)
        string pl;
        string pl2;
        int mon;
        pl=b.getName();
        pl2=a[1];
        mon=atoi(a[2].c_str());
        if(mon<=0||mon>50000) {
            outp.error("[Money system] 输入数值过大或过小(最大转账数50000，更大请分次转)");
        } else {
            if(red_money(pl,mon)) {
                add_money(pl2,mon);
                char msg[1000];
                sprintf(msg,"§e[Money system] 你给了 %s %d 余额",pl2.c_str(),mon);
                outp.success(string(msg));
                auto dstp=getplayer_byname(pl2);
                if(dstp)
                    sendText(dstp,"you get "+std::to_string(mon)+" money");
            } else {
                outp.error("[Money system] Sorry，转账失败，请检查你的余额是否足够。");
            }
        }
    }
    if(a[0]=="paygui"){
        string nm=b.getName();
        gui_ChoosePlayer((ServerPlayer*)b.getEntity(),"Choose a player to pay","Money System",[nm](const string& chosen){
            auto p=getplayer_byname(nm);
            if(p){
                gui_GetInput((ServerPlayer*)p,"How much to pay to "+chosen+"?","Money System",[chosen,nm](const string& mon){
                    auto p=getplayer_byname(nm);
                    if(p)
                        runcmdAs("money pay \""+chosen+"\" "+mon,p);
                });
            }
        });
    }
     if(a[0]=="help") {
        outp.error("经济系统命令列表:\n/money query ——查询自己余额\n/money pay 玩家ID 余额 ——给玩家打钱");
    }
}

void mod_init(std::list<string>& modlist) {
    printf("[MONEY] loaded! V2019-12-11\n");
    load();
    loadcfg();
    register_cmd("money",(void*)oncmd,"经济系统");
    register_cmd("reload_money",(void*)loadcfg,"reload money cfg",1);
    load_helper(modlist);
}
