#	$NetBSD: t_change.sh,v 1.4 2013/02/19 21:08:25 joerg Exp $
#
# Copyright (c) 2011 The NetBSD Foundation, Inc.
# All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

netserver=\
"rump_server -lrumpnet -lrumpnet_net -lrumpnet_netinet"
export RUMP_SERVER=unix://commsock

atf_test_case reject2blackhole cleanup
reject2blackhole_head()
{

	atf_set "descr" "Change a reject route to blackhole"
	atf_set "require.progs" "rump_server"
}

reject2blackhole_body()
{

	atf_check -s exit:0 ${netserver} ${RUMP_SERVER}

	atf_check -s exit:0 -o ignore \
	    rump.route add 207.46.197.32 127.0.0.1 -reject
	atf_check -s exit:0 -o match:UGHR -x \
	    "rump.route -n show -inet | grep ^207.46"
	atf_check -s exit:0 -o ignore \
	    rump.route change 207.46.197.32 127.0.0.1 -blackhole
	atf_check -s exit:0 -o match:' UGHBS ' -e ignore -x \
	    "rump.netstat -rn -f inet | grep ^207.46| grep ^207.46"
}

reject2blackhole_cleanup()
{

	env RUMP_SERVER=unix://commsock rump.halt
}

atf_init_test_cases()
{

	atf_add_test_case reject2blackhole
}
