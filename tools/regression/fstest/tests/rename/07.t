#!/bin/sh
# $FreeBSD$

desc="rename returns EPERM if the parent directory of the file pointed at by the 'from' argument has its immutable or append-only flag set"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

case "${os}:${fs}" in
FreeBSD:ZFS)
	flags1="SF_IMMUTABLE SF_APPEND"
	flags2="SF_NOUNLINK"
	echo "1..110"
	;;
FreeBSD:UFS)
	flags1="SF_IMMUTABLE SF_APPEND UF_IMMUTABLE UF_APPEND"
	flags2="SF_NOUNLINK UF_NOUNLINK"
	echo "1..182"
	;;
*)
	quick_exit
esac

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755

expect 0 create ${n0}/${n1} 0644
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0}/${n1} ${n2}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 mkdir ${n0}/${n1} 0755
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0}/${n1} ${n2}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}/${n1}

expect 0 mkfifo ${n0}/${n1} 0644
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0}/${n1} ${n2}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 mknod ${n0}/${n1} c 0644 1 2
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0}/${n1} ${n2}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 bind ${n0}/${n1}
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0}/${n1} ${n2}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 symlink ${n2} ${n0}/${n1}
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect EPERM rename ${n0}/${n1} ${n2}
	[ "${flag}" = "SF_APPEND" ] && todo FreeBSD:ZFS "Renaming a file protected by SF_APPEND should return EPERM."
	expect ENOENT rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 create ${n0}/${n1} 0644
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n0}/${n1} ${n2}
	expect 0 rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 mkdir ${n0}/${n1} 0755
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n0}/${n1} ${n2}
	expect 0 rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}/${n1}

expect 0 mkfifo ${n0}/${n1} 0644
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n0}/${n1} ${n2}
	expect 0 rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 mknod ${n0}/${n1} c 0644 1 2
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n0}/${n1} ${n2}
	expect 0 rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 bind ${n0}/${n1}
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n0}/${n1} ${n2}
	expect 0 rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 symlink ${n2} ${n0}/${n1}
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n0}/${n1} ${n2}
	expect 0 rename ${n2} ${n0}/${n1}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 rmdir ${n0}
