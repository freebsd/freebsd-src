# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Axcient
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

# Things that aren't tested due to lack of kernel support:
# * Creating camsim ports
# * Creating tpc ports
# * Creating camtgt ports
# * Creating umass ports

# TODO
# * Creating iscsi ports
# * Creating nvmf ports
# * Creating ha ports
# * Creating fc ports

skip_if_ctld() {
	if service ctld onestatus > /dev/null; then
		# If ctld is running on this server, let's not interfere.
		atf_skip "Cannot run this test while ctld is running"
	fi
}

cleanup() {
	driver=$1

	if [ -e port-create.txt ]; then
		portnum=`awk '/port:/ {print $2}' port-create.txt`
		ctladm port -r -d $driver -p $portnum
	fi
}

atf_test_case create_ioctl cleanup
create_ioctl_head()
{
	atf_set "descr" "ctladm can create a new ioctl port"
	atf_set "require.user" "root"
}
create_ioctl_body()
{
	skip_if_ctld

	atf_check -o save:port-create.txt ctladm port -c -d "ioctl"
	atf_check egrep -q "Port created successfully" port-create.txt
	atf_check egrep -q "frontend: *ioctl" port-create.txt
	atf_check egrep -q "port: *[0-9]+" port-create.txt
	portnum=`awk '/port:/ {print $2}' port-create.txt`
	atf_check -o save:portlist.txt ctladm portlist -qf ioctl
	atf_check egrep -q "$portnum *YES *ioctl *ioctl" portlist.txt
}
create_ioctl_cleanup()
{
	cleanup ioctl
}

atf_test_case create_ioctl_options cleanup
create_ioctl_options_head()
{
	atf_set "descr" "ctladm can set options when creating a new ioctl port"
	atf_set "require.user" "root"
}
create_ioctl_options_body()
{
	skip_if_ctld

	atf_check -o save:port-create.txt ctladm port -c -d "ioctl" -O pp=101 -O vp=102
	atf_check egrep -q "Port created successfully" port-create.txt
	atf_check egrep -q "frontend: *ioctl" port-create.txt
	atf_check egrep -q "port: *[0-9]+" port-create.txt
	portnum=`awk '/port:/ {print $2}' port-create.txt`
	atf_check -o save:portlist.txt ctladm portlist -qf ioctl
	if ! egrep -q '101[[:space:]]+102' portlist.txt; then
		ctladm portlist
		atf_fail "Did not create the port with the specified options"
	fi
}
create_ioctl_options_cleanup()
{
	cleanup ioctl
}


atf_test_case disable_ioctl cleanup
disable_ioctl_head()
{
	atf_set "descr" "ctladm can disable an ioctl port"
	atf_set "require.user" "root"
}
disable_ioctl_body()
{
	skip_if_ctld

	atf_check -o save:port-create.txt ctladm port -c -d "ioctl"
	portnum=`awk '/port:/ {print $2}' port-create.txt`
	atf_check -o save:portlist.txt ctladm portlist -qf ioctl
	atf_check -o ignore ctladm port -o off -p $portnum
	atf_check -o match:"^$portnum *NO" ctladm portlist -qf ioctl
}
disable_ioctl_cleanup()
{
	cleanup ioctl
}

atf_test_case enable_ioctl cleanup
enable_ioctl_head()
{
	atf_set "descr" "ctladm can enable an ioctl port"
	atf_set "require.user" "root"
}
enable_ioctl_body()
{
	skip_if_ctld

	atf_check -o save:port-create.txt ctladm port -c -d "ioctl"
	portnum=`awk '/port:/ {print $2}' port-create.txt`
	atf_check -o save:portlist.txt ctladm portlist -qf ioctl
	atf_check -o ignore ctladm port -o off -p $portnum
	atf_check -o ignore ctladm port -o on -p $portnum
	atf_check -o match:"^$portnum *YES" ctladm portlist -qf ioctl
}
enable_ioctl_cleanup()
{
	cleanup ioctl
}

atf_test_case remove_ioctl
remove_ioctl_head()
{
	atf_set "descr" "ctladm can remove an ioctl port"
	atf_set "require.user" "root"
}
remove_ioctl_body()
{
	skip_if_ctld

	# Specify exact pp and vp to make the post-removal portlist check
	# unambiguous
	atf_check -o save:port-create.txt ctladm port -c -d "ioctl" -O pp=10001 -O vp=10002
	portnum=`awk '/port:/ {print $2}' port-create.txt`
	atf_check -o save:portlist.txt ctladm portlist -qf ioctl
	atf_check -o inline:"Port destroyed successfully\n" ctladm port -r -d ioctl -p $portnum
	# Check that the port was removed.  A new port may have been added with
	# the same ID, so match against the pp and vp numbers, too.
	if ctladm portlist -qf ioctl | egrep -q "^${portnum} .*10001 *10002"; then
		ctladm portlist -qf ioctl
		atf_fail "port was not removed"
	fi
}

atf_init_test_cases()
{
	atf_add_test_case create_ioctl
	atf_add_test_case create_ioctl_options
	atf_add_test_case disable_ioctl
	atf_add_test_case enable_ioctl
	atf_add_test_case remove_ioctl
}
