#!/bin/bash
git submodule update --recursive --init
#sudo apt-get update -qq
sudo apt-get install -y -qq valgrind libopencv-dev libzmq-dev libczmq-dev
mkdir build
cd build
set -e
cmake -DWITH_ZMQ=ON ..
make
ctest --output-on-failure
