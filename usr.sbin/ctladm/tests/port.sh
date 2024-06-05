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

	if [ -e after-ports ]; then
		diff before-ports after-ports | awk "/$driver/ {print \$2}" | xargs -n1 ctladm port -r -d ioctl -p
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

	atf_check -o save:before-ports ctladm portlist -qf ioctl
	atf_check ctladm port -c -d "ioctl"
	atf_check -o save:after-ports ctladm portlist -qf ioctl
	if test `wc -l before-ports | cut -w -f2` -ge `wc -l after-ports | cut -w -f2`; then
		atf_fail "Did not create a new ioctl port"
	fi
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

	atf_check -o save:before-ports ctladm portlist -qf ioctl
	atf_check ctladm port -c -d "ioctl" -O pp=101 -O vp=102
	atf_check -o save:after-ports ctladm portlist -qf ioctl
	if test `wc -l before-ports | cut -w -f2` -ge `wc -l after-ports | cut -w -f2`; then
		atf_fail "Did not create a new ioctl port"
	fi
	if ! egrep -q '101[[:space:]]+102' after-ports; then
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

	atf_check -o save:before-ports ctladm portlist -qf ioctl
	atf_check ctladm port -c -d "ioctl"
	atf_check -o save:after-ports ctladm portlist -qf ioctl
	if test `wc -l before-ports | cut -w -f2` -ge `wc -l after-ports | cut -w -f2`; then
		atf_fail "Did not create a new ioctl port"
	fi
	portnum=`diff before-ports after-ports | awk '/ioctl/ {print $2}'`;
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

	atf_check -o save:before-ports ctladm portlist -qf ioctl
	atf_check ctladm port -c -d "ioctl"
	atf_check -o save:after-ports ctladm portlist -qf ioctl
	if test `wc -l before-ports | cut -w -f2` -ge `wc -l after-ports | cut -w -f2`; then
		atf_fail "Did not create a new ioctl port"
	fi
	portnum=`diff before-ports after-ports | awk '/ioctl/ {print $2}'`;
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

	atf_check -o save:before-ports ctladm portlist -qf ioctl
	atf_check ctladm port -c -d "ioctl"
	atf_check -o save:after-ports ctladm portlist -qf ioctl
	if test `wc -l before-ports | cut -w -f2` -ge `wc -l after-ports | cut -w -f2`; then
		atf_fail "Did not create a new ioctl port"
	fi
	portnum=`diff before-ports after-ports | awk '/ioctl/ {print $2}'`;
	atf_check ctladm port -r -d ioctl -p $portnum
}

atf_init_test_cases()
{
	atf_add_test_case create_ioctl
	atf_add_test_case create_ioctl_options
	atf_add_test_case disable_ioctl
	atf_add_test_case enable_ioctl
	atf_add_test_case remove_ioctl
}
