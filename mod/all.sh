export BDL_CXX="g++"
export BDL_CXXFLAG="-O3 -flto -Wl,-z,relro,-z,now"
export BDL_ADDLINK="../libleveldb.a"
cd base;sh cmp.sh;cd ..;
cd opti;sh cmp.sh;cd ..;
cd bear;sh cmp.sh;cd ..;
cd land;sh cmp.sh;cd ..;
cd money;sh cmp.sh;cd ..;
cd tptp;sh cmp.sh;cd ..;
cd gui;sh cmp.sh;cd ..;
#cd antixray;sh cmp.sh;cd ..;
cd cdk;sh cmp.sh;cd ..;
cd transfer;sh cmp.sh;cd ..;
cd cmdhelp;sh cmp.sh;cd ..;
cd killbonus;sh cmp.sh;cd ..;
cd protect;sh cmp.sh;cd ..;
