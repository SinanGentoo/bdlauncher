#include<cstdio>
#include<list>
#include<forward_list>
#include<string>
#include<unordered_map>
#include"../cmdhelper.h"
#include<vector>
#include<Loader.h>
#include<MC.h>
#include<unistd.h>
#include<cstdarg>

#include"../base/base.h"
#include<cmath>
#include<deque>
#include<dlfcn.h>
#include<string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "gui.h"
using std::string;
using std::unordered_map;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define dbg_printf(...) {}
//#define dbg_printf printf
extern "C" {
    BDL_EXPORT void mod_init(std::list<string>& modlist);
}
extern void load_helper(std::list<string>& modlist);

using namespace rapidjson;
using std::vector;
using std::unordered_map;
unordered_map<int,SharedForm*> id_forms;
AllocPool<SharedForm> FormMem;
SharedForm* getForm(string_view title,string_view cont,bool isInp){
    return FormMem.get(title,cont,true,isInp); //need free
}
static void relForm(SharedForm* sf){
    if(sf->needfree){
        FormMem.release(sf);
    }
}
struct GUIPK{
    MyPkt* pk;
    string_view fm;
    int id;
    GUIPK(){
        pk=new MyPkt(100,[this](void* x,BinaryStream& b)->void{
            b.writeUnsignedVarInt(id);
            b.writeUnsignedVarInt(fm.size());
            b.write(fm.data(),fm.size());
        });
    }
    void send(ServerPlayer& sp,string_view st,int i){
        fm=st,id=i;
        sp.sendNetworkPacket(*pk);
    }
} gGUIPK;
static inline void sendStr(ServerPlayer& sp,string_view fm,int id){
    gGUIPK.send(sp,fm,id);
}
static int autoid;
BDL_EXPORT void sendForm(ServerPlayer& sp,SharedForm* fm){
    if(id_forms.size()>128){
        for(auto& i:id_forms){
            delete i.second;
        }
        id_forms.clear();
        printf("[GUI] Warning!Form Spam Detected!Clearing Form datas.Last Player %s\n",sp.getName().c_str());
    }
    auto x=fm->serial();
    fm->fid=++autoid;
    id_forms[autoid]=fm;
    sendStr(sp,x,autoid);
}
THook(void*,_ZN20ServerNetworkHandler6handleERK17NetworkIdentifierRK23ModalFormResponsePacket,ServerNetworkHandler* sh,NetworkIdentifier const& iden,Packet* pk){
    ServerPlayer* p=sh->_getServerPlayer(iden,pk->getClientSubId());
    if(p){
        int id=access(pk,int,36);
        auto it=id_forms.find(id);
        if(it!=id_forms.end()){
            it->second->process(p,access(pk,string,40));
            relForm(it->second);
            id_forms.erase(it);
        }
    }
    return nullptr;
}

void gui_ChoosePlayer(ServerPlayer* sp,string_view text,string_view title,std::function<void(ServerPlayer*,string_view)> const& cb){
    vector<string> names;
    get_player_names(names);
    auto fm=getForm(title,text,false);
    fm->cb=[cb](ServerPlayer* sp,string_view sv,int x){
        cb(sp,sv);
    };
    for(auto& i:names){
        fm->addButton(i);
    }
    sendForm(*sp,fm);
}
void mod_init(std::list<string>& modlist) {
    printf("[GUI] Loaded " BDL_TAG "\n");
    load_helper(modlist);
    //PlayerListPacket::add(PlayerListEntry const&)
}