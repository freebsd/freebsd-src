#
# Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

ZPOOL_NAME_FILE=zpool_name
get_zpool_name()
{
	cat $ZPOOL_NAME_FILE
}
make_zpool_name()
{
	mktemp -u bectl_test_XXXXXX > $ZPOOL_NAME_FILE
	get_zpool_name
}

# Establishes a bectl_create zpool that can be used for some light testing; contains
# a 'default' BE and not much else.
bectl_create_setup()
{
	zpool=$1
	disk=$2
	mnt=$3

	# Sanity check to make sure `make_zpool_name` succeeded
	atf_check test -n "$zpool"

	kldload -n -q zfs || atf_skip "ZFS module not loaded on the current system"
	if ! getconf MIN_HOLE_SIZE "$(pwd)"; then
		echo "getconf MIN_HOLE_SIZE $(pwd) failed; sparse files " \
		    "probably not supported by file system"
		mount
		atf_skip "Test's work directory does not support sparse files;" \
		    "try with a different TMPDIR?"
	fi
	atf_check mkdir -p ${mnt}
	atf_check truncate -s 1G ${disk}
	atf_check zpool create -R ${mnt} ${zpool} ${disk}
	atf_check zfs create -o mountpoint=none ${zpool}/ROOT
	atf_check zfs create -o mountpoint=/ -o canmount=noauto \
	    ${zpool}/ROOT/default
}
bectl_create_deep_setup()
{
	zpool=$1
	disk=$2
	mnt=$3

	# Sanity check to make sure `make_zpool_name` succeeded
	atf_check test -n "$zpool"

	bectl_create_setup ${zpool} ${disk} ${mnt}
	atf_check mkdir -p ${root}
	atf_check -o ignore bectl -r ${zpool}/ROOT mount default ${root}
	atf_check mkdir -p ${root}/usr
	atf_check zfs create -o mountpoint=/usr -o canmount=noauto \
	    ${zpool}/ROOT/default/usr
	atf_check -o ignore bectl -r ${zpool}/ROOT umount default
}

bectl_cleanup()
{
	zpool=$1
	if [ -z "$zpool" ]; then
		echo "Skipping cleanup; zpool not set up"
	elif zpool get health ${zpool} >/dev/null 2>&1; then
		zpool destroy -f ${zpool}
	fi
}

