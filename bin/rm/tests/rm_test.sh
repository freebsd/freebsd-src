#
# Copyright 2018 Yuri Pankov
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
#

atf_test_case unlink_dash_filename
unlink_dash_filename_head()
{
	atf_set "descr" "unlink correctly handles -filename"
}
unlink_dash_filename_body()
{
	touch -- foo bar -foo -bar
	atf_check -s exit:0 unlink foo
	atf_check -s exit:0 unlink -- bar
	atf_check -s exit:0 unlink -foo
	atf_check -s exit:0 unlink -- -bar
}

atf_test_case f_flag_msdosfs cleanup
f_flag_msdosfs_head()
{
	atf_set "descr" "Verify that -f nonexistent* does not print an error on MS-DOS file systems"
	atf_set "require.user" root
}
f_flag_msdosfs_body()
{
	# Create an MS-DOS FS mount
	md=$(mdconfig -a -t swap -s 5m)
	mkdir mnt
	newfs_msdos -h 1 -u 63 "$md"
	mount_msdosfs /dev/"${md}" mnt

	atf_check -s exit:0 rm -f mnt/foo*
}
f_flag_msdosfs_cleanup()
{
	umount mnt
	mdconfig -d -u /dev/"${md}"
}

atf_init_test_cases()
{
	atf_add_test_case unlink_dash_filename
	atf_add_test_case f_flag_msdosfs
}
