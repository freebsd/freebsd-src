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

MAKEFS="makefs -t cd9660"

atf_test_case basic_cd9660 cleanup
basic_cd9660_body()
{
	create_test_inputs

	atf_check -e empty -o empty -s exit:0 \
	    $MAKEFS $TEST_IMAGE $TEST_INPUTS_DIR
	atf_check -e empty -o save:$TEST_MD_DEVICE_FILE -s exit:0 \
	    mdconfig -a -f $TEST_IMAGE
	atf_check -e empty -o empty -s exit:0 \
	    mount_cd9660 /dev/$(cat $TEST_MD_DEVICE_FILE) $TEST_MOUNT_DIR
	# diffutils doesn't feature --no-dereference until v3.3, so
	# $TEST_INPUTS_DIR/c will mismatch with $TEST_MOUNT_DIR/c (the
	# former will look like a directory; the latter like a file).
	#
	# XXX: the latter behavior seems suspect; seems like it should be a
	# symlink; need to verify this with mkisofs, etc
	atf_check -e empty -o empty -s exit:0 \
	    diff --exclude c -Naur $TEST_INPUTS_DIR $TEST_MOUNT_DIR
}
basic_cd9660_cleanup()
{
	test_md_device=$(cat $TEST_MD_DEVICE_FILE) || return

	umount -f /dev/$test_md_device
	mdconfig -d -u $test_md_device
}

atf_init_test_cases()
{

	atf_add_test_case basic_cd9660
}
