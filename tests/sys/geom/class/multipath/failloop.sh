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

. $(atf_get_srcdir)/conf.sh

# See also https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=178473
atf_test_case failloop cleanup
failloop_head()
{
	atf_set "descr" "A persistent failure in the provider should not cause an infinite loop, nor restore any providers that were faulted by the same bio"
	atf_set "require.user" "root"
	atf_set "require.config" "allow_sysctl_side_effects"
}
failloop_body()
{
	sysctl -n kern.geom.notaste > kern.geom.notaste.txt
	load_gnop
	load_gmultipath
	load_dtrace

	md0=$(alloc_md)
	md1=$(alloc_md)
	name=$(mkname)
	atf_check gnop create /dev/${md0}
	atf_check gnop create /dev/${md1}
	atf_check -s exit:0 gmultipath create "$name" ${md0}.nop ${md1}.nop
	sysctl kern.geom.notaste=1

	atf_check gnop configure -r 100 -w 100  ${md0}.nop
	atf_check gnop configure -r 100 -w 100  ${md1}.nop
	dd_status=`dtrace \
		-o restore_count \
		-i 'geom:multipath:config:restore {@restore = count()}' \
		-c "dd if=/dev/zero of=/dev/multipath/"$name" bs=4096 count=1" \
		2>&1 | awk '/exited with status/ {print $NF}'`
	if [ ! -f restore_count ]; then
		atf_fail "dtrace didn't execute successfully"
	fi
	# The dd command should've failed ...
	atf_check_equal 1 $dd_status
	# and triggered 1 or 2 path restores
	if [ `cat restore_count` -gt 2 ]; then
		atf_fail "gmultipath restored paths too many times"
	fi
}
failloop_cleanup()
{
	if [ -f kern.geom.notaste.txt ]; then
		sysctl kern.geom.notaste=`cat kern.geom.notaste.txt`
	fi
	common_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case failloop
}
