#!/bin/sh -eux

# Copyright (c) 2020-2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

LIBCBOR_URL="https://github.com/pjk/libcbor"
LIBCBOR_TAG="v0.10.2"
LIBCBOR_ASAN="address alignment bounds"
LIBCBOR_MSAN="memory"
OPENSSL_URL="https://github.com/openssl/openssl"
OPENSSL_TAG="openssl-3.0.12"
ZLIB_URL="https://github.com/madler/zlib"
ZLIB_TAG="v1.3"
ZLIB_ASAN="address alignment bounds undefined"
ZLIB_MSAN="memory"
FIDO2_ASAN="address bounds fuzzer-no-link implicit-conversion leak"
FIDO2_ASAN="${FIDO2_ASAN} pointer-compare pointer-subtract undefined"
FIDO2_MSAN="fuzzer-no-link memory"
COMMON_CFLAGS="-g2 -fno-omit-frame-pointer"
COMMON_CFLAGS="${COMMON_CFLAGS} -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION"
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1:strict_string_checks=1"
ASAN_OPTIONS="${UBSAN_OPTIONS}:detect_invalid_pointer_pairs=2:detect_leaks=1"
MSAN_OPTIONS="${UBSAN_OPTIONS}"

case "$1" in
asan)
	LIBCBOR_CFLAGS="-fsanitize=$(echo "${LIBCBOR_ASAN}" | tr ' ' ',')"
	ZLIB_CFLAGS="-fsanitize=$(echo "${ZLIB_ASAN}" | tr ' ' ',')"
	FIDO2_CFLAGS="-fsanitize=$(echo "${FIDO2_ASAN}" | tr ' ' ',')"
	FIDO2_CFLAGS="${FIDO2_CFLAGS} -fsanitize-address-use-after-scope"
	;;
msan)
	LIBCBOR_CFLAGS="-fsanitize=$(echo "${LIBCBOR_MSAN}" | tr ' ' ',')"
	ZLIB_CFLAGS="-fsanitize=$(echo "${ZLIB_MSAN}" | tr ' ' ',')"
	FIDO2_CFLAGS="-fsanitize=$(echo "${FIDO2_MSAN}" | tr ' ' ',')"
	FIDO2_CFLAGS="${FIDO2_CFLAGS} -fsanitize-memory-track-origins"
	;;
*)
	echo "unknown sanitiser \"$1\"" 1>&2 && exit 1
esac

${CC} --version
WORKDIR="${WORKDIR:-$(pwd)}"
FAKEROOT="${FAKEROOT:-$(mktemp -d)}"
cd "${FAKEROOT}"

# libcbor
git clone --depth=1 "${LIBCBOR_URL}" -b "${LIBCBOR_TAG}"
cd libcbor
patch -p0 -s < "${WORKDIR}/fuzz/README"
mkdir build
(cd build && cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS_DEBUG="${LIBCBOR_CFLAGS} ${COMMON_CFLAGS}" \
    -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_INSTALL_PREFIX="${FAKEROOT}" \
    -DSANITIZE=OFF ..)
make VERBOSE=1 -j"$(nproc)" -C build all install
cd -

# openssl
git clone --depth=1 "${OPENSSL_URL}" -b "${OPENSSL_TAG}"
cd openssl
./Configure linux-x86_64-clang "enable-$1" --prefix="${FAKEROOT}" \
    --openssldir="${FAKEROOT}/openssl" --libdir=lib
make install_sw
cd -

# zlib
git clone --depth=1 "${ZLIB_URL}" -b "${ZLIB_TAG}"
cd zlib
CFLAGS="${ZLIB_CFLAGS}" LDFLAGS="${ZLIB_CFLAGS}" ./configure \
    --prefix="${FAKEROOT}"
make install
cd -

# libfido2
mkdir build
export PKG_CONFIG_PATH="${FAKEROOT}/lib/pkgconfig"
(cd build && cmake -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS_DEBUG="${FIDO2_CFLAGS} ${COMMON_CFLAGS}" -DFUZZ=ON \
    -DFUZZ_LDFLAGS="-fsanitize=fuzzer" "${WORKDIR}")
make -j"$(nproc)" -C build

# fuzz
mkdir corpus
curl -s https://storage.googleapis.com/yubico-libfido2/corpus.tgz |
    tar -C corpus -zxf -
export UBSAN_OPTIONS ASAN_OPTIONS MSAN_OPTIONS
for f in assert bio cred credman hid largeblob mgmt netlink pcsc; do
	build/fuzz/fuzz_${f} -use_value_profile=1 -reload=30 -print_pcs=1 \
	    -print_funcs=30 -timeout=10 -runs=1 corpus/fuzz_${f}
done
