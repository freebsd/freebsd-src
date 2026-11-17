# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Maxim Konovalov <maxim@FreeBSD.org>
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

. $(atf_get_srcdir)/../common/utils.subr

atf_test_case "show_log" "cleanup"
show_log_head()
{
	atf_set descr \
	    'ipfw nat <name> show log uses the log formatter, not the config one'
	atf_set require.user root
}

show_log_body()
{
	firewall_init "ipfw"
	nat_init "ipfw"

	jname="ipfw_nat_show_log"
	vnet_mkjail ${jname}

	# A nat instance with logging enabled but no traffic: its log
	# buffer is empty.
	jexec ${jname} ipfw nat 123 config ip 192.0.2.1 log

	# "show config" of a single instance prints its configuration.
	atf_check -o match:"ipfw nat 123 config" \
	    jexec ${jname} ipfw nat 123 show config

	# "show log" of an empty log prints nothing.  Before the fix the
	# single-instance path ignored the command and used the config
	# formatter, so it printed the config line here instead.
	atf_check -o empty jexec ${jname} ipfw nat 123 show log
}

show_log_cleanup()
{
	firewall_cleanup "ipfw"
}

atf_init_test_cases()
{
	atf_add_test_case "show_log"
}
