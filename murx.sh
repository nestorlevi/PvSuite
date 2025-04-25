#!/bin/bash

echo   "try to find fake_qmake"
export fakeqmake=$(find $BROOT -name fake_qmake  | grep "fake_qmake/fake_qmake")
###echo   found: $fakeqnake
$fakeqmake -fake $1 $2 $3 $4 $5 $6 $7 $8

# echo "run /usr/bin/qmake6 -spec linux-g++   $1 $2 $3 $4 $5 $6 $7 $8"
# /usr/bin/qmake6 -spec linux-g++   $1 $2 $3 $4 $5 $6 $7 $8

# echo "run /usr/bin/qmake6 -spec linux-clang $1 $2 $3 $4 $5 $6 $7 $8"
# /usr/bin/qmake6 -spec linux-clang $1 $2 $3 $4 $5 $6 $7 $8

