#! /bin/sh
#
# Copyright (c) 2002 Gordon Tetlow. All rights reserved.
# Copyright (c) 2012 Sandvine Incorporated. All rights reserved.
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

delete="NO"
kenv=
force="NO"
nextboot_file="/boot/nextboot.conf"

add_kenv()
{
	local var value

	var=$1
	# strip literal quotes if passed in
	value=${2%\"*}
	value=${value#*\"}

	if [ -n "${kenv}" ]; then
		kenv="${kenv}
"
	fi
	kenv="${kenv}${var}=\"${value}\""
}

display_usage() {
	echo "Usage: nextboot [-e variable=value] [-f] [-k kernel] [-o options]"
	echo "       nextboot -D"
}

while getopts "De:fk:o:" argument ; do
	case "${argument}" in
	D)
		delete="YES"
		;;
	e)
		var=${OPTARG%%=*}
		value=${OPTARG#*=}
		if [ -z "$var" -o -z "$value" ]; then
			display_usage
			exit 1
		fi
		add_kenv "$var" "$value"
		;;
	f)
		force="YES"
		;;
	k)
		kernel="${OPTARG}"
		add_kenv kernel "$kernel"
		;;
	o)
		add_kenv kernel_options "${OPTARG}"
		;;
	*)
		display_usage
		exit 1
		;;
	esac
done

if [ ${delete} = "YES" ]; then
	rm -f ${nextboot_file}
	exit 0
fi

if [ -z "${kenv}" ]; then
	display_usage
	exit 1
fi

if [ -n "${kernel}" -a ${force} = "NO" -a ! -d /boot/${kernel} ]; then
	echo "Error: /boot/${kernel} doesn't exist. Use -f to override."
	exit 1
fi

df -Tn "/boot/" 2>/dev/null | while read _fs _type _other ; do
	[ "zfs" = "${_type}" ] || continue
	cat 1>&2 <<-EOF
		WARNING: loader(8) has only R/O support for ZFS
		nextboot.conf will NOT be reset in case of kernel boot failure
	EOF
done

cat > ${nextboot_file} << EOF
nextboot_enable="YES"
$kenv
EOF
