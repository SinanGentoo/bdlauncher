static string sname("Server");
BDL_EXPORT MCRESULT runcmd(string_view a) {
    //printf("runcmd %s\n",a.c_str());
    return getMC()->getCommands()->requestCommandExecution(std::unique_ptr<CommandOrigin>((CommandOrigin*)new ServerCommandOrigin(sname,*(ServerLevel*)getMC()->getLevel(),(CommandPermissionLevel)5)),string(a),4,1);
}
struct PlayerCommandOrigin{
    char filler[512];
    PlayerCommandOrigin(Player&);
};
BDL_EXPORT MCRESULT runcmdAs(string_view a,Player* sp) {
    return getMC()->getCommands()->requestCommandExecution(std::unique_ptr<CommandOrigin>((CommandOrigin*)new PlayerCommandOrigin(*sp)),string(a),4,1);
}

static void (*dummy)(std::vector<string_view>&,CommandOrigin const &,CommandOutput &outp);
        #include<iostream>
static void mylex(string_view oper,std::vector<string_view>& out) {
    //simple lexer
    size_t sz=oper.size();
    const char* c=oper.data();
    size_t tot=0;
    while(tot<sz){
        char now=c[tot];
        if(now=='"'){
            size_t next=oper.find('"',tot+1);
            out.emplace_back(oper.substr(tot+1,next-tot-1));
            if(next==string_view::npos) return;
            tot=next+2; //" x
        }else{
            size_t next=oper.find(' ',tot);
            out.emplace_back(oper.substr(tot,next-tot));
            if(next==string_view::npos) return;
            tot=next+1;
        }
    }
}
struct ACmd : Command {
    CommandMessage msg;
    void* callee;
    ~ACmd() override = default;
    void execute(CommandOrigin const &origin, CommandOutput &outp) override
    {
        std::string oper=msg.getMessage(origin);
        std::vector<string_view> args;
        mylex(oper,args);
        ((typeof(dummy))callee)(args,origin,outp);
    }
};
using std::string;
struct req {
    string name,desc;
    int permlevel;
    void* fn;
};
static std::deque<req> reqs;
BDL_EXPORT void register_cmd(const std::string& name,void (*fn)(std::vector<string_view>&,CommandOrigin const &,CommandOutput &outp),const std::string& desc,int perm) {
    reqs.push_back({name,desc,perm,fp(fn)});
}
BDL_EXPORT void register_cmd(const std::string& name,void (*fn)(void),const std::string& desc,int perm) {
    reqs.push_back({name,desc,perm,fp(fn)});
}
static ACmd* chelper(void* fn) {
    ACmd* fk=new ACmd();
    fk->callee=fn;
    return fk;
}

static void handle_regcmd(CommandRegistry& t) {
    for(auto const& i:reqs) {
        //printf("reg %s %p\n",i.name.c_str(),i.fn);
        t.registerCommand(i.name,i.desc.c_str(),(CommandPermissionLevel)i.permlevel,(CommandFlag)0,(CommandFlag)0x40);
        t.registerOverload2(i.name.c_str(),wr_regcmd(fp(chelper),fp(i.fn)),CommandParameterData(type_id<CommandRegistry, CommandMessage>(), &CommandRegistry::parse<CommandMessage>, "operation", (CommandParameterDataType)0, nullptr, offsetof(ACmd, msg), false, -1));
    }
    reqs.clear();
}

