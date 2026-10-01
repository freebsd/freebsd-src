#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Maksym Sobolev <sobomax@FreeBSD.org>
#

# Mount a file system and record its mount point for mp_cleanup.
mp_mount()
{
	local mntpt

	for mntpt; do :; done
	[ -e "$mntpt" ] || atf_check mkdir -p "$mntpt"
	echo "$PWD/$mntpt" >> mounts
	atf_check mount "$@"
}

mp_cleanup()
{
	[ -f mounts ] || return 0
	tail -r mounts | while read mntpt; do
		umount -f "$mntpt" 2>/dev/null
	done
	return 0
}

# Populate a minimal root with sh, mountpoint and the given programs,
# together with the shared libraries they need.
mp_mkroot()
{
	local root lib prog

	root=$1
	shift
	atf_check mkdir -p "$root/bin" "$root/libexec" "$root/mnt"
	atf_check cp /libexec/ld-elf.so.1 "$root/libexec/"
	for prog in sh mountpoint "$@"; do
		prog=$(command -v "$prog") || atf_fail "$prog not found"
		atf_check cp "$prog" "$root/bin/"
		for lib in $(ldd -f '%p\n' "$prog" | grep '^/'); do
			atf_check mkdir -p "$root${lib%/*}"
			atf_check cp "$lib" "$root$lib"
		done
	done
}

mp_require_jail()
{
	if [ "$(sysctl -n security.jail.jailed)" != 0 ]; then
		atf_skip "Cannot create jails from within a jail"
	fi
}

# Run "mountpoint path" inside root, entered via chroot(8) or jail(8)
# as selected by mode, and check that it reports rc.  jail(8) does not
# pass the exit status of the command through, so it is echoed back.
mp_check()
{
	local mode root path rc not

	mode=$1
	root=$2
	path=$3
	rc=$4
	not=
	[ "$rc" -eq 0 ] || not="not "
	case "$mode" in
	chroot)
		set -- chroot "$root" /bin/sh
		;;
	jail)
		set -- jail -c path="$PWD/$root" command=/bin/sh
		;;
	esac
	atf_check -o inline:"$path is ${not}a mount point\nrc=$rc\n" \
	    "$@" -c "/bin/mountpoint $path; echo rc=\$?"
}

atf_test_case not_mountpoint
not_mountpoint_head()
{
	atf_set	"descr" "Verify that a plain directory or file is not " \
			"reported as a mount point"
}

not_mountpoint_body()
{
	atf_check mkdir dir
	atf_check -s exit:1 -o inline:"dir is not a mount point\n" \
	    mountpoint dir
	atf_check -s exit:1 mountpoint -q dir
	atf_check touch file
	atf_check -s exit:1 -o inline:"file is not a mount point\n" \
	    mountpoint file
}

atf_test_case errors
errors_head()
{
	atf_set	"descr" "Verify that errors are reported with exit status 2"
}

errors_body()
{
	atf_check -s exit:2 -e match:"No such file" mountpoint nonexistent
	atf_check -s exit:2 -e match:"usage" mountpoint
	atf_check -s exit:2 -e match:"usage" mountpoint a b
	atf_check -s exit:2 -e match:"usage" mountpoint -x .
}

atf_test_case mountpoint cleanup
mountpoint_head()
{
	atf_set	"descr" "Verify that a mounted file system root is " \
			"reported as a mount point, also through a symlink"
	atf_set	"require.user" "root"
}

mountpoint_body()
{
	mp_mount -t tmpfs tmpfs mnt
	atf_check -o inline:"mnt is a mount point\n" mountpoint mnt
	atf_check -o empty mountpoint -q mnt
	atf_check ln -s mnt link
	atf_check -o inline:"link is a mount point\n" mountpoint link
	atf_check mkdir mnt/sub
	atf_check -s exit:1 -o inline:"mnt/sub is not a mount point\n" \
	    mountpoint mnt/sub
}

mountpoint_cleanup()
{
	mp_cleanup
}

atf_test_case file_mountpoint cleanup
file_mountpoint_head()
{
	atf_set	"descr" "Verify that a regular file with a file system " \
			"mounted over it is reported as a mount point"
	atf_set	"require.user" "root"
	atf_set	"require.kmods" "nullfs"
}

file_mountpoint_body()
{
	atf_check touch src dst
	mp_mount -t nullfs "$PWD/src" dst
	atf_check -o inline:"dst is a mount point\n" mountpoint dst
	atf_check ln -s dst link
	atf_check -o inline:"link is a mount point\n" mountpoint link
	atf_check -s exit:1 -o inline:"src is not a mount point\n" \
	    mountpoint src
}

file_mountpoint_cleanup()
{
	mp_cleanup
}

# The root is a plain directory on top of a mount point.  Neither the
# root nor its ".." may be reported as a mount point, so the mount point
# above the root does not leak in.
root_not_mountpoint_body()
{
	mp_mount -t tmpfs tmpfs outer
	mp_mkroot outer/root
	mp_check $1 outer/root / 1
	mp_check $1 outer/root /.. 1
	mp_check $1 outer/root /bin 1
}

# The root is itself a mount point.
root_mountpoint_body()
{
	mp_mount -t tmpfs tmpfs root
	mp_mkroot root
	mp_check $1 root / 0
	mp_check $1 root /.. 0
	mp_check $1 root /bin 1
}

