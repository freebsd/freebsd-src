#!/bin/sh

#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2023 Beckhoff Automation GmbH & Co. KG
# Copyright 2023 Bjoern A. Zeeb
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted providing that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

: ${LIBEXEC_PATH:="/usr/libexec/fwget"}

usage()
{
	cat <<EOF
Usage: $(basename "$0") [options] [subsystem]

Supported subsystems
  pci, usb

Options:
  -n		-- Do not install packages, only print the results
  -q		-- Quiet mode.  If used with -n only prints a package a line
  -v		-- More verbose
EOF
	exit 1
}

log()
{
	if [ "${QUIET}" != "y" ]; then
		echo "$@"
	fi
}

log_verbose()
{
	if [ "${VERBOSE}" = "n" ]; then
		return
	fi

	echo "$@"
}

addpkg()
{
	local _p

	_p=$1

	case "${packages}" in
	"")	packages="${_p}" ;;
	*)	# Avoid duplicates.
		case " ${packages} " in
		*\ ${_p}\ *) ;;	# duplicate
		*)	packages="${packages} ${_p}" ;;
		esac
	esac
}

DRY_RUN=n
QUIET=n
VERBOSE=n

while getopts ":nqv" _arg; do
	case ${_arg} in
	n)
		DRY_RUN=y
		;;
	q)
		QUIET=y
		;;
	v)
		VERBOSE=y
		;;
	?)
		usage
		;;
	esac
done
shift $(($OPTIND - 1))
subsystems="$@"

# Default searching PCI and USB subsystem
if [ -z "${subsystems}" ]; then
	subsystems="pci usb"
fi

# Fail early on unsupported subsystem
for subsystem in ${subsystems}; do
	if [ ! -f "${LIBEXEC_PATH}"/"${subsystem}" ]; then
		usage
	fi
	. "${LIBEXEC_PATH}"/"${subsystem}"
done

packages=""
for subsystem in ${subsystems}; do
	"${subsystem}"_search_packages
done

case "${packages}" in
""|^[[:space:]]*$)
	log "No firmware packages to install."
	exit 0
	;;
esac

log "Needed firmware packages: '${packages}'"
if [ "${DRY_RUN}" = "y" ]; then
	if [ "${QUIET}" = "y" ]; then
		for pkg in ${packages}; do
			case "${pkg}" in
			""|^[[:space:]]*$) continue ;;
			esac
			echo "${pkg}"
		done
	fi
	exit 0
fi

pkg install -qy ${packages}
