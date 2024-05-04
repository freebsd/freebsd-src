#!/bin/sh -eux

# Copyright (c) 2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

${CC} --version
SCAN=scan-build${CC#clang}

# Check exports.
(cd src && ./diff_exports.sh)

# Build, analyze, and install libfido2.
for T in Debug Release; do
	mkdir build-$T
	(cd build-$T && ${SCAN} --use-cc="${CC}" cmake -DCMAKE_BUILD_TYPE=$T ..)
	${SCAN} --use-cc="${CC}" --status-bugs make -j"$(nproc)" -C build-$T
	make -C build-$T regress
	sudo make -C build-$T install
done
