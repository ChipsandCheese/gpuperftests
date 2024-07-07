#!/bin/sh

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DNFD_INSTALL=OFF -DNFD_BUILD_TESTS=OFF ..
cmake --build .