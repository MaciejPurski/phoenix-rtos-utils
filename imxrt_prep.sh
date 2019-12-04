#!/bin/bash -e
#
#Phoenix-RTOS
#
#initial besmart image build script
#
#Copyright 2019 Phoenix Systems
#Author: Aleksander Kaminski
#
#This file is part of Phoenix-RTOS.
#
#%LICENSE%

pushd phoenix-rtos-kernel/src
make TARGET=armv7-imxrt DEBUG=1 clean all install-headers
popd

pushd libphoenix
make TARGET=armv7-imxrt clean all install
popd

pushd phoenix-rtos-devices
make DEBUG=1 TARGET=armv7-imxrt clean all
popd

pushd phoenix-rtos-filesystems
make TARGET=armv7-imxrt clean all
popd

pushd phoenix-rtos-coreutils
make TARGET=armv7-imxrt clean all
popd

cp -v _build/armv7-imxrt/prog/imxrt-multi .
cp -v _build/armv7-imxrt/prog/dummyfs .
cp -v _build/armv7-imxrt/prog/psh .

pushd phoenix-rtos-kernel/scripts
./mkimg-imxrt.sh ../phoenix-armv7-imxrt.elf "Ximxrt-multi Xdummyfs Xpsh" ../../imxrt_image.bin ../../_build/armv7-imxrt/prog/imxrt-multi ../../_build/armv7-imxrt/prog/dummyfs ../../_build/armv7-imxrt/prog/psh
popd
