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

TEST_MD_DEVICE_FILE="md.output"
TEST_MOUNT_DIR="mnt"

create_test_dir()
{
	[ -z "$ATF_TMPDIR" ] || return 0

	export ATF_TMPDIR=$(pwd)

	TEST_MD_DEVICE_FILE="${ATF_TMPDIR}/${TEST_MD_DEVICE_FILE}"
	TEST_MOUNT_DIR="${ATF_TMPDIR}/${TEST_MOUNT_DIR}"

	# XXX: need to nest this because of how kyua creates $TMPDIR; otherwise
	# it will run into EPERM issues later
	TEST_INPUTS_DIR="${ATF_TMPDIR}/test/inputs"

	atf_check -e empty -s exit:0 mkdir -m 0777 -p $TEST_MOUNT_DIR
	atf_check -e empty -s exit:0 mkdir -m 0777 -p $TEST_INPUTS_DIR
	cd $TEST_INPUTS_DIR
}

create_test_inputs()
{
	create_test_dir

	atf_check -e empty -s exit:0 mkdir -m 0755 -p a/b/1
	atf_check -e empty -s exit:0 ln -s a/b c
	atf_check -e empty -s exit:0 touch d
	atf_check -e empty -s exit:0 ln d e
	atf_check -e empty -s exit:0 touch .f
	atf_check -e empty -s exit:0 mkdir .g
	atf_check -e empty -s exit:0 mkfifo h
	atf_check -e ignore -s exit:0 dd if=/dev/zero of=i count=1000 bs=1
	atf_check -e empty -s exit:0 touch klmn
	atf_check -e empty -s exit:0 touch opqr
	atf_check -e empty -s exit:0 touch stuv
	atf_check -e empty -s exit:0 install -m 0755 /dev/null wxyz
	atf_check -e empty -s exit:0 touch 0b00000001
	atf_check -e empty -s exit:0 touch 0b00000010
	atf_check -e empty -s exit:0 touch 0b00000011
	atf_check -e empty -s exit:0 touch 0b00000100
	atf_check -e empty -s exit:0 touch 0b00000101
	atf_check -e empty -s exit:0 touch 0b00000110
	atf_check -e empty -s exit:0 touch 0b00000111
	atf_check -e empty -s exit:0 touch 0b00001000
	atf_check -e empty -s exit:0 touch 0b00001001
	atf_check -e empty -s exit:0 touch 0b00001010
	atf_check -e empty -s exit:0 touch 0b00001011
	atf_check -e empty -s exit:0 touch 0b00001100
	atf_check -e empty -s exit:0 touch 0b00001101
	atf_check -e empty -s exit:0 touch 0b00001110
}
