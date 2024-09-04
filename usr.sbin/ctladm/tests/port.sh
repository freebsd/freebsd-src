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
# * Creating nvmf ports
# * Creating ha ports
# * Creating fc ports

# The PGTAG can be any 16-bit number.  The only constraint is that each
# PGTAG,TARGET pair must be globally unique.
PGTAG=30257

load_cfiscsi() {
	if ! kldstat -q -m cfiscsi; then
		kldload cfiscsi || atf_skip "could not load cfscsi kernel mod"
	fi
}

skip_if_ctld() {
	if service ctld onestatus > /dev/null; then
		# If ctld is running on this server, let's not interfere.
		atf_skip "Cannot run this test while ctld is running"
	fi
}

cleanup() {
	driver=$1

	if [ -e port-create.txt ]; then
		case "$driver" in
		"ioctl")
			PORTNUM=`awk '/port:/ {print $2}' port-create.txt`
			ctladm port -r -d $driver -p $PORTNUM
			;;
		"iscsi")
			TARGET=`awk '/target:/ {print $2}' port-create.txt`
			ctladm port -r -d $driver -O cfiscsi_portal_group_tag=$PGTAG -O cfiscsi_target=$TARGET
			;;
		esac
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

atf_test_case remove_ioctl_without_required_args cleanup
remove_ioctl_without_required_args_head()
{
	atf_set "descr" "ctladm will gracefully fail to remove an ioctl target if required arguments are missing"
	atf_set "require.user" "root"
}
remove_ioctl_without_required_args_body()
{
	skip_if_ctld

	atf_check -o save:port-create.txt ctladm port -c -d "ioctl"
	atf_check egrep -q "Port created successfully" port-create.txt
	atf_check -s exit:1 -e match:"Missing required argument: port_id" ctladm port -r -d "ioctl"
}
remove_ioctl_without_required_args_cleanup()
{
	cleanup ioctl
}

atf_test_case create_iscsi cleanup
create_iscsi_head()
{
	atf_set "descr" "ctladm can create a new iscsi port"
	atf_set "require.user" "root"
}
create_iscsi_body()
{
	skip_if_ctld
	load_cfiscsi

	TARGET=iqn.2018-10.myhost.create_iscsi
	atf_check -o save:port-create.txt ctladm port -c -d "iscsi" -O cfiscsi_portal_group_tag=$PGTAG -O cfiscsi_target="$TARGET"
	echo "target: $TARGET" >> port-create.txt
	atf_check egrep -q "Port created successfully" port-create.txt
	atf_check egrep -q "frontend: *iscsi" port-create.txt
	atf_check egrep -q "port: *[0-9]+" port-create.txt
	atf_check -o save:portlist.txt ctladm portlist -qf iscsi
	# Unlike the ioctl driver, the iscsi driver creates ports in a disabled
	# state, so the port's lunmap may be set before enabling it.
	atf_check egrep -q "$portnum *NO *iscsi *iscsi.*$TARGET" portlist.txt
}
create_iscsi_cleanup()
{
	cleanup iscsi
}

atf_test_case create_iscsi_alias cleanup
create_iscsi_alias_head()
{
	atf_set "descr" "ctladm can create a new iscsi port with a target alias"
	atf_set "require.user" "root"
}
create_iscsi_alias_body()
{
	skip_if_ctld
	load_cfiscsi

	TARGET=iqn.2018-10.myhost.create_iscsi_alias
	ALIAS="foobar"
	atf_check -o save:port-create.txt ctladm port -c -d "iscsi" -O cfiscsi_portal_group_tag=$PGTAG -O cfiscsi_target="$TARGET" -O cfiscsi_target_alias="$ALIAS"
	echo "target: $TARGET" >> port-create.txt
	atf_check egrep -q "Port created successfully" port-create.txt
	atf_check egrep -q "frontend: *iscsi" port-create.txt
	atf_check egrep -q "port: *[0-9]+" port-create.txt
	atf_check -o save:portlist.txt ctladm portlist -qvf iscsi
	atf_check egrep -q "cfiscsi_target_alias=$ALIAS" portlist.txt
}
create_iscsi_alias_cleanup()
{
	cleanup iscsi
}

