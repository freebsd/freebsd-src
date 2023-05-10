# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Netflix, Inc.
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
# $FreeBSD$

#
# These tests exercise a few basic cases for the sendfile() syscall:
# - successful operation.
# - sendfile() starts an async disk read but that async I/O fails.
# - sendfile() fails to read an indirect block and thus cannot
#   even start an async I/O.
#
# In all cases we request some read ahead in addition to
# the data to be sent to the socket.
#

MD_DEVS="md.devs"
MNT=mnt
FILE=$MNT/file
HELPER="$(atf_get_srcdir)/sendfile_helper"
BSIZE=4096

atf_test_case io_success cleanup
io_success_head()
{
	atf_set "descr" "sendfile where all disk I/O succeeds"
	atf_set "require.user" "root"
	atf_set "timeout" 15
}
io_success_body()
{
	if [ "$(atf_config_get qemu false)" = "true" ]; then
	    atf_skip "Sendfile(4) unimplemented. https://github.com/qemu-bsd-user/qemu-bsd-user/issues/25"
	fi

	md=$(alloc_md)
	common_body_setup $md

	atf_check $HELPER $FILE 0 0x10000 0x10000
}
io_success_cleanup()
{
	common_cleanup
}

atf_test_case io_fail_sync cleanup
io_fail_sync_head()
{
	atf_set "descr" "sendfile where we fail to start async I/O"
	atf_set "require.user" "root"
	atf_set "timeout" 15
}
io_fail_sync_body()
{
	if [ "$(atf_config_get qemu false)" = "true" ]; then
	    atf_skip "Sendfile(4) unimplemented. https://github.com/qemu-bsd-user/qemu-bsd-user/issues/25"
	fi

	md=$(alloc_md)
	common_body_setup $md

	atf_check gnop configure -r 100 -e 5 ${md}.nop
	atf_check -s exit:3 -e ignore $HELPER $FILE $((12 * $BSIZE)) $BSIZE 0x10000
}
io_fail_sync_cleanup()
{
	common_cleanup
}

atf_test_case io_fail_async cleanup
io_fail_async_head()
{
	atf_set "descr" "sendfile where an async I/O fails"
	atf_set "require.user" "root"
	atf_set "timeout" 15
}
io_fail_async_body()
{
	if [ "$(atf_config_get qemu false)" = "true" ]; then
	    atf_skip "Sendfile(4) unimplemented. https://github.com/qemu-bsd-user/qemu-bsd-user/issues/25"
	fi

	md=$(alloc_md)
	common_body_setup $md

	atf_check gnop configure -r 100 -e 5 ${md}.nop
	atf_check -s exit:2 -e ignore $HELPER $FILE 0 $BSIZE 0x10000
}
io_fail_async_cleanup()
{
	common_cleanup
}


atf_init_test_cases()
{
	atf_add_test_case io_success
	atf_add_test_case io_fail_sync
	atf_add_test_case io_fail_async
}

alloc_md()
{
	local md

	md=$(mdconfig -a -t swap -s 256M) || atf_fail "mdconfig -a failed"
	echo ${md} >> $MD_DEVS
	echo ${md}
}

common_body_setup()
{
	us=$1

	atf_check mkdir $MNT
	atf_check -o ignore -e ignore newfs -b $BSIZE -U -j /dev/${us}
	atf_check mount /dev/${us} $MNT
	atf_check -e ignore dd if=/dev/zero of=$FILE bs=1m count=1
	atf_check umount $MNT

	load_gnop
	atf_check gnop create /dev/${us}
	atf_check mount /dev/${us}.nop $MNT
	atf_check -o ignore ls -l $MNT/file
}

common_cleanup()
{
	umount -f $MNT
	if [ -f "$MD_DEVS" ]; then
		while read test_md; do
			gnop destroy -f ${test_md}.nop 2>/dev/null
			mdconfig -d -u $test_md 2>/dev/null
		done < $MD_DEVS
		rm $MD_DEVS
	fi

	true
}

load_gnop()
{
	if ! kldstat -q -m g_nop; then
		geom nop load || atf_skip "could not load module for geom nop"
	fi
}
