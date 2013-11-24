#!/bin/sh
make say_defconfig
make -j8
cp -rf arch/arm/boot/Image /tftpboot/zImage.ezs5pv210-s100-android
