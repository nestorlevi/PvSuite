#!/bin/bash
echo "create pvbrowser with www using cmake and clang for linux systems"
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
cp CMakeLists.txt-with-www CMakeLists.txt
echo cmake punkt2 with-www
#rm -rf cmake-builddir-with-www
#mkdir  cmake-builddir-with-www
cmake -S $PVB6DIR/pvbrowser -B cmake-builddir-with-www
cmake --build cmake-builddir-with-www
ls -al cmake-builddir-with-www/pvbrowser6
