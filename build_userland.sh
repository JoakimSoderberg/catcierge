#!/bin/bash

pushd rpi_userland

if [ "armv6l" = `arch` ]; then
	# Native compile on the Raspberry Pi
	mkdir -p build/raspberry/release
	pushd build/raspberry/release
	cmake -DCMAKE_BUILD_TYPE=Release ../../..
	make
else
	# Cross compile on a more capable machine
	mkdir -p build/arm-linux/release/
	pushd build/arm-linux/release/
	cmake -DCMAKE_TOOLCHAIN_FILE=../../../makefiles/cmake/toolchains/arm-linux-gnueabihf.cmake -DCMAKE_BUILD_TYPE=Release ../../..
	make -j 6
fi
popd

popd
