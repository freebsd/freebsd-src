#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2019 Rob Wing
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

# The code for the following tests was copied from the
# bectl tests found in src/sbin/bectl/tests, modified as needed.

ZPOOL_NAME_FILE=zpool_name
get_zpool_name()
{
	cat $ZPOOL_NAME_FILE
}
make_zpool_name()
{
	mktemp -u libbe_test_XXXXXX > $ZPOOL_NAME_FILE
	get_zpool_name
}

# Establishes a libbe zpool that can be used for some light testing; contains
# a 'default' BE and not much else.
libbe_create_setup()
{
	zpool=$1
	disk=$2
	mnt=$3

	# Sanity check to make sure `make_zpool_name` succeeded
	atf_check test -n "$zpool"

	kldload -n -q zfs || atf_skip "ZFS module not loaded on the current system"
	atf_check mkdir -p ${mnt}
	atf_check truncate -s 1G ${disk}
	atf_check zpool create -o altroot=${mnt} ${zpool} ${disk}
	atf_check zfs create -o mountpoint=none ${zpool}/ROOT
	atf_check zfs create -o mountpoint=/ -o canmount=noauto \
	    ${zpool}/ROOT/default
        atf_check zfs create -o mountpoint=/usr -o canmount=noauto \
	    ${zpool}/ROOT/default/usr
        atf_check zfs create -o mountpoint=/usr/obj -o canmount=noauto \
	    ${zpool}/ROOT/default/usr/obj
}

libbe_cleanup()
{
	zpool=$1
	cwd=$(atf_get_srcdir)

	if [ -z "$zpool" ]; then
		echo "Skipping cleanup; zpool not set up"
	elif zpool get health ${zpool} >/dev/null 2>&1; then
		zpool destroy -f ${zpool}
	fi

	if [ -f "${cwd}/disk.img" ]; then
		rm ${cwd}/disk.img
	fi
}

atf_test_case libbe_create cleanup
libbe_create_head()
{
	atf_set "descr" "check _be_create from libbe"
	atf_set "require.user" root
}
libbe_create_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(atf_get_srcdir)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt
	prog=${cwd}/./target_prog

	# preliminary setup/checks
	atf_require_prog $prog
	libbe_create_setup ${zpool} ${disk} ${mount}

	# a recursive and non-recursive snapshot to test against
	atf_check zfs snapshot ${zpool}/ROOT/default@non-recursive
	atf_check zfs snapshot -r ${zpool}/ROOT/default@recursive

	# create a dataset after snapshots were taken
        atf_check zfs create -o mountpoint=/usr/src -o canmount=noauto \
	    ${zpool}/ROOT/default/usr/src

	# test boot environment creation with depth of 0 (i.e. a non-recursive boot environment).
	atf_check $prog "${zpool}/ROOT" \
		nonrecursive \
		"${zpool}/ROOT/default@non-recursive" \
		0
	# the dataset should exist
	atf_check -o not-empty \
		zfs list "${zpool}/ROOT/nonrecursive"
	# the child dataset should not exist.
	atf_check -e not-empty -s not-exit:0 \
		zfs list "${zpool}/ROOT/nonrecursive/usr"

	# test boot environment creation with unlimited depth (i.e. a recursive boot environment).
	atf_check $prog "${zpool}/ROOT" \
		recursive \
		"${zpool}/ROOT/default@recursive" \
		-1
	# the dataset should exist
	atf_check -o not-empty \
		zfs list "${zpool}/ROOT/recursive"
	# the child dataset should exist
	atf_check -o not-empty \
		  zfs list "${zpool}/ROOT/recursive/usr"
	# the child dataset should exist
	atf_check -o not-empty \
		  zfs list "${zpool}/ROOT/recursive/usr/obj"
	# the child dataset should not exist.
	atf_check -e not-empty -s not-exit:0 \
		zfs list "${zpool}/ROOT/recursive/usr/src"

	# test boot environment creation with a depth of 1
	atf_check $prog "${zpool}/ROOT" \
		depth \
		"${zpool}/ROOT/default@recursive" \
		1
	# the child dataset should exist
	atf_check -o not-empty \
		zfs list "${zpool}/ROOT/depth/usr"
	# the child dataset should not exist.
	atf_check -e not-empty -s not-exit:0 \
		zfs list "${zpool}/ROOT/depth/usr/obj"
	# the child dataset should not exist.
	atf_check -e not-empty -s not-exit:0 \
		zfs list "${zpool}/ROOT/depth/usr/src"


	# create a recursive boot environment named 'relative-snap'.
	# This test is to ensure that a relative snapshot label can be used,
	# (i.e. the format: 'bootenvironment@snapshot')
	atf_check $prog "${zpool}/ROOT" \
		relative-snap \
		default@recursive \
		-1
	# the dataset should exist
	atf_check -o not-empty \
		zfs list "${zpool}/ROOT/relative-snap"
	# the child dataset should exist
	atf_check -o not-empty \
		  zfs list "${zpool}/ROOT/relative-snap/usr"
}

libbe_create_cleanup()
{
	libbe_cleanup $(get_zpool_name)
}

atf_init_test_cases()
{
	atf_add_test_case libbe_create
}
