#!/bin/sh

TARGET=$BUILD_DIR/../target

rm -rf $TARGET/etc/init.d/S01syslogd
rm -rf $TARGET/etc/init.d/S02klogd
rm -rf $TARGET/etc/init.d/S02sysctl
rm -rf $TARGET/etc/init.d/S20urandom
rm -rf $TARGET/etc/init.d/S40network

mkdir -p $TARGET/lib/modules/
cp $BUILD_DIR/../../package/210/rtl8821cu/8821cu.ko $TARGET/lib/modules/ -a
cp $BUILD_DIR/../../package/210/rtl8821cu/rtl8821cu.sh $TARGET/usr/bin/ -a
cp $BUILD_DIR/../../package/210/gadgetraw/f_gadgetraw.ko $TARGET/lib/modules/ -a
cp $BUILD_DIR/../../package/210/gadgetraw/gr.sh $TARGET/usr/bin/ -a
