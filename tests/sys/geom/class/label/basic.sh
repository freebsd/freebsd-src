#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2025 Brad Davis
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

class=label
. $(atf_get_srcdir)/../geom_subr.sh

atf_test_case create cleanup
create_head()
{
	atf_set "descr" "Create and verify GEOM labels"
	atf_set "require.user" "root"
}
create_body()
{
	geom_atf_test_setup

	f1=$(mktemp ${class}.XXXXXX)
	atf_check truncate -s 32M "$f1"
	attach_md md -t vnode -f "$f1"

	atf_check -s exit:0 -o match:"^Done." glabel create -v test "/dev/$md"
	atf_check -s exit:0 -o match:"^label/test     N/A  $md$" glabel status "/dev/$md"
	atf_check -s exit:0 -o match:"^/dev/label/test$" ls /dev/label/test
	atf_check -s exit:0 glabel stop test
}
create_cleanup()
{
	geom_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case create
}
