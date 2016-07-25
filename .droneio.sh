#!/bin/bash
set -e

git submodule update --recursive --init
sudo apt-get update -qq
sudo apt-get install -y -qq valgrind libopencv-dev libzmq-dev libczmq3
mkdir build
cd build
cmake ..
make
ctest --output-on-failure
