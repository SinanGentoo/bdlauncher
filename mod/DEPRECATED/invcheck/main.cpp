#include<Loader.h>
#include<MC.h>
#include"../base/base.h"
#include"../cmdhelper.h"
#include"../base/db.hpp"
#include"../serial/seral.hpp"
extern "C" {
   BDL_EXPORT void mod_init(std::list<string>& modlist);
}
extern void load_helper(std::list<string>& modlist);
string dumpSP(ServerPlayer& sp){
    string ret;
    auto& x=sp.getSupplies();
    int sz=x.getContainerSize((ContainerID)0ul);
    for(int i=0;i<sz;++i){
        auto& item=x.getItem(i,(ContainerID)0ul);
        //printf("%s\n",item.toString().c_str());
        ret+=item.toString()+" | ";
    }
    return ret;
}
string dumpSP_Ender(ServerPlayer& sp){
    string ret;
    auto* x=sp.getEnderChestContainer();
    if(!x){
        printf("no ender found\n");
        return "";
    }
    int sz=x->getContainerSize();
    for(int i=0;i<sz;++i){
        auto& item=x->getItem(i);
        //printf("%s\n",item.toString().c_str());
        ret+=item.toString()+" | ";
    }
    return ret;
}
string dumpall(ServerPlayer* sp){
    string ret;
    ret=dumpSP(*sp);
    ret+="\n-----starting ender chest dump-----\n";
    ret+=dumpSP_Ender(*sp)+"\n";
    return ret;
}
static void oncmd(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    ARGSZ(1)
    auto sp=getSP(getplayer_byname2(a[0]));
    if(!sp){
        outp.error("cant find user");
        return;
    }
    outp.addMessage(dumpall(sp));
}
static void onJoin(ServerPlayer* sp){
    auto inv=dumpall(sp);
    int fd=open(("invdump/"+sp->getName()).c_str(),O_WRONLY|O_TRUNC|O_CREAT,S_IRWXU);
    write(fd,inv.data(),inv.size());
    close(fd);
}
void mod_init(std::list<string>& modlist){
    register_cmd("invcheck",fp(oncmd),"inspect player's inventory",1);
    mkdir("invdump",S_IRWXU);
    reg_player_join(onJoin);
    printf("[INVCheck] loaded! V2019-12-11\n");
    load_helper(modlist);
}
