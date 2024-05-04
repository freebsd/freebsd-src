#!/bin/sh -eux

# Copyright (c) 2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

export PKG_CONFIG_PATH="$(brew --prefix openssl@3.0)/lib/pkgconfig"
SCAN="$(brew --prefix llvm)/bin/scan-build"

# Build, analyze, and install libfido2.
for T in Debug Release; do
	mkdir build-$T
	(cd build-$T && ${SCAN} cmake -DCMAKE_BUILD_TYPE=$T ..)
	${SCAN} --status-bugs make -j"$(sysctl -n hw.ncpu)" -C build-$T
	make -C build-$T man_symlink_html
	make -C build-$T regress
	sudo make -C build-$T install
done
