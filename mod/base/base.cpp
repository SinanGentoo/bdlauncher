#define BASE
#include"../cmdhelper.h"
#include"../myhook.h"
#include<Loader.h>
#include<MC.h>
#include<vector>
#include<seral.hpp>
#include<signal.h>
#include <sys/stat.h>
#include<unistd.h>
using std::list;
using std::vector;
#include"base.h"
#include"hook.hpp"
#include"cmdreg.hpp"
#include"utils.hpp"
#include<sstream>
#include"base.h"
#include"dbimpl.hpp"
extern "C" {
   BDL_EXPORT void mod_init(list<string>& modlist);
}

//export APIS
void split_string(string_view s, std::vector<std::string_view>& v, string_view c)
{
  std::string::size_type pos1, pos2;
  pos2 = s.find(c);
  pos1 = 0;
  while(std::string::npos != pos2)
  {
    v.emplace_back(s.data()+pos1,pos2-pos1); //s.substr(pos1, pos2-pos1)
    pos1 = pos2 + c.size();
    pos2 = s.find(c, pos1);
  }
  if(pos1 != s.length())
    v.emplace_back(s.data()+pos1,s.size()-pos1);
}
void split_string(string_view s, static_deque<std::string_view>& v, string_view c)
{
  std::string::size_type pos1, pos2;
  pos2 = s.find(c);
  pos1 = 0;
  while(std::string::npos != pos2)
  {
    v.push_back(s.substr(pos1, pos2-pos1)); //
    pos1 = pos2 + c.size();
    pos2 = s.find(c, pos1);
    if(v.full()) return;
  }
  if(pos1 != s.length() && !v.full())
    v.push_back(string_view(s.data()+pos1,s.size()-pos1));
}
bool execute_cmd_random(const static_deque<string_view>& chain){
    int rd=rand()%chain.size();
    return execute_cmdchain(chain[rd],"",false);
}

bool execute_cmdchain(string_view chain,string_view sp,bool chained){  
    auto fg=false;
    if(sp[0]!='"') fg=1;
    char buf[8192];
    const char* src=chain.data();
    int bufsz=0;
    for(int i=0;i<chain.size();++i){
        if(memcmp(src+i,"%name%",6)==0){
            if(fg) buf[bufsz++]='"';
            memcpy(buf+bufsz,sp.data(),sp.size());
            bufsz+=sp.size();
            if(fg) buf[bufsz++]='"';
            i+=5;
        }else{
            buf[bufsz++]=src[i];
        }
    }
    auto chainxx=string_view(buf,bufsz);
    if(chainxx[0]=='!'){
        static_deque<string_view> dst;
        split_string(chainxx.substr(1),dst,";");
        return execute_cmd_random(dst);
    }
    static_deque<string_view> dst;
    split_string(chainxx,dst,",");
    for(auto& i:dst){
        auto res=runcmd(i);
        if(!res.isSuccess() && chained) return false;
    }
    return true;
}
struct TeleportCommand {
    char filler[1024];
    TeleportCommand();
    void teleport(Actor &,Vec3,Vec3*,AutomaticID<Dimension,int>) const;
};
//https://github.com/PetteriM1/NukkitPetteriM1Edition/blob/master/src/main/java/cn/nukkit/network/protocol/TextPacket.java
/* this.putByte(this.type);
        this.putBoolean(this.isLocalized || type == TYPE_TRANSLATION);
        switch (this.type) {
            case TYPE_RAW:
            case TYPE_TIP:
            case TYPE_SYSTEM:
            case TYPE_JSON:
                this.putString(this.message);
                break;
            case TYPE_TRANSLATION:
            case TYPE_POPUP:
            case TYPE_JUKEBOX_POPUP:
                this.putString(this.message);
                this.putUnsignedVarInt(this.parameters.length);
                for (String parameter : this.parameters) {
                    this.putString(parameter);
                }
        }
                this.putString(this.xboxUserId);
            this.putString(this.platformChatId);
    }*/
struct MyTxtPk{
    string_view str;
    TextType type;
    MyPkt* pk;
    void setText(string_view ct,TextType typ){
        str=ct;
        type=typ;
    }
    MyTxtPk(){
        pk=new MyPkt(0x9,[this](void*,BinaryStream& bs){
            bs.writeByte(this->type);
            bs.writeBool(false);
            bs.writeUnsignedVarInt(this->str.size());
            bs.write(this->str.data(),this->str.size());
            bs.writeUnsignedInt64(0);
            /*
            bs.writeUnsignedVarInt(0);
            bs.writeUnsignedVarInt(0);//padding
            bs.writeUnsignedVarInt(0);*/
        });
    }
    void send(Player* p){
        ((ServerPlayer*)p)->sendNetworkPacket(*pk);
    }
};
MyTxtPk gTextPkt;
void sendText(Player* a,string_view ct,TextType type) {
    gTextPkt.setText(ct,type);
    gTextPkt.send(a);
}
void broadcastText(Player* a,string_view ct,TextType type){
    gTextPkt.setText(ct,type);
    auto& vc=*getSrvLevel()->getUsers();
    for(auto& i:vc){
        gTextPkt.send(i.get());
    }
}
static TeleportCommand cmd_p;

