#!/bin/sh
#
#

# PROVIDE: linux
# REQUIRE: kldxref zfs
# KEYWORD: nojail

. /etc/rc.subr

name="linux"
desc="Enable Linux ABI"
rcvar="linux_enable"
start_cmd="${name}_start"
stop_cmd=":"

linux_mount() {
	local _fs _mount_point
	_fs="$1"
	_mount_point="$2"
	shift 2
	if ! mount | grep -q "^$_fs on $_mount_point ("; then
		mkdir -p "$_mount_point"
		mount "$@" -t "$_fs" "$_fs" "$_mount_point"
	fi
}

linux_start()
{
	local _emul_path _tmpdir

	case `sysctl -n hw.machine_arch` in
	aarch64)
		load_kld -e 'linux64elf' linux64
		;;
	amd64)
		load_kld -e 'linuxelf' linux
		load_kld -e 'linux64elf' linux64
		;;
	i386)
		load_kld -e 'linuxelf' linux
		;;
	esac

	_emul_path="$(sysctl -n compat.linux.emul_path)"

	if [ -x ${_emul_path}/sbin/ldconfigDisabled ]; then
		_tmpdir=`mktemp -d -t linux-ldconfig`
		${_emul_path}/sbin/ldconfig -C ${_tmpdir}/ld.so.cache
		if ! cmp -s ${_tmpdir}/ld.so.cache ${_emul_path}/etc/ld.so.cache; then
			cat ${_tmpdir}/ld.so.cache > ${_emul_path}/etc/ld.so.cache
		fi
		rm -rf ${_tmpdir}
	fi

	# Linux uses the pre-pts(4) tty naming scheme.
	load_kld pty

	# Explicitly load the filesystem modules; they are usually required,
	# even with linux_mounts_enable="NO".
	load_kld fdescfs
	load_kld linprocfs
	load_kld linsysfs

	# Handle unbranded ELF executables by defaulting to ELFOSABI_LINUX.
	if [ `sysctl -ni kern.elf64.fallback_brand` -eq "-1" ]; then
		sysctl kern.elf64.fallback_brand=3 > /dev/null
	fi

	if [ `sysctl -ni kern.elf32.fallback_brand` -eq "-1" ]; then
		sysctl kern.elf32.fallback_brand=3 > /dev/null
	fi

	if checkyesno linux_mounts_enable; then
		linux_mount linprocfs "${_emul_path}/proc" -o nocover
		linux_mount linsysfs "${_emul_path}/sys" -o nocover
		linux_mount devfs "${_emul_path}/dev" -o nocover
		linux_mount fdescfs "${_emul_path}/dev/fd" -o nocover,linrdlnk
		linux_mount tmpfs "${_emul_path}/dev/shm" -o nocover,mode=1777
	fi
}

load_rc_config $name

# doesn't make sense to run in a svcj: kernel modules and FS-mounting
linux_svcj="NO"

run_rc_command "$1"
