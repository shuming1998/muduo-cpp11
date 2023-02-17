#!/bin/bash

set -e

if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build &&
    cmake .. &&
    make -j32

cd ..

if [ ! -d /usr/include/cmuduo ]; then
    mkdir /usr/include/cmuduo
fi

for header in `ls include/*.h`
do
    cp $header /usr/include/cmuduo
done

cp `pwd`/lib/libcmuduo.so /usr/lib

ldconfig
