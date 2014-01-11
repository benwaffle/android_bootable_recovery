#!/usr/bin/env bash
sh $ANDROID_BUILD_TOP/bootable/recovery/safestrap/devices/samsung/h-common/build-safestrap.sh
cd $ANDROID_BUILD_TOP/bootable/recovery/safestrap/devices/samsung
cp -fr hltexx/ss.config $OUT/install-files/etc/safestrap/ss.config
cp -fr hltexx/ss.config $OUT/APP/ss.config
cp -fr hltexx/ss.config $OUT/recovery/root/ss.config
cp -fr hltexx/build-fs.sh $OUT/recovery/root/sbin/