atf_test_case create_iscsi_without_required_args
create_iscsi_without_required_args_head()
{
	atf_set "descr" "ctladm will gracefully fail to create an iSCSI target if required arguments are missing"
	atf_set "require.user" "root"
}
create_iscsi_without_required_args_body()
{
	skip_if_ctld
	load_cfiscsi

	TARGET=iqn.2018-10.myhost.create_iscsi
	atf_check -s exit:1 -e match:"Missing required argument: cfiscsi_target" ctladm port -c -d "iscsi" -O cfiscsi_portal_group_tag=$PGTAG
	atf_check -s exit:1 -e match:"Missing required argument: cfiscsi_portal_group_tag" ctladm port -c -d "iscsi" -O cfiscsi_target=$TARGET
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

atf_test_case remove_iscsi
remove_iscsi_head()
{
	atf_set "descr" "ctladm can remove an iscsi port"
	atf_set "require.user" "root"
}
remove_iscsi_body()
{
	skip_if_ctld
	load_cfiscsi

	TARGET=iqn.2018-10.myhost.remove_iscsi
	atf_check -o save:port-create.txt ctladm port -c -d "iscsi" -O cfiscsi_portal_group_tag=$PGTAG -O cfiscsi_target="$TARGET"
	portnum=`awk '/port:/ {print $2}' port-create.txt`
	atf_check -o save:portlist.txt ctladm portlist -qf iscsi
	atf_check -o inline:"Port destroyed successfully\n" ctladm port -r -d iscsi -O cfiscsi_portal_group_tag=$PGTAG -O cfiscsi_target="$TARGET"
	# Check that the port was removed.  A new port may have been added with
	# the same ID, so match against the target and tag, too.
	PGTAGHEX=0x7631	# PGTAG in hex
	if ctladm portlist -qf iscsi | egrep -q "^${portnum} .*$PGTAG +[0-9]+ +$TARGET,t,$PGTAGHEX"; then
		ctladm portlist -qf iscsi
		atf_fail "port was not removed"
	fi
}

atf_test_case remove_iscsi_without_required_args cleanup
remove_iscsi_without_required_args_head()
{
	atf_set "descr" "ctladm will gracefully fail to remove an iSCSI target if required arguments are missing"
	atf_set "require.user" "root"
}
remove_iscsi_without_required_args_body()
{
	skip_if_ctld
	load_cfiscsi

	TARGET=iqn.2018-10.myhost.remove_iscsi_without_required_args
	atf_check -o save:port-create.txt ctladm port -c -d "iscsi" -O cfiscsi_portal_group_tag=$PGTAG -O cfiscsi_target="$TARGET"
	echo "target: $TARGET" >> port-create.txt
	atf_check -s exit:1 -e match:"Missing required argument: cfiscsi_portal_group_tag" ctladm port -r -d iscsi -O cfiscsi_target="$TARGET"
	atf_check -s exit:1 -e match:"Missing required argument: cfiscsi_target" ctladm port -r -d iscsi -O cfiscsi_portal_group_tag=$PGTAG
}
remove_iscsi_without_required_args_cleanup()
{
	cleanup iscsi
}

atf_init_test_cases()
{
	atf_add_test_case create_ioctl
	atf_add_test_case create_iscsi
	atf_add_test_case create_iscsi_without_required_args
	atf_add_test_case create_iscsi_alias
	atf_add_test_case create_ioctl_options
	atf_add_test_case disable_ioctl
	atf_add_test_case enable_ioctl
	atf_add_test_case remove_ioctl
	atf_add_test_case remove_ioctl_without_required_args
	atf_add_test_case remove_iscsi
	atf_add_test_case remove_iscsi_without_required_args
}
