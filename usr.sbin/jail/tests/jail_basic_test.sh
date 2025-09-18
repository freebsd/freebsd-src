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

atf_test_case "list" "cleanup"
list_head()
{
	atf_set descr 'Specify some jail parameters as lists'
	atf_set require.user root
}

list_body()
{
	if [ "$(sysctl -qn kern.features.vimage)" -ne 1 ]; then
		atf_skip "cannot create VNET jails"
	fi
	atf_check -o save:epair ifconfig epair create

	epair=$(cat epair)
	atf_check jail -c name=basejail vnet persist vnet.interface=${epair},${epair%a}b

	atf_check -o ignore jexec basejail ifconfig ${epair}
	atf_check -o ignore jexec basejail ifconfig ${epair%a}b
}

list_cleanup()
{
	jail -r basejail
	if [ -f epair ]; then
		ifconfig $(cat epair) destroy
	fi
}

atf_test_case "nested" "cleanup"
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

atf_test_case "commands" "cleanup"
commands_head()
{
	atf_set descr 'Commands jail test'
	atf_set require.user root
}

commands_body()
{
	cp "$(atf_get_srcdir)/commands.jail.conf" jail.conf
	echo "path = \"$PWD\";" >> jail.conf

	# exec.prestart (START) and exec.poststart (env)
	atf_check -o save:stdout -e empty \
		jail -f jail.conf -qc basejail

	# exec.prestart output is missing
	atf_check grep -qE '^START$' stdout
	# JID was not set in the exec.poststart env
	atf_check grep -qE '^JID=[0-9]+' stdout
	# JNAME was not set in the exec.poststart env
	atf_check grep -qE '^JNAME=basejail$' stdout
	# JPATH was not set in the exec.poststart env
	atf_check grep -qE "^JPATH=$PWD$" stdout

	# exec.prestop by jailname
	atf_check -s exit:0 -o inline:"STOP\n" \
		jail -f jail.conf -qr basejail
	# exec.prestop by jid
	jail -f jail.conf -qc basejail
	atf_check -s exit:0 -o inline:"STOP\n" \
		jail -f jail.conf -qr `jls -j basejail jid`
}

commands_cleanup()
{
	if jls -j basejail > /dev/null 2>&1; then
	    jail -r basejail
	fi
}

atf_test_case "jid_name_set" "cleanup"
jid_name_set_head()
{
	atf_set descr 'Test that one can set both the jid and name in a config file'
	atf_set require.user root
}

find_unused_jid()
{
	: ${JAIL_MAX=999999}

	# We'll start at a higher jid number and roll through the space until
	# we find one that isn't taken.  We start high to avoid racing parallel
	# activity for the 'next available', though ideally we don't have a lot
	# of parallel jail activity like that.
	jid=5309
	while jls -cj "$jid"; do
		if [ "$jid" -eq "$JAIL_MAX" ]; then
			atf_skip "System has too many jail, cannot find free slot"
		fi

		jid=$((jid + 1))
	done

	echo "$jid" | tee -a jails.lst
}
clean_jails()
{
	if [ ! -s jails.lst ]; then
		return 0
	fi

	while read jail; do
		if jls -c -j "$jail"; then
			jail -r "$jail"
		fi
	done < jails.lst
}

jid_name_set_body()
{
	local jid=$(find_unused_jid)

	echo "basejail" >> jails.lst
	echo "$jid { name = basejail; persist; }" > jail.conf
	atf_check -o match:"$jid: created" jail -f jail.conf -c "$jid"
	# Confirm that we didn't override the explicitly-set name with the jid
	# as the name.
	atf_check -o match:"basejail" jls -j "$jid" name
	atf_check -o match:"$jid: removed" jail -f jail.conf -r "$jid"

	echo "$jid { host.hostname = \"\${name}\"; persist; }" > jail.conf
	atf_check -o match:"$jid: created" jail -f jail.conf -c "$jid"
	# Confirm that ${name} expanded and expanded correctly to the
	# jid-implied name.
	atf_check -o match:"$jid" jls -j "$jid" host.hostname
	atf_check -o match:"$jid: removed" jail -f jail.conf -r "$jid"

	echo "basejail { jid = $jid; persist; }" > jail.conf
	atf_check -o match:"basejail: created" jail -f jail.conf -c basejail
	# Confirm that our jid assigment in the definition worked out and we
	# did in-fact create the jail there.
	atf_check -o match:"$jid" jls -j "basejail" jid
	atf_check -o match:"basejail: removed" jail -f jail.conf -r basejail
}