atf_test_case bectl_create cleanup
bectl_create_head()
{
	atf_set "descr" "Check the various forms of bectl create"
	atf_set "require.user" root
}
bectl_create_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt

	bectl_create_setup ${zpool} ${disk} ${mount}

	# Create a child dataset that will be used to test creation
	# of recursive and non-recursive boot environments.
	atf_check zfs create -o mountpoint=/usr -o canmount=noauto \
	    ${zpool}/ROOT/default/usr

	# BE datasets with spaces are not bootable, PR 254441.
	atf_check -e not-empty -s not-exit:0 \
		bectl -r ${zpool}/ROOT create "foo bar"

	# Test standard creation, creation of a snapshot, and creation from a
	# snapshot.
	atf_check bectl -r ${zpool}/ROOT create -e default default2
	atf_check bectl -r ${zpool}/ROOT create default2@test_snap
	atf_check bectl -r ${zpool}/ROOT create -e default2@test_snap default3

	# Test standard creation, creation of a snapshot, and creation from a
	# snapshot for recursive boot environments.
	atf_check bectl -r ${zpool}/ROOT create -r -e default recursive
	atf_check bectl -r ${zpool}/ROOT create -r recursive@test_snap
	atf_check bectl -r ${zpool}/ROOT create -r -e recursive@test_snap recursive-snap

	# Test that non-recursive boot environments have no child datasets.
	atf_check -e not-empty -s not-exit:0 \
		zfs list "${zpool}/ROOT/default2/usr"
	atf_check -e not-empty -s not-exit:0 \
		zfs list "${zpool}/ROOT/default3/usr"

	# Test that recursive boot environments have child datasets.
	atf_check -o not-empty \
		zfs list "${zpool}/ROOT/recursive/usr"
	atf_check -o not-empty \
		zfs list "${zpool}/ROOT/recursive-snap/usr"
}
bectl_create_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_test_case bectl_destroy cleanup
bectl_destroy_head()
{
	atf_set "descr" "Check bectl destroy"
	atf_set "require.user" root
}
bectl_destroy_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt
	root=${mount}/root

	bectl_create_setup ${zpool} ${disk} ${mount}
	atf_check bectl -r ${zpool}/ROOT create -e default default2
	atf_check -o not-empty zfs get mountpoint ${zpool}/ROOT/default2
	atf_check -e ignore bectl -r ${zpool}/ROOT destroy default2
	atf_check -e not-empty -s not-exit:0 zfs get mountpoint ${zpool}/ROOT/default2

	# Test origin snapshot deletion when the snapshot to be destroyed
	# belongs to a mounted dataset, see PR 236043.
	atf_check mkdir -p ${root}
	atf_check -o not-empty bectl -r ${zpool}/ROOT mount default ${root}
	atf_check bectl -r ${zpool}/ROOT create -e default default3
	atf_check bectl -r ${zpool}/ROOT destroy -o default3
	atf_check bectl -r ${zpool}/ROOT unmount default

	# create two be from the same parent and destroy the parent
	atf_check bectl -r ${zpool}/ROOT create -e default default2
	atf_check bectl -r ${zpool}/ROOT create -e default default3
	atf_check bectl -r ${zpool}/ROOT destroy default
	atf_check bectl -r ${zpool}/ROOT destroy default2
	atf_check bectl -r ${zpool}/ROOT rename default3 default

	# Create a BE, have it be the parent for another and repeat, then start
	# deleting environments.  Arbitrarily chose default3 as the first.
	# Sleeps are required to prevent conflicting snapshots- libbe will
	# use the time with a serial at the end as needed to prevent collisions,
	# but as BEs get promoted the snapshot names will convert and conflict
	# anyways.  libbe should perhaps consider adding something extra to the
	# default name to prevent collisions like this, but the default name
	# includes down to the second and creating BEs this rapidly is perhaps
	# uncommon enough.
	atf_check bectl -r ${zpool}/ROOT create -e default default2
	sleep 1
	atf_check bectl -r ${zpool}/ROOT create -e default2 default3
	sleep 1
	atf_check bectl -r ${zpool}/ROOT create -e default3 default4
	atf_check bectl -r ${zpool}/ROOT destroy default3
	atf_check bectl -r ${zpool}/ROOT destroy default2
	atf_check bectl -r ${zpool}/ROOT destroy default4

	# Create two BEs, then create an unrelated snapshot on the originating
	# BE and destroy it.  We shouldn't have promoted the second BE, and it's
	# only possible to tell if we promoted it by making sure we didn't
	# demote the first BE at some point -- if we did, it's origin will no
	# longer be empty.
	atf_check bectl -r ${zpool}/ROOT create -e default default2
	atf_check bectl -r ${zpool}/ROOT create default@test

	atf_check bectl -r ${zpool}/ROOT destroy default@test
	atf_check -o inline:"-\n" zfs get -Ho value origin ${zpool}/ROOT/default
	atf_check bectl -r ${zpool}/ROOT destroy default2

	# As observed by beadm, if we explicitly try to destroy a snapshot that
	# leads to clones, we shouldn't have allowed it.
	atf_check bectl -r ${zpool}/ROOT create default@test
	atf_check bectl -r ${zpool}/ROOT create -e default@test default2

	atf_check -e  not-empty -s not-exit:0 bectl -r ${zpool}/ROOT destroy \
	    default@test
}
bectl_destroy_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_test_case bectl_export_import cleanup
bectl_export_import_head()
{
	atf_set "descr" "Check bectl export and import"
	atf_set "require.user" root
}
bectl_export_import_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt

	bectl_create_setup ${zpool} ${disk} ${mount}
	atf_check -o save:exported bectl -r ${zpool}/ROOT export default
	atf_check -x "bectl -r ${zpool}/ROOT import default2 < exported"
	atf_check -o not-empty zfs get mountpoint ${zpool}/ROOT/default2
	atf_check -e ignore bectl -r ${zpool}/ROOT destroy default2
	atf_check -e not-empty -s not-exit:0 zfs get mountpoint \
	    ${zpool}/ROOT/default2
}
bectl_export_import_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_test_case bectl_list cleanup
bectl_list_head()
{
	atf_set "descr" "Check bectl list"
	atf_set "require.user" root
}
bectl_list_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt

	bectl_create_setup ${zpool} ${disk} ${mount}
	# Test the list functionality, including that BEs come and go away
	# as they're created and destroyed.  Creation and destruction tests
	# use the 'zfs' utility to verify that they're actually created, so
	# these are just light tests that 'list' is picking them up.
	atf_check -o save:list.out bectl -r ${zpool}/ROOT list
	atf_check -o not-empty grep 'default' list.out
	atf_check bectl -r ${zpool}/ROOT create -e default default2
	atf_check -o save:list.out bectl -r ${zpool}/ROOT list
	atf_check -o not-empty grep 'default2' list.out
	atf_check -e ignore bectl -r ${zpool}/ROOT destroy default2
	atf_check -o save:list.out bectl -r ${zpool}/ROOT list
	atf_check -s not-exit:0 grep 'default2' list.out
	# XXX TODO: Formatting checks
}
bectl_list_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_test_case bectl_mount cleanup
bectl_mount_head()
{
	atf_set "descr" "Check bectl mount/unmount"
	atf_set "require.user" root
}
bectl_mount_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt
	root=${mount}/root

	bectl_create_deep_setup ${zpool} ${disk} ${mount}
	atf_check mkdir -p ${root}
	# Test unmount first...
	atf_check -o not-empty bectl -r ${zpool}/ROOT mount default ${root}
	atf_check -o not-empty -x "mount | grep '^${zpool}/ROOT/default'"
	atf_check bectl -r ${zpool}/ROOT unmount default
	atf_check -s not-exit:0 -x "mount | grep '^${zpool}/ROOT/default'"
	# Then umount!
	atf_check -o not-empty bectl -r ${zpool}/ROOT mount default ${root}
	atf_check -o not-empty -x "mount | grep '^${zpool}/ROOT/default'"
	atf_check bectl -r ${zpool}/ROOT umount default
	atf_check -s not-exit:0 -x "mount | grep '^${zpool}/ROOT/default'"
}
bectl_mount_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_test_case bectl_rename cleanup
bectl_rename_head()
{
	atf_set "descr" "Check bectl rename"
	atf_set "require.user" root
}
bectl_rename_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt

	bectl_create_setup ${zpool} ${disk} ${mount}
	atf_check bectl -r ${zpool}/ROOT rename default default2
	atf_check -o not-empty zfs get mountpoint ${zpool}/ROOT/default2
	atf_check -e not-empty -s not-exit:0 zfs get mountpoint \
	    ${zpool}/ROOT/default
}
bectl_rename_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_test_case bectl_jail cleanup
bectl_jail_head()
{
	atf_set "descr" "Check bectl rename"
	atf_set "require.user" root
	atf_set "require.progs" jail
}
bectl_jail_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt
	root=${mount}/root

	if [ ! -f /rescue/rescue ]; then
		atf_skip "This test requires a rescue binary"
	fi
	bectl_create_deep_setup ${zpool} ${disk} ${mount}
	# Prepare our minimal BE... plop a rescue binary into it
	atf_check mkdir -p ${root}
	atf_check -o ignore bectl -r ${zpool}/ROOT mount default ${root}
	atf_check mkdir -p ${root}/rescue
	atf_check cp /rescue/rescue ${root}/rescue/rescue
	atf_check bectl -r ${zpool}/ROOT umount default

	# Prepare some more boot environments
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT create -e default target
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT create -e default 1234

	# Attempt to unjail a BE with numeric name; jail_getid at one point
	# did not validate that the input was a valid jid before returning the
	# jid.
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT jail -b 1234
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT unjail 1234

	# When a jail name is not explicit, it should match the jail id.
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT jail -b -o jid=233637 default
	atf_check -o inline:"233637\n" -s exit:0 -x "jls -j 233637 name"
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT unjail default

	# Basic command-mode tests, with and without jail cleanup
	atf_check -o inline:"rescue\nusr\n" bectl -r ${zpool}/ROOT \
	    jail default /rescue/rescue ls -1
	atf_check -o inline:"rescue\nusr\n" bectl -r ${zpool}/ROOT \
	    jail -Uo path=${root} default /rescue/rescue ls -1
	atf_check [ -f ${root}/rescue/rescue ]
	atf_check bectl -r ${zpool}/ROOT ujail default

	# Batch mode tests
	atf_check bectl -r ${zpool}/ROOT jail -bo path=${root} default
	atf_check -o not-empty -x "jls | grep -F \"${root}\""
	atf_check bectl -r ${zpool}/ROOT ujail default
	atf_check -s not-exit:0 -x "jls | grep -F \"${root}\""
	# 'unjail' naming
	atf_check bectl -r ${zpool}/ROOT jail -b default
	atf_check bectl -r ${zpool}/ROOT unjail default
	atf_check -s not-exit:0 -x "jls | grep -F \"${root}\""
	# 'unjail' by BE name. Force bectl to lookup jail id by the BE name.
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT jail -b default
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT jail -b -o name=bectl_test target
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT unjail target
	atf_check -o empty -s exit:0 bectl -r ${zpool}/ROOT unjail default
	# cannot unjail an unjailed BE (by either command name)
	atf_check -e ignore -s not-exit:0 bectl -r ${zpool}/ROOT ujail default
	atf_check -e ignore -s not-exit:0 bectl -r ${zpool}/ROOT unjail default

	# set+unset
	atf_check bectl -r ${zpool}/ROOT jail -b -o path=${root} -u path default
	# Ensure that it didn't mount at ${root}
	atf_check -s not-exit:0 -x "mount | grep -F '${root}'"
	atf_check bectl -r ${zpool}/ROOT ujail default
}

