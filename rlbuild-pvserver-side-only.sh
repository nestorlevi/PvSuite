#!/bin/bash
# please adjust this shell script to your needs
echo "################################################################"
echo "# start compiling with ./rlbuild-pvserver-side-only.sh !!!     #"
echo "################################################################"
./rlcompile-fake-qmake.sh
echo   "try to find fake_qmake"
export fakeqmake=$(echo ${PWD}/fake_qmake/fake_qmake)
echo "################################################################"
echo "build pvserver libraries for inetd and for multithreaded pvserver + build a very old pvserver"
./rlbuild-pvserver.sh
echo "build rllib"
./rlbuild-rllib.sh
echo "build pvserver language binding for lua"
./rlbuild-language-lua.sh
echo "build start_pvbapp"
cd start_pvbapp
#../qmake.sh start_pvbapp.pro
#make
./build.sh
cd ..
echo "################################################################"
echo "# finished compiling !!!                                       #"
echo "#              _                                               #"
echo "#   _ ____   _| |__  _ __ _____      _____  ___ _ __           #"
echo "#  | '_ \ \ / / '_ \| '__/ _ \ \ /\ / / __|/ _ \ '__|          #"
echo "#  | |_) \ V /| |_) | | | (_) \ V  V /\__ \  __/ |             #"
echo "#  | .__/ \_/ |_.__/|_|  \___/ \_/\_/ |___/\___|_|             #"
echo "#  |_|                                                         #"
echo "# verify that no errors occured by running me again            #"
echo "# now run:                                                     #"
echo "#   su                                                         #"
echo "#   ./install.sh                                               #"
echo "#   exit                                                       #"
echo "# if you want to use Qt Designer for designing your masks,     #"
echo "#   copy the plugins to Qt Designer (read designer/README.txt) #"
echo "# Have a lot of fun (Yours pvbrowser community)                #"
echo "################################################################"
