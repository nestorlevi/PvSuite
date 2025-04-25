#!/bin/bash
echo "info from qmake.sh"
if   [ "$USE_FAKE_QMAKE" == "yes" ]; then
	echo "run_fake_qmake"
	$fakeqmake -fake $1 $2 $3 $4 $5 $6 $7 $8
elif [ "$USE_GPP_QMAKE" == "yes" ]; then
	echo "run /usr/bin/qmake6 -spec linux-g++   $1 $2 $3 $4 $5 $6 $7 $8"
	/usr/bin/qmake6 -spec linux-g++   $1 $2 $3 $4 $5 $6 $7 $8
elif [ "$USE_CLANG_QMAKE" == "yes" ]; then
	echo "run /usr/bin/qmake6 -spec linux-clang $1 $2 $3 $4 $5 $6 $7 $8"
	/usr/bin/qmake6 -spec linux-clang $1 $2 $3 $4 $5 $6 $7 $8
else	
	#echo "run_fake_qmake"
	#$fakeqmake -fake $1 $2 $3 $4 $5 $6 $7 $8
	echo "run_qmake6"
	/usr/bin/qmake6 $1 $2 $3 $4 $5 $6 $7 $8
fi
