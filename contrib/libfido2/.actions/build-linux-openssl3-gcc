#!/bin/sh -eux

# Copyright (c) 2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

${CC} --version
FAKEROOT="$(mktemp -d)"

# Build and install OpenSSL 3.0.12.
git clone --branch openssl-3.0.12 \
	--depth=1 https://github.com/openssl/openssl
cd openssl
./Configure linux-x86_64 --prefix="${FAKEROOT}" \
	--openssldir="${FAKEROOT}/openssl" --libdir=lib
make install_sw
cd ..

# Build and install libfido2.
for T in Debug Release; do
	mkdir build-$T
	export PKG_CONFIG_PATH="${FAKEROOT}/lib/pkgconfig"
	(cd build-$T && cmake -DCMAKE_BUILD_TYPE=$T ..)
	make -j"$(nproc)" -C build-$T
	make -C build-$T regress
	sudo make -C build-$T install
done
