#!/bin/bash
export PATH=$PWD/prebuilts/clang/host/linux-x86/clang-r383902/bin/:$PATH
export PATH=$PWD/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/:$PATH
cd kernel-4.19
rm -rf out/

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu- LD=ld.lld LD_LIBRARY_PATH=$PWD/prebuilts/clang/host/linux-x86/clang-r383902/lib64: NM=llvm-nm OBJCOPY=llvm-objcopy CC=clang ROOTDIR=$PWD O=out P98928AA1_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu- LD=ld.lld LD_LIBRARY_PATH=$PWD/prebuilts/clang/host/linux-x86/clang-r383902/lib64: NM=llvm-nm OBJCOPY=llvm-objcopy CC=clang ROOTDIR=$PWD O=out O=out -j32
