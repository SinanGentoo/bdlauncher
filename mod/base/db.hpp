#pragma once
#include"stkbuf.hpp"
#include <leveldb/db.h>
#include <string>
#include <cstdio>
#include <functional>
#include <cstdlib>
#include<string_view>
#include <cstring>
using std::function;
using std::string;
using std::string_view;
using leveldb::Slice;
struct LDBImpl
{
    leveldb::DB *db;
    leveldb::ReadOptions rdopt;
    leveldb::WriteOptions wropt;
    void load(const char *name, bool read_cache=true){
        rdopt = leveldb::ReadOptions();
        wropt = leveldb::WriteOptions();
        rdopt.fill_cache = read_cache;
        rdopt.verify_checksums = false;
        wropt.sync = false;
        leveldb::Options options;
        options.create_if_missing = true;
        leveldb::Status status = leveldb::DB::Open(options, name, &db);
        if(!status.ok()){
            printf("[DB ERROR] cannot load %s reason: %s\n",name,status.ToString().c_str());
        }
        assert(status.ok());
    }
    LDBImpl(const char *name, bool read_cache=true)
    {
        load(name,read_cache);
    }
    void close(){
        delete db;
    }
    ~LDBImpl()
    {
        close();
    }
    bool Get(string_view key, string &val) const
    {
        auto s = db->Get(rdopt, Slice(key.data(),key.size()), &val);
        return s.ok();
    }
    void Put(string_view key, string_view val)
    {
        auto s = db->Put(wropt, Slice(key.data(),key.size()), Slice(val.data(),val.size()));
        if (!s.ok())
        {
            printf("[DBError] %s\n", s.ToString().c_str());
        }
    }
    bool Del(string_view key)
    {
        auto s = db->Delete(wropt, Slice(key.data(),key.size()));
        return s.ok();
    }
    void Iter(function<void(string_view, string_view)> fn) const
    {   
        leveldb::Iterator *it = db->NewIterator(rdopt);
        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
            auto k=it->key();
            auto v=it->value();
            fn({k.data(),k.size()},{v.data(),v.size()});
        }
        delete it;
    }
    void CompactAll(){
        db->CompactRange(nullptr,nullptr);
    }
};