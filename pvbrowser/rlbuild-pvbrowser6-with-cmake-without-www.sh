#!/bin/bash
echo "INFO from R.L.: script is currently setup for generating linux version"
echo "Test Linux Distribution /proc/version"
export   xysuse=$(grep -i suse /proc/version)
export xyfedora=$(grep -i fedora /proc/version)
export xyredhat=$(grep -i redhat /proc/version)
export xydebian=$(grep -i debian /proc/version)
export xyubuntu=$(grep -i ubuntu /proc/version)
export   xyelse=$(echo else)

if    [ 'x${xysuse}'   != 'x' ]; then
  echo "issuse"
elif  [ 'x${xyfedora}' != 'x' ]; then
  echo "isfedora"
elif  [ 'x${redhat}'   != 'x' ]; then
  echo "isredhat"
elif  [ 'x${debian}'   != 'x' ]; then
  echo "isdebian"
elif  [ 'x${ubuntu}'   != 'x' ]; then
  echo "isubuntu"
else
  echo "iselse"
fi

#export PVB6DIR=$PWD/../
export PVB6DIR=$PVB6ROOT
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
rm -f CMakeCache.txt
cp CMakeLists.txt-without-www CMakeLists.txt
echo cmake punkt without-www
#rm -rf cmake-builddir-without-www
#mkdir  cmake-builddir-without-www
cmake -S $PVB6DIR/pvbrowser -B cmake-builddir-without-www
cmake --build cmake-builddir-without-www
#
#echo "INFO from R.L.: script is currently setup for generating android version"
#export PVB6DIR=$PWD/../
#export CC=/home/lehrig/Android/Sdk/ndk/21.4.7075529/toolchains/llvm/prebuilt/linux-x86_64/bin/clang
#export CXX=/home/lehrig/Android/Sdk/ndk/21.4.7075529/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++
#rm -f CMakeCache.txt
#cp CMakeLists.txt-without-www CMakeLists.txt
#/home/lehrig/Android/Sdk/ndk/21.4.7075529/build/cmake -S $PWD -B cmake-builddir-without-www
#/home/lehrig/Android/Sdk/ndk/21.4.7075529/build/cmake --build cmake-builddir-without-www

