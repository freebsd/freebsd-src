#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 The FreeBSD Foundation
#
# This software was developed by Klara, Inc.
# under sponsorship from the FreeBSD Foundation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the distribution.
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

MAKEFS="makefs -t msdos"
MOUNT="mount_msdosfs"
. "$(dirname "$0")/makefs_tests_common.sh"

common_cleanup()
{
	if ! test_md_device=$(cat $TEST_MD_DEVICE_FILE); then
		echo "$TEST_MD_DEVICE_FILE could not be opened; has an md(4) device been attached?"
		return
	fi

	umount -f /dev/$test_md_device || :
	mdconfig -d -u $test_md_device || :
}

check_msdosfs_support()
{
	kldstat -m msdosfs || \
		atf_skip "Requires msdosfs filesystem support to be present in the kernel"
}

atf_test_case T_flag_dir cleanup
T_flag_dir_body()
{
	atf_expect_fail \
	    "The msdos backend saves the wrong timestamp value" \
	    "(possibly due to the 2s resolution for FAT timestamp)"
	timestamp=1742574909
	check_msdosfs_support

	create_test_dirs
	mkdir -p $TEST_INPUTS_DIR/dir1
	atf_check -e empty -o not-empty -s exit:0 \
	    $MAKEFS -T $timestamp -s 1m $TEST_IMAGE $TEST_INPUTS_DIR

	mount_image
	eval $(stat -s  $TEST_MOUNT_DIR/dir1)
	atf_check_equal $st_atime $timestamp
	atf_check_equal $st_mtime $timestamp
	atf_check_equal $st_ctime $timestamp
}

T_flag_dir_cleanup()
{
	common_cleanup
}

atf_test_case T_flag_F_flag cleanup
T_flag_F_flag_body()
{
	atf_expect_fail "-F doesn't take precedence over -T"
	timestamp_F=1742574909
	timestamp_T=1742574910
	create_test_dirs
	mkdir -p $TEST_INPUTS_DIR/dir1

	atf_check -e empty -o save:$TEST_SPEC_FILE -s exit:0 \
	    mtree -c -k "type,time" -p $TEST_INPUTS_DIR
	change_mtree_timestamp $TEST_SPEC_FILE $timestamp_F
	atf_check -e empty -o not-empty -s exit:0 \
	    $MAKEFS -F $TEST_SPEC_FILE -T $timestamp_T -s 1m $TEST_IMAGE $TEST_INPUTS_DIR

	mount_image
	eval $(stat -s  $TEST_MOUNT_DIR/dir1)
	atf_check_equal $st_atime $timestamp_F
	atf_check_equal $st_mtime $timestamp_F
	atf_check_equal $st_ctime $timestamp_F
}

T_flag_F_flag_cleanup()
{
	common_cleanup
}

atf_test_case T_flag_mtree cleanup
T_flag_mtree_body()
{
	timestamp=1742574908 # Even value, timestamp precision is 2s.
	check_msdosfs_support

	create_test_dirs
	mkdir -p $TEST_INPUTS_DIR/dir1
	atf_check -e empty -o save:$TEST_SPEC_FILE -s exit:0 \
	    mtree -c -k "type" -p $TEST_INPUTS_DIR
	atf_check -e empty -o not-empty -s exit:0 \
	    $MAKEFS -T $timestamp -s 1m  $TEST_IMAGE $TEST_SPEC_FILE

	mount_image
	eval $(stat -s  $TEST_MOUNT_DIR/dir1)
        # FAT directory entries don't have an access time, just a date.
	#atf_check_equal $st_atime $timestamp
	atf_check_equal $st_mtime $timestamp
	atf_check_equal $st_ctime $timestamp
}

T_flag_mtree_cleanup()
{
	common_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case T_flag_dir
	atf_add_test_case T_flag_F_flag
	atf_add_test_case T_flag_mtree
}
