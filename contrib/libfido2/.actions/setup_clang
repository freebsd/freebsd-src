#!/bin/sh -eu

# Copyright (c) 2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

CC="$1"
APT="http://apt.llvm.org"
CODENAME="$(lsb_release -cs)"
VERSION="${CC#*-}"
apt-get install -q -y software-properties-common
apt-key add ./.actions/llvm.gpg
add-apt-repository \
    "deb ${APT}/${CODENAME}/ llvm-toolchain-${CODENAME}-${VERSION} main"
apt-get update -q
apt-get install -q -y "${CC}" "clang-tools-${VERSION}"
