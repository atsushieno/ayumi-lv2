#!/bin/sh

if ! [ -d $LV2_INSTALL_PATH ] ; then
LV2_INSTALL_PATH=`pwd`/dist
fi
echo "target directory: $LV2_INSTALL_PATH"

clang -g ayumi-lv2.c ayumi.c -fPIC -lm -shared -o ayumi-lv2.so

mkdir -p $LV2_INSTALL_PATH/ayumi-lv2
cp ayumi-lv2.so ayumi-lv2.ttl manifest.ttl $LV2_INSTALL_PATH/ayumi-lv2
