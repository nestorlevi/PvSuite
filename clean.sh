#!/bin/bash

rm -f pvdevelop/.qmake.stash
rm -f qwt/src/.qmake.stash
rm -f qwt/designer/.qmake.stash

rm tags
find . -name "*~"                -exec rm "{}" ";"
find . -name "*.a"               -exec rm "{}" ";"
find . -name "*.o"               -exec rm "{}" ";"
find . -name "lib*.o.*"          -exec rm "{}" ";"
find . -name "moc_*"             -exec rm "{}" ";"
find . -name "qrc_*"             -exec rm "{}" ";"
find . -name "ui_*"              -exec rm "{}" ";"
find . -name "installdox"        -exec rm "{}" ";"
find . -name "*.stash"           -exec rm "{}" ";"

if [ -d win-mingw ]; then
  cd win-mingw
  ./clean.sh
  cd ..
fi  
