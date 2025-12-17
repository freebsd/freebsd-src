#!/bin/sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Klara, Inc.
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

# Create and mount a filesystem for use in our tests
unionfs_mkfs() {
	local name=$1
	local size=${2:-1}
	# Create mountpoint
	atf_check mkdir ${name}
	# Create filesystem image
	atf_check -e ignore dd if=/dev/zero of=${name}.img bs=1m count=${size}
	echo ${name} >>imgs
	# Create memory disk
	atf_check -o save:${name}.md mdconfig ${name}.img
	md=$(cat ${name}.md)
	echo ${md} >>mds
	# Format and mount filesystem
	atf_check -o ignore newfs /dev/${md}
	atf_check mount /dev/${md} ${name}
	echo ${name} >>mounts
}

# Mount a unionfs
unionfs_mount() {
	local upper=$1
	local lower=$2
	# Mount upper over lower
	atf_check mount -t unionfs ${upper} ${lower}
	echo ${lower} >>mounts
}

# Clean up after a test
unionfs_cleanup() {
	# Unmount filesystems
	if [ -f mounts ]; then
		tail -r mounts | while read mount; do
			umount ${mount} || true
		done
	fi
	# Destroy memory disks
	if [ -f mds ]; then
		tail -r mds | while read md; do
			mdconfig -d -u ${md} || true
		done
	fi
	# Delete filesystem images and mountpoints
	if [ -f imgs ]; then
		tail -r imgs | while read name; do
			rm -f ${name}.img || true
			rmdir ${name} || true
		done
	fi
}

atf_test_case unionfs_basic cleanup
unionfs_basic_head() {
	atf_set "descr" "Basic function test"
	atf_set "require.user" "root"
	atf_set "require.kmods" "unionfs"
}
unionfs_basic_body() {
	# Create upper and lower
	unionfs_mkfs upper
	unionfs_mkfs lower
	# Mount upper over lower
	unionfs_mount upper lower
	# Create object on unionfs
	atf_check touch upper/file
	atf_check mkdir upper/dir
	atf_check touch lower/dir/file
	# Verify that objects were created on upper
	atf_check test -f lower/file
	atf_check test -d lower/dir
	atf_check test -f upper/dir/file
}
unionfs_basic_cleanup() {
	unionfs_cleanup
}

atf_test_case unionfs_exec cleanup
unionfs_exec_head() {
	atf_set "descr" "Test executing programs"
	atf_set "require.user" "root"
	atf_set "require.kmods" "unionfs"
}
unionfs_exec_body() {
	# Create upper and copy a binary to it
	unionfs_mkfs upper
	atf_check cp -p /usr/bin/true upper/upper
	# Create lower and copy a binary to it
	unionfs_mkfs lower
	atf_check cp -p /usr/bin/true lower/lower
	# Mount upper over lower
	unionfs_mount upper lower
	# Execute both binaries
	atf_check lower/lower
	atf_check lower/upper
}
unionfs_exec_cleanup() {
	unionfs_cleanup
}

atf_test_case unionfs_rename cleanup
unionfs_rename_head() {
	atf_set "descr" "Test renaming objects on lower"
	atf_set "require.user" "root"
	atf_set "require.kmods" "unionfs"
}
unionfs_rename_body() {
	# Create upper and lower
	unionfs_mkfs upper
	unionfs_mkfs lower
	# Create objects on lower
	atf_check touch lower/file
	atf_check mkdir lower/dir
	atf_check ln -s dead lower/link
	# Mount upper over lower
	unionfs_mount upper lower
	# Rename objects
	atf_check mv lower/file lower/newfile
	atf_check mv lower/dir lower/newdir
	atf_check mv lower/link lower/newlink
	# Verify that old names no longer exist
	atf_check test ! -f lower/file
	atf_check test ! -d lower/dir
	atf_check test ! -L lower/link
	# Verify that new names exist on upper
	atf_check test -f upper/newfile
	atf_check test -d upper/newdir
	atf_check test -L upper/newlink
}
unionfs_rename_cleanup() {
	unionfs_cleanup
}

atf_init_test_cases() {
	atf_add_test_case unionfs_basic
	atf_add_test_case unionfs_exec
	atf_add_test_case unionfs_rename
}