# The root is a nullfs mount, as is common for jails.
root_nullfs_body()
{
	mp_mkroot src
	mp_mount -t nullfs "$PWD/src" root
	mp_check $1 root / 0
	mp_check $1 root /bin 1
}

# The root is a plain directory with a file system mounted inside it.
submount_body()
{
	mp_mkroot root
	mp_mount -t tmpfs tmpfs root/mnt
	mp_check $1 root / 1
	mp_check $1 root /mnt 0
	mp_check $1 root /mnt/.. 1
}

atf_test_case chroot_root_not_mountpoint cleanup
chroot_root_not_mountpoint_head()
{
	atf_set	"descr" "Verify that a chroot root which is not a mount " \
			"point is not reported as one"
	atf_set	"require.user" "root"
}

chroot_root_not_mountpoint_body()
{
	root_not_mountpoint_body chroot
}

chroot_root_not_mountpoint_cleanup()
{
	mp_cleanup
}

atf_test_case chroot_root_mountpoint cleanup
chroot_root_mountpoint_head()
{
	atf_set	"descr" "Verify that a chroot root which is a mount point " \
			"is reported as one"
	atf_set	"require.user" "root"
}

chroot_root_mountpoint_body()
{
	root_mountpoint_body chroot
}

chroot_root_mountpoint_cleanup()
{
	mp_cleanup
}

atf_test_case chroot_root_nullfs cleanup
chroot_root_nullfs_head()
{
	atf_set	"descr" "Verify that a chroot root which is a nullfs " \
			"mount is reported as a mount point"
	atf_set	"require.user" "root"
	atf_set	"require.kmods" "nullfs"
}

chroot_root_nullfs_body()
{
	root_nullfs_body chroot
}

chroot_root_nullfs_cleanup()
{
	mp_cleanup
}

atf_test_case chroot_submount cleanup
chroot_submount_head()
{
	atf_set	"descr" "Verify that a file system mounted inside a chroot " \
			"is reported as a mount point"
	atf_set	"require.user" "root"
}

chroot_submount_body()
{
	submount_body chroot
}

chroot_submount_cleanup()
{
	mp_cleanup
}

atf_test_case jail_root_not_mountpoint cleanup
jail_root_not_mountpoint_head()
{
	atf_set	"descr" "Verify that a jail root which is not a mount " \
			"point is not reported as one"
	atf_set	"require.user" "root"
	atf_set	"require.progs" "jail"
}

jail_root_not_mountpoint_body()
{
	mp_require_jail
	root_not_mountpoint_body jail
}

jail_root_not_mountpoint_cleanup()
{
	mp_cleanup
}

atf_test_case jail_root_mountpoint cleanup
jail_root_mountpoint_head()
{
	atf_set	"descr" "Verify that a jail root which is a mount point " \
			"is reported as one"
	atf_set	"require.user" "root"
	atf_set	"require.progs" "jail"
}

jail_root_mountpoint_body()
{
	mp_require_jail
	root_mountpoint_body jail
}

jail_root_mountpoint_cleanup()
{
	mp_cleanup
}

atf_test_case jail_root_nullfs cleanup
jail_root_nullfs_head()
{
	atf_set	"descr" "Verify that a jail root which is a nullfs mount " \
			"is reported as a mount point"
	atf_set	"require.user" "root"
	atf_set	"require.progs" "jail"
	atf_set	"require.kmods" "nullfs"
}

jail_root_nullfs_body()
{
	mp_require_jail
	root_nullfs_body jail
}

jail_root_nullfs_cleanup()
{
	mp_cleanup
}

atf_test_case jail_submount cleanup
jail_submount_head()
{
	atf_set	"descr" "Verify that a file system mounted inside a jail " \
			"from the host is reported as a mount point"
	atf_set	"require.user" "root"
	atf_set	"require.progs" "jail"
}

jail_submount_body()
{
	mp_require_jail
	submount_body jail
}

jail_submount_cleanup()
{
	mp_cleanup
}

atf_test_case jail_mount_inside cleanup
jail_mount_inside_head()
{
	atf_set	"descr" "Verify that a file system mounted from within " \
			"a jail is reported as a mount point there"
	atf_set	"require.user" "root"
	atf_set	"require.progs" "jail"
}

jail_mount_inside_body()
{
	local out

	mp_require_jail
	mp_mkroot root mount umount
	echo "$PWD/root/mnt" >> mounts
	out="/mnt is a mount point\nrc=0\n/ is not a mount point\nrc=1\n"
	atf_check -o inline:"$out" jail -c path="$PWD/root" \
	    allow.mount allow.mount.tmpfs enforce_statfs=1 \
	    command=/bin/sh -c "/bin/mount -t tmpfs tmpfs /mnt &&
		/bin/mountpoint /mnt; echo rc=\$?;
		/bin/mountpoint /; echo rc=\$?;
		/bin/umount /mnt"
}

jail_mount_inside_cleanup()
{
	mp_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case not_mountpoint
	atf_add_test_case errors
	atf_add_test_case mountpoint
	atf_add_test_case file_mountpoint
	atf_add_test_case chroot_root_not_mountpoint
	atf_add_test_case chroot_root_mountpoint
	atf_add_test_case chroot_root_nullfs
	atf_add_test_case chroot_submount
	atf_add_test_case jail_root_not_mountpoint
	atf_add_test_case jail_root_mountpoint
	atf_add_test_case jail_root_nullfs
	atf_add_test_case jail_submount
	atf_add_test_case jail_mount_inside
}
