#!/bin/sh
#
#

# PROVIDE: ldconfig
# REQUIRE: FILESYSTEMS
# BEFORE:  DAEMON

. /etc/rc.subr

name="ldconfig"
desc="Configure the shared library cache"
ldconfig_command="/sbin/ldconfig"
start_cmd="ldconfig_start"
stop_cmd=":"

ldconfig_paths()
{
	local _dirs _files _ii _ldpaths _paths

	_dirs="${1}"
	_paths="${2}"
	_ldpaths="${3}"

	for _ii in ${_dirs}; do
		if [ -d "${_ii}" ]; then
			_files=`find ${_ii} -type f`
			if [ -n "${_files}" ]; then
				_paths="${_paths} `cat ${_files} | sort -u`"
			fi
		fi
	done
	for _ii in ${_paths}; do
		if [ -r "${_ii}" ]; then
			_ldpaths="${_ldpaths} ${_ii}"
		fi
	done

	echo "${_ldpaths}"
}

ldconfig_start()
{
	local _files _ins

	_ins=
	ldconfig=${ldconfig_command}
	checkyesno ldconfig_insecure && _ins="-i"
	if [ -x "${ldconfig_command}" ]; then
		_LDC=$(/libexec/ld-elf.so.1 -v | sed -n -e '/^Default lib path /s///p' | tr : ' ')
		_LDC=$(ldconfig_paths "${ldconfig_local_dirs}" \
		    "${ldconfig_paths} /etc/ld-elf.so.conf" "$_LDC")
		startmsg 'ELF ldconfig path:' ${_LDC}
		${ldconfig} -elf ${_ins} ${_LDC}

		if check_kern_features compat_freebsd32; then
			_LDC=""
			if [ -x /libexec/ld-elf32.so.1 ]; then
				for x in $(/libexec/ld-elf32.so.1 -v | sed -n -e '/^Default lib path /s///p' | tr : ' '); do
					if [ -d "${x}" ]; then
						_LDC="${_LDC} ${x}"
					fi
				done
			fi
			_LDC=$(ldconfig_paths "${ldconfig_local32_dirs}" \
			    "${ldconfig32_paths}" "$_LDC")
			startmsg '32-bit compatibility ldconfig path:' ${_LDC}
			${ldconfig} -32 ${_ins} ${_LDC}
		fi

	fi
}

load_rc_config $name

# doesn't make sense to run in a svcj: config setting
ldconfig_svcj="NO"

run_rc_command "$1"
