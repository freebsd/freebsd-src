#
# Copyright 2015 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

. "$(dirname "$0")/makefs_tests_common.sh"

MAKEFS="makefs -t ffs"
TEST_IMAGE="test.img"

atf_test_case basic_ffs cleanup
basic_ffs_body()
{
	create_test_inputs

	atf_check -e empty -o not-empty -s exit:0 \
	    $MAKEFS -M 1m $TEST_IMAGE $TEST_INPUTS_DIR
	atf_check -e empty -o save:$TEST_MD_DEVICE_FILE -s exit:0 \
	    mdconfig -a -f $TEST_IMAGE
	atf_check -e save:$ATF_TMPDIR/tunefs.output -o empty -s exit:0 \
	    tunefs -p /dev/$(cat $TEST_MD_DEVICE_FILE)
	atf_check -e empty -o empty -s exit:0 \
	    mount /dev/$(cat $TEST_MD_DEVICE_FILE) $TEST_MOUNT_DIR
	atf_check -e empty -o not-empty -s exit:0 ls $TEST_MOUNT_DIR
}
basic_ffs_cleanup()
{
	ls -a

	test_md_device=$(cat $TEST_MD_DEVICE_FILE) || return

	umount -f /dev/$test_md_device
	mdconfig -d -u $test_md_device
}

atf_init_test_cases()
{

	atf_add_test_case basic_ffs
}
