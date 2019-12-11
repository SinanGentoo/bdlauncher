g++ -w -fvisibility=hidden -shared -fPIC -Wl,-z,relro,-z,now -O3 -flto -std=gnu++17 -I ../base -I ../../include base.cpp ../libleveldb.a -o ../../out/base.so
