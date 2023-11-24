#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2023 Gavin D. Howard and contributors.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

set -e

script="$0"

testdir=$(dirname "${script}")

# Just print the usage and exit with an error. This can receive a message to
# print.
# @param 1  A message to print.
usage() {
	if [ $# -eq 1 ]; then
		printf '%s\n\n' "$1"
	fi
	printf 'usage: %s dc_exec year [options...]\n' "$script"
	exit 1
}

. "$testdir/../../../scripts/functions.sh"

if test $# -lt 2
then
	usage "Not enough arguments; need 2"
fi

dc_exec="$1"
shift
check_exec_arg "$dc_exec"

year="$1"
shift

echo $year '
[
	ddsf
	[
		lfp
		[too early
		]P
		q
	]s@
	1583>@
	ddd19%1+sg100/1+d3*4/12-sx8*5+25/5-sz5*4/lx-10-sdlg11*20+lz+lx-30%
	d
	[30+]s@
	0>@d
	[
		[1+]s@
		lg11<@
	]s@
	25=@d
	[1+]s@
	24=@se44le-d
	[30+]s@
	21>@dld+7%-7+
	[March ]sm
	d
	[
		31-
		[April ]sm
	]s@
	31<@psnlmPpsn1z>p
]sp
lpx' | "$dc_exec" "$@" | tr '\012' ' '
echo ''
