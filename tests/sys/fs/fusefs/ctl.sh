# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 ConnectWise
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

. $(atf_get_srcdir)/../../cam/ctl/ctl.subr

# Regression test for https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=283402
#
# Almost any fuse file system would work, but this tests uses fusefs-ext2
# because it's simple and its download is very small.
atf_test_case remove_lun_with_atime cleanup
remove_lun_with_atime_head()
{
	atf_set "descr" "Remove a fuse-backed CTL LUN when atime is enabled"
	atf_set "require.user" "root"
	atf_set "require.progs" "fuse-ext2 mkfs.ext2"
}
remove_lun_with_atime_body()
{
	MOUNTPOINT=$PWD/mnt
	atf_check mkdir $MOUNTPOINT
	atf_check truncate -s 1g ext2.img
	atf_check mkfs.ext2 -q ext2.img
	# Note: both default_permissions and atime must be enabled
	atf_check fuse-ext2 -o default_permissions,allow_other,rw+ ext2.img \
		$MOUNTPOINT

	atf_check truncate -s 1m $MOUNTPOINT/file
	create_block -o file=$MOUNTPOINT/file

	# Force fusefs to open the file, and dirty its atime
	atf_check dd if=/dev/$dev of=/dev/null count=1 status=none

	# Finally, remove the LUN.  Hopefully it won't panic.
	atf_check -o ignore ctladm remove -b block -l $LUN

	rm lun-create.txt	# So we don't try to remove the LUN twice
}
remove_lun_with_atime_cleanup()
{
	cleanup
	umount $PWD/mnt
}

atf_init_test_cases()
{
	atf_add_test_case remove_lun_with_atime
}