# If a test has failed, it's possible that the boot environment hasn't
# been 'unjail'ed. We want to remove the jail before 'bectl_cleanup'
# attempts to destroy the zpool.
bectl_jail_cleanup()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	zpool=$(get_zpool_name)
	for bootenv in "default" "target" "1234"; do
		# mountpoint of the boot environment
		mountpoint="$(bectl -r ${zpool}/ROOT list -H | grep ${bootenv} | awk '{print $3}')"

		# see if any jail paths match the boot environment mountpoint
		jailid="$(jls | grep ${mountpoint} | awk '{print $1}')"

		if [ -z "$jailid" ]; then
		       continue;
		fi
		jail -r ${jailid}
	done;

	bectl_cleanup ${zpool}
}

atf_test_case bectl_promotion cleanup
bectl_promotion_head()
{
	atf_set "descr" "Check bectl promotion upon activation"
	atf_set "require.user" root
}
bectl_promotion_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt
	root=${mount}/root

	bectl_create_deep_setup ${zpool} ${disk} ${mount}
	atf_check mkdir -p ${root}

	# Sleeps interspersed to workaround some naming quirks; notably,
	# bectl will append a serial if two snapshots were created within
	# the same second, but it can only do that for the one root it's
	# operating on.  It won't check that other roots don't have a snapshot
	# with the same name, and the promotion will fail.
	atf_check bectl -r ${zpool}/ROOT rename default A
	sleep 1
	atf_check bectl -r ${zpool}/ROOT create -r -e A B
	sleep 1
	atf_check bectl -r ${zpool}/ROOT create -r -e B C

	# C should be a clone of B to start with
	atf_check -o not-inline:"-" zfs list -Hr -o origin ${zpool}/ROOT/C

	# Activating it should then promote it all the way out of clone-hood.
	# This entails two promotes internally, as the first would promote it to
	# a snapshot of A before finally promoting it the second time out of
	# clone status.
	atf_check -o not-empty bectl -r ${zpool}/ROOT activate C
	atf_check -o inline:"-\n-\n" zfs list -Hr -o origin ${zpool}/ROOT/C
}
bectl_promotion_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_test_case bectl_destroy_bootonce cleanup
bectl_destroy_bootonce_head()
{
	atf_set "descr" "Check bectl destroy (bootonce)"
	atf_set "require.user" root
}
bectl_destroy_bootonce_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt
	root=${mount}/root

	be=default2

	bectl_create_setup ${zpool} ${disk} ${mount}
	atf_check -s exit:0 -o empty bectl -r ${zpool}/ROOT create -e default ${be}

	# Create boot environment and bootonce activate it
	atf_check -s exit:0 -o ignore bectl -r ${zpool}/ROOT activate -t ${be}
	atf_check -s exit:0 -o inline:"zfs:${zpool}/ROOT/${be}:\n" zfsbootcfg -z ${zpool}

	# Destroy it
	atf_check -s exit:0 -o ignore bectl -r ${zpool}/ROOT destroy ${be}

	# Should be empty
	atf_check -s exit:0 -o empty zfsbootcfg -z ${zpool}
}
bectl_destroy_bootonce_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_test_case bectl_rename_bootonce cleanup
bectl_rename_bootonce_head()
{
	atf_set "descr" "Check bectl destroy (bootonce)"
	atf_set "require.user" root
}
bectl_rename_bootonce_body()
{
	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "i386" ]; then
		atf_skip "https://bugs.freebsd.org/249055"
	fi

	if [ "$(atf_config_get ci false)" = "true" ] && \
		[ "$(uname -p)" = "armv7" ]; then
		atf_skip "https://bugs.freebsd.org/249229"
	fi

	cwd=$(realpath .)
	zpool=$(make_zpool_name)
	disk=${cwd}/disk.img
	mount=${cwd}/mnt
	root=${mount}/root

	be=default2

	bectl_create_setup ${zpool} ${disk} ${mount}
	atf_check -s exit:0 -o empty bectl -r ${zpool}/ROOT create -e default ${be}

	# Create boot environment and bootonce activate it
	atf_check -s exit:0 -o ignore bectl -r ${zpool}/ROOT activate -t ${be}
	atf_check -s exit:0 -o inline:"zfs:${zpool}/ROOT/${be}:\n" zfsbootcfg -z ${zpool}

	# Rename it
	atf_check -s exit:0 -o ignore bectl -r ${zpool}/ROOT rename ${be} ${be}_renamed

	# Should be renamed
	atf_check -s exit:0 -o inline:"zfs:${zpool}/ROOT/${be}_renamed:\n" zfsbootcfg -z ${zpool}
}
bectl_rename_bootonce_cleanup()
{
	bectl_cleanup $(get_zpool_name)
}

atf_init_test_cases()
{
	atf_add_test_case bectl_create
	atf_add_test_case bectl_destroy
	atf_add_test_case bectl_export_import
	atf_add_test_case bectl_list
	atf_add_test_case bectl_mount
	atf_add_test_case bectl_rename
	atf_add_test_case bectl_jail
	atf_add_test_case bectl_promotion
	atf_add_test_case bectl_destroy_bootonce
	atf_add_test_case bectl_rename_bootonce
}
