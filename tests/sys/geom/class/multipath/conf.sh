#!/bin/sh
# Copyright (c) 2019 Axcient
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

MD_DEVS="md.devs"
MULTIPATH_DEVS="multipath.devs"

alloc_md()
{
	local md

	md=$(mdconfig -a -t swap -s 1M) || atf_fail "mdconfig -a failed"
	echo ${md} >> $MD_DEVS
	echo ${md}
}

# Verify expected state.
# check_multipath_state <active_path> <geom_state> <prov0_state> <prov1_state> [prov2_state]
check_multipath_state()
{
	local want_active_path=$1
	local want_geom_state=$2
	local want_prov0_state=$3
	local want_prov1_state=$4
	local want_prov2_state=$5
	local geom_state
	local prov0_state
	local prov1_state
	local prov2_state

	geom_state=`gmultipath list "$name" | awk '/^State:/ {print $2}'`
	atf_check_equal "$want_geom_state" "$geom_state"
	prov0_state=`gmultipath list "$name" | awk '/1. Name: md[0-9]/ {trigger=1} /State:/ && trigger == 1 {print $2; trigger=0;}'`
	prov1_state=`gmultipath list "$name" | awk '/2. Name: md[0-9]/ {trigger=1} /State:/ && trigger == 1 {print $2; trigger=0;}'`
	prov2_state=`gmultipath list "$name" | awk '/3. Name: md[0-9]/ {trigger=1} /State:/ && trigger == 1 {print $2; trigger=0;}'`
	atf_check_equal "$want_active_path" "`gmultipath getactive "$name"`"
	atf_check_equal "$want_prov0_state" $prov0_state
	atf_check_equal "$want_prov1_state" $prov1_state
	if [ -n "$want_prov2_state" ]; then
		atf_check_equal "$want_prov2_state" $prov2_state
	fi
}

common_cleanup()
{
	name=$(cat $MULTIPATH_DEVS)
	if [ -n "$name" -a -c "/dev/multipath/$name" ]; then
		gmultipath destroy "$name"
		rm $MULTIPATH_DEVS
	fi
	if [ -f "$MD_DEVS" ]; then
		while read test_md; do
			gnop destroy -f ${test_md}.nop 2>/dev/null
			mdconfig -d -u $test_md 2>/dev/null
		done < $MD_DEVS
		rm $MD_DEVS
	fi
	true
}

load_dtrace()
{
	if ! kldstat -q -m sdt; then
		kldload sdt || atf_skip "could not load module for dtrace SDT"
	fi
}

load_gmultipath()
{
	if ! kldstat -q -m g_multipath; then
		geom multipath load || atf_skip "could not load module for geom multipath"
	fi
}

load_gnop()
{
	if ! kldstat -q -m g_nop; then
		geom nop load || atf_skip "could not load module for geom nop"
	fi
}

mkname()
{
	mktemp -u mp.XXXXXX | tee $MULTIPATH_DEVS
}
