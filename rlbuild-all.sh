#!/bin/bash
echo "try find libQt6..."
find /usr -name "libQt6*.so"
echo "These are the qmake6 settings"
export USE_CLANG_QMAKE="yes"
qmake6 -query
# please adjust this shell script to your needs
echo "################################################################"
echo "# start compiling with ./rlbuild-all.sh !!!                    #"
echo "################################################################"
export PVB6ROOT=$PWD
echo "PVB6ROOT is"
echo $PVB6ROOT
./rlcompile-fake-qmake.sh
echo   "try to find fake_qmake"
export fakeqmake=$(echo ${PWD}/fake_qmake/fake_qmake)
echo "build qwt"
./rlbuild-qwt.sh
echo "build plugins"
./rlbuild-plugins.sh
echo "build pvdevelop"
./rlbuild-pvdevelop.sh
echo "build pvbrowser (you may select the exact build method within rlbuild-all.sh)"
cd pvbrowser
./rlbuild-pvbrowser6-with-cmake-without-www.sh
./rlbuild-pvbrowser6-with-cmake-with-www.sh
cd ..
#echo "skip build pvserver libraries for inetd and for multithreaded pvserver + build a very old pvserver"
./rlbuild-pvserver.sh
#echo "skip build rllib"
./rlbuild-rllib.sh
#echo "skip build pvserver language binding for lua"
./rlbuild-language-lua.sh
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
echo "The following versions of pvbrowser may have been created"
#ls -al pvbrowser/pvbrowser
ls -al ${PVB6ROOT}/pvbrowser/cmake-builddir-without-www/pvbrowser6
ls -al ${PVB6ROOT}/pvbrowser/cmake-builddir-with-www/pvbrowser6
cp     ${PVB6ROOT}/pvbrowser/cmake-builddir-with-www/pvbrowser6 ${PVB6ROOT}/pvbrowser/pvbrowser
echo " Copied pvbrowser6 with www to: "
ls -al ${PVB6ROOT}/pvbrowser/pvbrowser