extern void load_helper(list<string>& modlist);
void TeleportA(Actor& a,Vec3 b,AutomaticID<Dimension,int> c) {
    if(a.getDimensionId()!=c){
        cmd_p.teleport(a,b,nullptr,c);
        cmd_p.teleport(a,b,nullptr,c);
    }else{
        cmd_p.teleport(a,b,nullptr,c);
    } 
}
void KillActor(Actor* a) {
    //!!! dirty workaround
    ((Mob*)a)->kill();
}

#define fcast(a,b) (*((a*)(&b)))
ServerPlayer* getplayer_byname2(string_view name) {
    ServerPlayer* rt=NULL;
    auto vc=getSrvLevel()->getUsers();
    for(auto& tg:*vc){
        string bf=tg->getName();
        #define min(a,b) ((a)<(b)?(a):(b))
        int sz=min(bf.size(),name.size());
        int eq=1;
        for(int i=0; i<sz; ++i) {
            if(tolower(bf[i])!=tolower(name[i])) {
                eq=0;
                break;
            }
        }
        if(eq) {
            rt=tg.get();
            return rt;
        }
        continue;
    }
    return rt;
}

void get_player_names(vector<string>& a){
    auto vc=getSrvLevel()->getUsers();
    for(auto& i:*vc){
        a.emplace_back(i->getName());
    }
}
static Minecraft* MC;
BDL_EXPORT Minecraft* _getMC() {
    return MC;
}
THook(void*,_ZN14ServerCommands19setupStandardServerER9MinecraftRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES9_P15PermissionsFile,Minecraft& a, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const& d, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const& b, PermissionsFile* c){
    auto ret=original(a,d,b,c);
    printf("MC %p\n",&a);
    MC=&a;
    return ret;
}
THook(void*,_ZN10SayCommand5setupER15CommandRegistry,CommandRegistry& thi,CommandRegistry& a){
    handle_regcmd(thi);
    return original(thi,a);
}
struct DedicatedServer
{
    void stop();
};
DedicatedServer* dserver;
THook(void*,_ZN15DedicatedServer5startERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE,DedicatedServer* t,string& b){
    printf("starting server %p\n",t);
    dserver=t;
    return original(t,b);
}
void set_int_handler(void* fn);
static int ctrlc;
static void autostop(){
        ctrlc++;
        if(ctrlc>=3){
            printf("killing server\n");
            exit(0);
        }
        if(dserver){
            printf("stoping server\n");
            dserver->stop();
        }
}
int getPlayerCount(){
    return getSrvLevel()->getUserCount();
}
int getMobCount(){
    return getSrvLevel()->getTickedMobCountPrevious();
}

NetworkIdentifier* getPlayerIden(ServerPlayer& sp){
    return access(&sp,NetworkIdentifier*,2952);
}
ServerPlayer* getuser_byname(string_view a){
    auto vc=getSrvLevel()->getUsers();
    for(auto& i:*vc){
        if(i->getName()==a) return i.get();
    }
    return nullptr;
}
void forceKickPlayer(ServerPlayer& sp){
    sp.disconnect();
}
extern "C"{
	void _ZTV19ServerCommandOrigin();
	void _ZN19ServerCommandOriginD2Ev();
};
void* fake_vtbl[(0x000000000ABE2610-0x000000000ABE2528)/8];
static ServerCommandOrigin* SCO;
static string sname("Server");
extern "C"{
    MCRESULT _ZNK17MinecraftCommands23requestCommandExecutionESt10unique_ptrI13CommandOriginSt14default_deleteIS1_EERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEib(MinecraftCommands*,CommandOrigin**,std::string const &, int, bool);
};
BDL_EXPORT MCRESULT runcmd(string_view a) {
    //ServerOrigin is const so we can simply share it here
    //char fakeOrigin[72];
    CommandOrigin** ori_ptr=(CommandOrigin**)&SCO;
    if(!SCO){
        SCO=new ServerCommandOrigin(sname,*(ServerLevel*)MC->getLevel(),(CommandPermissionLevel)5);
        access(SCO,void*,0)=fake_vtbl+2;
    }
    //memcpy(fakeOrigin,SCO,sizeof(fakeOrigin));
    return _ZNK17MinecraftCommands23requestCommandExecutionESt10unique_ptrI13CommandOriginSt14default_deleteIS1_EERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEib(MC->getCommands(),ori_ptr,string(a),4,1);
}
struct PlayerCommandOrigin{
    char filler[72];
    PlayerCommandOrigin(Player&);
};
BDL_EXPORT MCRESULT runcmdAs(string_view a,Player* sp) {
    auto ori=(CommandOrigin*)new PlayerCommandOrigin(*sp);
    return _ZNK17MinecraftCommands23requestCommandExecutionESt10unique_ptrI13CommandOriginSt14default_deleteIS1_EERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEib(MC->getCommands(),&ori,string(a),4,1);       
}
static void dummy__(){}
void mod_init(list<string>& modlist)
{
    uintptr_t* pt=(uintptr_t*)_ZTV19ServerCommandOrigin;
    memcpy(fake_vtbl,(void*)_ZTV19ServerCommandOrigin,sizeof(fake_vtbl));
    printf("[BASE] fake vtable pt %p %p %p %p\n",pt[0],pt[1],pt[2],_ZN19ServerCommandOriginD2Ev); //pt[3]=D0ev
    fake_vtbl[3]=(void*)dummy__; //fix free(ServerCommandOrigin)
    mkdir("data_v2",S_IRWXU);
    printf("[MOD/BASE] loaded! " BDL_TAG "\n");   	
    set_int_handler(fp(autostop));		
    load_helper(modlist);
}
