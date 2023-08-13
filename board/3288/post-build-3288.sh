#!/bin/sh

TARGET=$BUILD_DIR/../target

rm -rf $TARGET/etc/init.d/S01syslogd
rm -rf $TARGET/etc/init.d/S02klogd
rm -rf $TARGET/etc/init.d/S02sysctl
rm -rf $TARGET/etc/init.d/S20urandom
rm -rf $TARGET/etc/init.d/S40network