jid_name_set_cleanup()
{
	clean_jails
}

atf_test_case "param_consistency" "cleanup"
param_consistency_head()
{
	atf_set descr 'Test for consistency in jid/name params being set implicitly'
	atf_set require.user root
}

param_consistency_body()
{
	local iface jid

	echo "basejail" >> jails.lst

	# Most basic test: exec.poststart running a command without a jail
	# config.  This would previously crash as we only had the jid and name
	# as populated at creation time.
	atf_check jail -c path=/ exec.poststart="true" command=/usr/bin/true

	iface=$(ifconfig lo create)
	atf_check test -n "$iface"
	echo "$iface" >> interfaces.lst

	# Now do it again but exercising IP_VNET_INTERFACE, which is an
	# implied command that wants to use the jid or name.  This would crash
	# as neither KP_JID or KP_NAME are populated when a jail is created,
	# just as above- just at a different spot.
	atf_check jail -c \
		path=/ vnet=new vnet.interface="$iface" command=/usr/bin/true

	# Test that a jail that we only know by name will have its jid resolved
	# and added to its param set.
	echo "basejail {path = /; exec.prestop = 'echo STOP'; persist; }" > jail.conf

	atf_check -o ignore jail -f jail.conf -c basejail
	atf_check -o match:"STOP" jail -f jail.conf -r basejail

	# Do the same sequence as above, but use a jail with a jid-ish name.
	jid=$(find_unused_jid)
	echo "$jid {path = /; exec.prestop = 'echo STOP'; persist; }" > jail.conf

	atf_check -o ignore jail -f jail.conf -c "$jid"
	atf_check -o match:"STOP" jail -f jail.conf -r "$jid"

	# Ditto, but now we set a name for that jid-jail.
	echo "$jid {name = basejail; path = /; exec.prestop = 'echo STOP'; persist; }" > jail.conf

	atf_check -o ignore jail -f jail.conf -c "$jid"
	atf_check -o match:"STOP" jail -f jail.conf -r "$jid"

	# Confirm that we have a valid jid available in exec.poststop.  It's
	# probably debatable whether we should or not.
	echo "basejail {path = /; exec.poststop = 'echo JID=\$JID'; persist; }" > jail.conf
	atf_check -o ignore jail -f jail.conf -c basejail
	jid=$(jls -j basejail jid)

	atf_check -o match:"JID=$jid" jail -f jail.conf -r basejail

}

param_consistency_cleanup()
{
	clean_jails

	if [ -f "interfaces.lst" ]; then
		while read iface; do
			ifconfig "$iface" destroy
		done < interfaces.lst
	fi
}

atf_test_case "setaudit"
setaudit_head()
{
	atf_set descr 'Test that setaudit works in a jail when configured with allow.setaudit'
	atf_set require.user root
	atf_set require.progs setaudit
}

setaudit_body()
{
	# Try to modify the audit mask within a jail without
	# allow.setaudit configured.
	atf_check -s not-exit:0 -o empty -e not-empty jail -c name=setaudit_jail \
	    command=setaudit -m fr ls /
	# The command should succeed if allow.setaudit is configured.
	atf_check -s exit:0 -o ignore -e empty jail -c name=setaudit_jail \
	    allow.setaudit command=setaudit -m fr ls /
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "list"
	atf_add_test_case "nested"
	atf_add_test_case "commands"
	atf_add_test_case "jid_name_set"
	atf_add_test_case "param_consistency"
	atf_add_test_case "setaudit"
}
