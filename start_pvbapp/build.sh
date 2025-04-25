#!/bin/bash
echo build start_pvbapp
g++ main.cpp ../rllib/lib/librllib.so.1 -I../rllib/lib -o start_pvbapp
