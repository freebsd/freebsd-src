#!/bin/sh
#
# Copyright (c) 2000 Alexandre Peixoto
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

# Change ipfw(8) rules with safety guarantees for remote operation
#
# Invoke this script to edit ${firewall_script}. It will call ${EDITOR},
# or vi(1) if the environment variable is not set, for you to edit
# ${firewall_script}, asks for confirmation and then run
# ${firewall_script}. You can then examine the output of ipfw list and
# confirm whether you want the new version or not.
#
# If no answer is received in 30 seconds, the previous
# ${firewall_script} is run, restoring the old rules (this assumes ipfw
# flush is present in it).
#
# If the new rules are confirmed, they'll replace ${firewall_script} and
# the previous ones will be copied to ${firewall_script}.{date}. A mail
# will also be sent to root with the unified diffs of the rule change.
#
# Non-approved rules are kept in ${firewall_script}.new, and you are
# offered the option of changing them instead of the present rules when
# you call this script.
#
# It is suggested improving this script by using some version control
# software.

if [ -r /etc/defaults/rc.conf ]; then
	. /etc/defaults/rc.conf
	source_rc_confs
elif [ -r /etc/rc.conf ]; then
	. /etc/rc.conf
fi

EDITOR=${EDITOR:-/usr/bin/vi}

get_yes_no() {
	while true
	do
		echo -n "$1 (Y/N) ? " 
		read -t 30 a
		if [ $? != 0 ]; then
			a="No";
		        return;
		fi
		case $a in
			[Yy]) a="Yes";
			      return;;
			[Nn]) a="No";
			      return;;
			*);;
		esac
	done
}

restore_rules() {
	nohup sh ${firewall_script} >/dev/null 2>&1 
	exit
}

if [ -f /etc/${firewall_script}.new ]; then
	get_yes_no "A new rules file already exists, do you want to use it"
	[ $a = 'No' ] && cp ${firewall_script} /etc/${firewall_script}.new
else 
	cp ${firewall_script} /etc/${firewall_script}.new
fi

trap restore_rules SIGHUP

${EDITOR} /etc/${firewall_script}.new

get_yes_no "Do you want to install the new rules"

[ $a = 'No' ] && exit

cat <<!
The rules will be changed now. If the message 'Type y to keep the new
rules' do not appear on the screen or the y key is not pressed in 30
seconds, the former rules will be restored.
The TCP/IP connections might be broken during the change. If so, restore
the ssh/telnet connection being used.
!

nohup sh /etc/${firewall_script}.new > /tmp/${firewall_script}.out 2>&1;
sleep 2;
get_yes_no "Would you like to see the resulting new rules"
[ $a = 'Yes' ] && ${EDITOR} /tmp/${firewall_script}.out
get_yes_no "Type y to keep the new rules"
[ $a != 'Yes' ] && restore_rules

DATE=`date "+%Y%m%d%H%M"`
cp ${firewall_script} /etc/${firewall_script}.$DATE
mv /etc/${firewall_script}.new ${firewall_script} 
cat <<!
The new rules are now default. The previous rules have been preserved in
the file /etc/${firewall_script}.$DATE
!
diff -F "^# .*[A-Za-z]" -u /etc/${firewall_script}.$DATE ${firewall_script} | mail -s "`hostname` Firewall rule change" root

