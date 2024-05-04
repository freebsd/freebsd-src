#!/bin/sh -eux

# Copyright (c) 2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

${CC} --version

# Build and install libfido2.
for T in Debug Release; do
	mkdir build-$T
	(cd build-$T && cmake -DCMAKE_BUILD_TYPE=$T ..)
	make -j"$(nproc)" -C build-$T
	make -C build-$T regress
	sudo make -C build-$T install
done

# Check udev/fidodevs.
[ -x "$(which update-alternatives)" ] && {
	sudo update-alternatives --set awk "$(which original-awk)"
}
udev/check.sh udev/fidodevs
