#!/bin/sh
#
# Copyright (c) 2006 "David Kirchner" <dpk@dpk.net>. All rights reserved.
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
#
# Generates tables_linux.h
#

set -e

LC_ALL=C; export LC_ALL

if [ -z "$1" ]
then
	echo "usage: sh $0 include-dir [output-file]"
	exit 1
fi
include_dir=$1
if [ -n "$2" ]; then
	output_file="$2"
	output_tmp=$(mktemp -u)
	exec > "$output_tmp"
fi

all_headers=
#
# Generate a table C #definitions.  The including file can define the
# TABLE_NAME(n), TABLE_ENTRY(x), and TABLE_END macros to define what
# the tables map to.
#
gen_table()
{
	local name grep file excl filter
	name=$1
	grep=$2
	file=$3
	excl=$4

	if [ -z "$excl" ]; then
		filter="cat"
	else
		filter="egrep -v"
	fi
	cat <<_EOF_
TABLE_START(${name})
_EOF_
	if [ -e "${include_dir}/${file}" ]; then
		all_headers="${all_headers:+${all_headers} }${file}"
		egrep "^#[[:space:]]*define[[:space:]]+"${grep}"[[:space:]]*" \
			$include_dir/$file | ${filter} ${excl} | \
		awk '{ for (i = 1; i <= NF; i++) \
			if ($i ~ /define/) \
				break; \
			++i; \
			sub(/LINUX_/, "", $i); \
			printf "TABLE_ENTRY(LINUX_%s, %s)\n", $i, $i }'
	fi
cat <<_EOF_
TABLE_END

_EOF_
}

cat <<_EOF_
/* This file is auto-generated. */

_EOF_

gen_table "atflags"     "LINUX_AT_[A-Z_]+[[:space:]]+[0-9]+"             "compat/linux/linux_file.h"
gen_table "clockids"    "LINUX_CLOCK_[A-Z_]+[[:space:]]+[0-9]+"          "compat/linux/linux_time.h"
gen_table "clockflags"  "LINUX_TIMER_[A-Z_]+[[:space:]]+0x[0-9]+"        "compat/linux/linux_time.h"
gen_table "clockcpuids" "LINUX_CPUCLOCK_[A-Z_]+[[:space:]]+[0-9]+"       "compat/linux/linux_time.h"	"_MASK|_MAX"
gen_table "openflags"   "LINUX_O_[A-Z_]+[[:space:]]+[0-9]+"              "compat/linux/linux_file.h"	"O_RDONLY|O_RDWR|O_WRONLY|O_ACCMODE"
gen_table "sigprocmaskhow" "LINUX_SIG_[A-Z]+[[:space:]]+[0-9]+"          "compat/linux/linux.h"
gen_table "cloneflags"  "LINUX_CLONE_[A-Z_]+[[:space:]]+[[:alnum:]]+"    "compat/linux/linux_fork.h"	"LINUX_CLONE_LEGACY_FLAGS|LINUX_CLONE_CLEAR_SIGHAND|LINUX_CLONE_INTO_CGROUP|LINUX_CLONE_NEWTIME"

# Generate a .depend file for our output file
if [ -n "$output_file" ]; then
	depend_tmp=$(mktemp -u)
	{
		echo "$output_file: \\"
		echo "$all_headers" | tr ' ' '\n' | sort -u |
		    sed -e "s,^,	$include_dir/," -e 's,$, \\,'
		echo
	} > "$depend_tmp"
	if cmp -s "$output_tmp" "$output_file"; then
		rm -f "$output_tmp" "$depend_tmp"
	else
		mv -f "$depend_tmp" ".depend.${output_file}"
		mv -f "$output_tmp" "$output_file"
	fi
fi
