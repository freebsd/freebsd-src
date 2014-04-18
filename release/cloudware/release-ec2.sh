#!/bin/sh
#-
# Copyright (c) 2014 The FreeBSD Foundation
# All rights reserved.
#
# This software was developed by Glen Barber
# under sponsorship from the FreeBSD Foundation.
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
#

# Script to produce Amazon EC2 images.
# This is heavily based on work done by Colin Percival, and his
# code in ^/user/cperciva/EC2-build.

PATH="/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin"
export PATH

RELENGDIR="$(realpath $(dirname $(basename ${0})))"

# Default settings if a configuration file is not specified:
CHROOTDIR="/scratch"
CONF="${RELENGDIR}/ec2.conf"

usage() {
	echo "${0} [-c /path/to/configuration/file]"
	exit 1
}

diskbuild() {}
imagebuild() {}
pushami() {}

main() {
	while getopts "c:" arg; do
		case ${arg} in
			c)
				CONF=${OPTARG}
				;;
			*)
				usage
				;;
		esac
	done
	shift $(( ${OPTIND} - 1 ))
	. ${CONF}
}

main "$@"

