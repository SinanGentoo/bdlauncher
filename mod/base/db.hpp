#pragma once
#include <leveldb/db.h>
#include <string>
#include <cstdio>
#include <functional>
#include <cstdlib>
#include <cstring>
using std::function;
using std::string;
struct LDBImpl
{
    leveldb::DB *db;
    leveldb::ReadOptions rdopt;
    leveldb::WriteOptions wropt;
    LDBImpl(const char *name, bool read_cache=true)
    {
        rdopt = leveldb::ReadOptions();
        wropt = leveldb::WriteOptions();
        rdopt.fill_cache = read_cache;
        rdopt.verify_checksums = false;
        wropt.sync = false;
        leveldb::Options options;
        options.create_if_missing = true;
        leveldb::Status status = leveldb::DB::Open(options, name, &db);
        assert(status.ok());
    }
    ~LDBImpl()
    {
        delete db;
    }
    bool Get(const string &key, string &val) const
    {
        auto s = db->Get(rdopt, key, &val);
        return s.ok();
    }
    void Put(const string &key, const string &val)
    {
        auto s = db->Put(wropt, key, val);
        if (!s.ok())
        {
            printf("[DBError] %s\n", s.ToString().c_str());
        }
    }
    bool Del(const string &key)
    {
        auto s = db->Delete(wropt, key);
        return s.ok();
    }
    void Iter(function<void(const string &, const string &)> fn) const
    {
        leveldb::Iterator *it = db->NewIterator(rdopt);
        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
            fn(it->key().ToString(), it->value().ToString());
        }
        delete it;
    }
};