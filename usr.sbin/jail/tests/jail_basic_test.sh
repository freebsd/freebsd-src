#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Michael Zhilin 
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

atf_test_case "basic" "cleanup"
atf_test_case "nested" "cleanup"
atf_test_case "commands" "cleanup"

basic_head()
{
	atf_set descr 'Basic jail test'
	atf_set require.user root
}

basic_body()
{
	# Create the jail
	atf_check -s exit:0 -o ignore jail -c name=basejail persist ip4.addr=192.0.1.1
	# Check output of jls
	atf_check -s exit:0 -o ignore jls
	atf_check -s exit:0 -o ignore jls -v
	atf_check -s exit:0 -o ignore jls -n
	# Stop jail
	atf_check -s exit:0 -o ignore jail -r basejail
	jail -c name=basejail persist ip4.addr=192.0.1.1
	# Stop jail by jid
	atf_check -s exit:0 -o ignore jail -r `jls -j basejail jid`
	# Recreate
	atf_check -s exit:0 -o ignore jail -cm name=basejail persist ip4.addr=192.0.1.1
	# Restart
	atf_check -s exit:0 -o ignore jail -rc name=basejail persist ip4.addr=192.0.1.1
}

basic_cleanup()
{
	jail -r basejail
}

nested_head()
{
	atf_set descr 'Hierarchical jails test'
	atf_set require.user root
}

nested_body()
{
	# Create the first jail
	jail -c name=basejail persist ip4.addr=192.0.1.1 children.max=1
	atf_check -s exit:0 -o empty \
		jexec basejail \
			jail -c name=nestedjail persist ip4.addr=192.0.1.1

	atf_check -s exit:1 -o empty -e inline:"jail: prison limit exceeded\n"\
		jexec basejail \
			jail -c name=secondnestedjail persist ip4.addr=192.0.1.1 
	# Check output of jls
	atf_check -s exit:0 -o ignore \
		jexec basejail jls
	atf_check -s exit:0 -o ignore \
		jexec basejail jls -v
	atf_check -s exit:0 -o ignore \
		jexec basejail jls -n
	# Create jail with no child - children.max should be 0 by default
	jail -c name=basejail_nochild persist ip4.addr=192.0.1.1
	atf_check -s exit:1 -o empty \
		-e inline:"jail: jail_set: Operation not permitted\n" \
		jexec basejail_nochild \
			jail -c name=nestedjail persist ip4.addr=192.0.1.1 
}

nested_cleanup()
{
	jail -r nestedjail
	jail -r basejail
	jail -r basejail_nochild
}

commands_head()
{
	atf_set descr 'Commands jail test'
	atf_set require.user root
}

commands_body()
{
	# exec.prestart
	atf_check -s exit:0 -o inline:"START\n" \
		jail -f $(atf_get_srcdir)/commands.jail.conf -qc basejail
	# exec.prestop by jailname
	atf_check -s exit:0 -o inline:"STOP\n" \
		jail -f $(atf_get_srcdir)/commands.jail.conf -qr basejail 
	# exec.prestop by jid
	jail -f $(atf_get_srcdir)/commands.jail.conf -qc basejail
	atf_check -s exit:0 -o inline:"STOP\n" \
		jail -f $(atf_get_srcdir)/commands.jail.conf -qr `jls -j basejail jid`
}

commands_cleanup() 
{
	jls -j basejail > /dev/null 2>&1
	if [ $? -e 0 ] 
	then
	    jail -r basejail
	fi
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "nested"
	atf_add_test_case "commands"
}
