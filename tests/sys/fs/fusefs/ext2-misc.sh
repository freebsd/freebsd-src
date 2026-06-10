# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 ConnectWise
# All rights reserved.
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
# THIS DOCUMENTATION IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Regression test for https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=295957
#
# Almost any fuse file system would work, but this tests uses fusefs-ext2
# because it's simple and its download is very small.
atf_test_case execute cleanup
execute_head()
{
	atf_set "descr" "Execute a file mounted on a fusefs file system"
	atf_set "require.user" "root"
	atf_set "require.progs" "fuse-ext2 mkfs.ext2"
	atf_set "require.kmods" "fusefs"
}
execute_body()
{
	atf_check mkdir mnt
	atf_check truncate -s 64m ext2.img
	atf_check -o ignore -e ignore mkfs.ext2 ext2.img
	atf_check fuse-ext2 -o rw+ ext2.img mnt
	atf_check cp /usr/bin/true mnt
	atf_check su -m nobody -c mnt/true
}
execute_cleanup()
{
	umount $PWD/mnt || true
}

atf_init_test_cases()
{
	atf_add_test_case execute
}
