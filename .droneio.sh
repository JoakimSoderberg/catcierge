#!/bin/bash

git submodule update --recursive --init
sudo apt-get update -qq
sudo apt-get install -y -qq valgrind libopencv-dev
mkdir build
cd build
cmake -DRPI=OFF ..
make
ctest --output-on-failure
