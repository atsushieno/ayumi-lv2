#!/bin/sh

PREFIX=`pwd`/dist

clang -g ayumi-lv2.c ayumi.c -fPIC -lm -shared -o ayumi-lv2.so

mkdir -p $PREFIX/ayumi-lv2
cp ayumi-lv2.so ayumi-lv2.ttl manifest.ttl $PREFIX/ayumi-lv2
