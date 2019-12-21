#pragma once
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include<functional>
#include<vector>
using std::vector;
using std::string;
using namespace rapidjson;
struct BaseForm{
    virtual void setID(int i)=0;
    virtual string_view getstr()=0;
    virtual int getid()=0;
    virtual void process(const string&)=0;
};

struct Form:BaseForm{
    int fid;
    Document dc;
    vector<string> labels;
    std::function< void(string_view)> cb;
    StringBuffer buf;
    Form(std::function< void(string_view)> cbx){
        cb=cbx;
        dc.SetObject();
        dc.AddMember("type","form",dc.GetAllocator());
        dc.AddMember("title","",dc.GetAllocator());
        dc.AddMember("content","",dc.GetAllocator());
        Value x(kArrayType);
        dc.AddMember("buttons",x,dc.GetAllocator());
    }
    void process(const string& d){
        //printf("gui get %s\n",d.c_str());
        if(d[0]=='n') return;
        int idx=atoi(d.c_str());
        cb(labels[idx]);
    }
    void setID(int i){
        fid=i;
    }
    int getid(){
        return fid;
    }
    string_view getstr(){
        Writer<StringBuffer> writer(buf);
        dc.Accept(writer);
        return {buf.GetString(),buf.GetSize()};
    }
    Form* addButton(string_view text,string_view label){
        Value bt(kObjectType);
        Value ss;
        ss.SetString(text.data(),text.size(),dc.GetAllocator());
        bt.AddMember("text",ss,dc.GetAllocator());
        dc["buttons"].PushBack(bt,dc.GetAllocator());
        labels.emplace_back(label);
        return this;
    }
    Form* setContent(string_view text){
        dc["content"].SetString(text.data(),text.size(),dc.GetAllocator());
        return this;
    }
    Form* setTitle(string_view text){
        dc["title"].SetString(text.data(),text.size(),dc.GetAllocator());
        return this;
    }
};
struct SimpleInput:BaseForm{
    int fid;
    Document dc;
    std::function< void(string_view)> cb;
    StringBuffer buf;
    SimpleInput(string_view ti,std::function< void(string_view)> cbx){
        cb=cbx;
        Value ss;
        dc.SetObject();
        ss.SetString(ti.data(),ti.size(),dc.GetAllocator());
        dc.AddMember("type","custom_form",dc.GetAllocator());
        dc.AddMember("title",ss,dc.GetAllocator());
        Value x(kArrayType);
        dc.AddMember("content",x,dc.GetAllocator());
    }
    void process(const string& d){
        if(d[0]=='n') return;
        cb(string_view(d).substr(2,d.size()-5));
    }
    int getid(){
        return fid;
    }
    void setID(int i){
        fid=i;
    }
    string_view getstr(){
        Writer<StringBuffer> writer(buf);
        dc.Accept(writer);
        return {buf.GetString(),buf.GetSize()};
    }
    SimpleInput* addInput(string_view text){
        Value tmp(kObjectType);
        Value ss;
        ss.SetString(text.data(),text.size(),dc.GetAllocator());
        tmp.AddMember("type","input",dc.GetAllocator())
        .AddMember("text",ss,dc.GetAllocator())
        .AddMember("placeholder",Value(kNullType),dc.GetAllocator())
        .AddMember("default",Value(kNullType),dc.GetAllocator());
        dc["content"].GetArray().PushBack(tmp,dc.GetAllocator());
        return this;
    }
};
BDL_EXPORT void sendForm(ServerPlayer& sp,BaseForm* fm);
BDL_EXPORT void sendForm(string_view sp,BaseForm* fm);

BDL_EXPORT void gui_ChoosePlayer(ServerPlayer* sp,string_view text,string_view title,std::function<void(string_view)> cb);
BDL_EXPORT void gui_GetInput(ServerPlayer* sp,string_view text,string_view title,std::function<void(string_view)> cb);
using std::pair;
BDL_EXPORT void gui_Buttons(ServerPlayer* sp,string_view text,string_view title,const list<pair<string,std::function<void()> > >* li);
