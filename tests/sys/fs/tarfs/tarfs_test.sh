#!/bin/sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Klara, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

mktar="$(dirname $(realpath "$0"))"/mktar
mnt="$(realpath ${TMPDIR:-/tmp})/mnt.$$"

# expected SHA256 checksum of file contained in test tarball
sum=4da2143234486307bb44eaa610375301781a577d1172f362b88bb4b1643dee62

atf_test_case tarfs_test
tarfs_basic_head() {
	atf_set "descr" "Basic function test"
	atf_set "require.user" "root"
}
tarfs_basic_body() {
	mkdir "${mnt}"
	"${mktar}" tarfs_test.tar.zst
	atf_check mount -rt tarfs tarfs_test.tar.zst "${mnt}"
	atf_check_equal "$(stat -f%d,%i "${mnt}"/sparse_file)" "$(stat -f%d,%i "${mnt}"/hard_link)"
	atf_check_equal "$(stat -f%d,%i "${mnt}"/sparse_file)" "$(stat -L -f%d,%i "${mnt}"/short_link)"
	atf_check_equal "$(stat -f%d,%i "${mnt}"/sparse_file)" "$(stat -L -f%d,%i "${mnt}"/long_link)"
	atf_check_equal "$(sha256 -q "${mnt}"/sparse_file)" ${sum}
}
tarfs_basic_cleanup() {
	umount "${mnt}"
}

atf_test_case tarfs_notdir
tarfs_notdir_head() {
	atf_set "descr" "Regression test for PR 269519"
	atf_set "require.user" "root"
}
tarfs_notdir_body() {
	mkdir "${mnt}"
	echo "hello" >d
	tar cf tarfs_notdir.tar d
	rm d
	mkdir -p d/s
	echo "world" >d/s/f
	tar rf tarfs_notdir.tar d/s/f
	atf_check -s not-exit:0 -e match:"Invalid" \
	    mount -rt tarfs tarfs_notdir.tar "${mnt}"
}
tarfs_notdir_cleanup() {
	umount "${mnt}"
}

atf_init_test_cases() {
	atf_add_test_case tarfs_basic
	atf_add_test_case tarfs_notdir
}
