#!/bin/bash
###!/bin/csh -v

cd fake_qmake
if [ "$CPU" == "x86_64" ]; then
	echo compile fake_qmake for CPU x86_64
	g++ -c -m64 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o main.o main.cpp
	g++ -c -m64 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o fake_qmake.o fake_qmake.cpp
	g++ -c -m64 -pipe -O2 -Wall -W -Wno-implicit-fallthrough -I. -I../rllib/lib -o rlstring.o ../rllib/lib/rlstring.cpp
	g++ -c -m64 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlspreadsheet.o ../rllib/lib/rlspreadsheet.cpp
	g++ -c -m64 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlfileload.o ../rllib/lib/rlfileload.cpp
	g++ -c -m64 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlcutil.o ../rllib/lib/rlcutil.cpp
	g++ -m64 -Wl,-O1 -o fake_qmake main.o fake_qmake.o rlstring.o rlspreadsheet.o rlfileload.o rlcutil.o
	echo compile fake_qmake for CPU x86_64 done
elif [ "$CPU" == "x86_32" ]; then
	echo compile fake_qmake for CPU x86_32
	g++ -c -m32 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o main.o main.cpp
	g++ -c -m32 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o fake_qmake.o fake_qmake.cpp
	g++ -c -m32 -pipe -O2 -Wall -W -Wno-implicit-fallthrough -I. -I../rllib/lib -o rlstring.o ../rllib/lib/rlstring.cpp
	g++ -c -m32 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlspreadsheet.o ../rllib/lib/rlspreadsheet.cpp
	g++ -c -m32 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlfileload.o ../rllib/lib/rlfileload.cpp
	g++ -c -m32 -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlcutil.o ../rllib/lib/rlcutil.cpp
	g++ -m32 -Wl,-O1 -o fake_qmake main.o fake_qmake.o rlstring.o rlspreadsheet.o rlfileload.o rlcutil.o
	echo compile fake_qmake for CPU x86_32 done
else
	echo compile fake_qmake for unknown CPU 
	g++ -c -pipe -O2 -Wall -W  -I. -I../rllib/lib -o main.o main.cpp
	g++ -c -pipe -O2 -Wall -W  -I. -I../rllib/lib -o fake_qmake.o fake_qmake.cpp
	g++ -c -pipe -O2 -Wall -W -Wno-implicit-fallthrough -I. -I../rllib/lib -o rlstring.o ../rllib/lib/rlstring.cpp
	g++ -c -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlspreadsheet.o ../rllib/lib/rlspreadsheet.cpp
	g++ -c -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlfileload.o ../rllib/lib/rlfileload.cpp
	g++ -c -pipe -O2 -Wall -W  -I. -I../rllib/lib -o rlcutil.o ../rllib/lib/rlcutil.cpp
	g++ -Wl,-O1 -o fake_qmake main.o fake_qmake.o rlstring.o rlspreadsheet.o rlfileload.o rlcutil.o
	echo compile fake_qmake for unknown CPU done
fi
cd ..
