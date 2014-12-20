#!/bin/bash

git submodule update --recursive --init
sudo apt-get update -qq
sudo apt-get install -y -qq libopencv-dev
mkdir build
cd build
cmake -DRPI=OFF ..
cmake --build .
ctest --output-on-failure
