#include<cstdio>
#include<list>
#include<forward_list>
#include<string>
#include<unordered_map>
#include"../cmdhelper.h"
#include<vector>
#include<Loader.h>
#include<MC.h>
#include"../serial/seral.hpp"
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
unordered_map<int,BaseForm*> id_forms;
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
static void sendStr(ServerPlayer& sp,string_view fm,int id){
    gGUIPK.send(sp,fm,id);
}
int autoid;
BDL_EXPORT void sendForm(ServerPlayer& sp,BaseForm* fm){
    if(id_forms.size()>128){
        for(auto& i:id_forms){
            delete i.second;
        }
        id_forms.clear();
        printf("[GUI] Warning!Form Spam Detected!Clearing Form datas.Last Player %s\n",sp.getName().c_str());
    }
    auto x=fm->getstr();
    fm->setID(++autoid);
    id_forms[autoid]=fm;
    sendStr(sp,x,fm->getid());
}
BDL_EXPORT void sendForm(string_view sp,BaseForm* fm){
    auto x=getplayer_byname(sp);
    if(x)
    sendForm(*x,fm);
}
THook(void*,_ZN20ServerNetworkHandler6handleERK17NetworkIdentifierRK23ModalFormResponsePacket,ServerNetworkHandler* sh,NetworkIdentifier const& iden,Packet* pk){
     ServerPlayer* p=sh->_getServerPlayer(iden,pk->getClientSubId());
    if(p){
       // printf("handle %d [%s]\n",access(pk,int,36),access(pk,string,40).c_str());
        int id=access(pk,int,36);
        if(id_forms.count(id)){
            id_forms[id]->process(access(pk,string,40));
            delete id_forms[id];
            id_forms.erase(id);
        }
    }
    return nullptr;
}

void gui_ChoosePlayer(ServerPlayer* sp,string_view text,string_view title,std::function<void(string_view)> cb){
    vector<string> names;
    get_player_names(names);
    Form* fm=new Form(cb);
    fm->setContent(text);
    fm->setTitle(title);
    for(auto& i:names){
        fm->addButton(i,i);
    }
    sendForm(*sp,fm);
}
void gui_GetInput(ServerPlayer* sp,string_view text,string_view title,std::function<void(string_view)> cb){
    SimpleInput* fm=new SimpleInput(title,cb);
    fm->addInput(text);
    sendForm(*sp,fm);
}
void gui_Buttons(ServerPlayer* sp,string_view text,string_view title,const list<pair<string,std::function<void()> > >* li){
    Form* fm=new Form([li](string_view x)->void{
        for(const auto& i:*li){
            if(i.first==x){
                i.second();
                delete li;
                return;
            }
        }
        delete li;
        return;
    });
    fm->setTitle(title);
    fm->setContent(text);
    for(const auto& i:*li){
        fm->addButton(i.first,i.first);
    }
    sendForm(*sp,fm);
}
void mod_init(std::list<string>& modlist) {
    printf("[GUI] Loaded " BDL_TAG "\n");
    load_helper(modlist);
}