#!/bin/sh
# $FreeBSD$

desc="rename returns EPERM if the file pointed at by the 'from' argument has its immutable, undeletable or append-only flag set"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

case "${os}:${fs}" in
FreeBSD:ZFS)
	flags="SF_IMMUTABLE SF_NOUNLINK SF_APPEND"
	echo "1..105"
	;;
FreeBSD:UFS)
	flags="SF_IMMUTABLE SF_NOUNLINK SF_APPEND UF_IMMUTABLE UF_NOUNLINK UF_APPEND"
	echo "1..189"
	;;
*)
	quick_exit
esac

n0=`namegen`
n1=`namegen`

expect 0 create ${n0} 0644
for flag in ${flags}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0} ${n1}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n1} ${n0}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0755
for flag in ${flags}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a directory protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0} ${n1}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a directory protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n1} ${n0}
done
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}

expect 0 mkfifo ${n0} 0644
for flag in ${flags}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a fifo protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0} ${n1}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a fifo protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n1} ${n0}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 mknod ${n0} b 0644 1 2
for flag in ${flags}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a device protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0} ${n1}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a device protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n1} ${n0}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 mknod ${n0} c 0644 1 2
for flag in ${flags}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a device protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0} ${n1}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a device protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n1} ${n0}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 bind ${n0}
for flag in ${flags}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a socket protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0} ${n1}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a socket protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n1} ${n0}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 symlink ${n1} ${n0}
for flag in ${flags}; do
	expect 0 lchflags ${n0} ${flag}
	expect ${flag} lstat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a symlink protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0} ${n1}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a symlink protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n1} ${n0}
done
expect 0 lchflags ${n0} none
expect 0 unlink ${n0}
