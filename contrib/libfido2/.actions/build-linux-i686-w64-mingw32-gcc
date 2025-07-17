#!/bin/sh -eux

# Copyright (c) 2022-2023 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

# XXX defining CC and cross-compiling confuses OpenSSL's build.
unset CC

sudo mkdir /fakeroot
sudo chmod 755 /fakeroot

cat << EOF > /tmp/mingw.cmake
SET(CMAKE_SYSTEM_NAME Windows)
SET(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
SET(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
SET(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
SET(CMAKE_FIND_ROOT_PATH /fakeroot)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

# Build and install libcbor.
git clone --depth=1 https://github.com/pjk/libcbor -b v0.10.1
cd libcbor
mkdir build
(cd build && cmake -DCMAKE_TOOLCHAIN_FILE=/tmp/mingw.cmake \
	-DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/fakeroot ..)
make -j"$(nproc)" -C build
sudo make -C build install
cd ..

# Build and install OpenSSL 1.1.1w.
git clone --depth=1 https://github.com/openssl/openssl -b OpenSSL_1_1_1w
cd openssl
./Configure mingw --prefix=/fakeroot --openssldir=/fakeroot/openssl \
	--cross-compile-prefix=i686-w64-mingw32-
make -j"$(nproc)"
sudo make install_sw
cd ..

# Build and install zlib.
git clone --depth=1 https://github.com/madler/zlib -b v1.3
cd zlib
make -fwin32/Makefile.gcc PREFIX=i686-w64-mingw32-
sudo make -fwin32/Makefile.gcc PREFIX=i686-w64-mingw32- DESTDIR=/fakeroot \
	INCLUDE_PATH=/include LIBRARY_PATH=/lib BINARY_PATH=/bin install
cd ..

# Build and install libfido2.
export PKG_CONFIG_PATH=/fakeroot/lib/pkgconfig
mkdir build
(cd build && cmake -DCMAKE_TOOLCHAIN_FILE=/tmp/mingw.cmake \
	-DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/fakeroot ..)
make -j"$(nproc)" -C build
sudo make -C build install
