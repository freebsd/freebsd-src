#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Alexander Leidinger <netchild@FreeBSD.org>
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
# Service jail behaviour of run_rc_command().  Each case generates an rc.d
# script, drives it the way an operator would, and inspects the jail and the
# process that came out.
#
# Every case does that inside a chroot of its own, built in its ATF work
# directory, because the paths a service jail uses are absolute and would
# otherwise be the running system's: service(8) searches /etc/rc.d and
# ${local_startup} only, and a service jail re-enters its own script through
# it, so a fixture below the work directory is unreachable from inside the
# jail.  The chroot is read-only nullfs mounts of the system directories plus
# a copied /etc, and rc.subr's "jail -c path=/" then roots the service jail
# at it.  Nothing outside the work directory is written at any point.

# svcj_svcname
#	The service name for the current case.  It becomes a jail name and a
#	shell variable prefix, so it is reduced to alphanumerics.
svcj_svcname()
{
	echo "svcjt$(atf_get ident | tr -cd '[:alnum:]')"
}

# svcj_chroot
#	The root of this case's chroot, on the host.
svcj_chroot()
{
	echo "$(pwd)/svcjtestchroot"
}

# svcj_rcpath
#	This case's rc.d script, as seen from inside the chroot.  The chroot's
#	/etc is a copy, so using /etc/rc.d here cannot disturb the running
#	system's, and an rcorder(8) run over /etc/rc.d/* -- which another test
#	in this directory does -- never sees the fixture.
svcj_rcpath()
{
	echo "/etc/rc.d/$(svcj_svcname)"
}

# svcj_workdir
#	Where a case keeps its pid file and its markers, as seen from inside
#	the chroot.
svcj_workdir()
{
	echo "/var/run/svcjt.$(svcj_svcname)"
}

# svcj_hostrc / svcj_hostdir
#	The same two paths as seen from outside, for the assertions.
svcj_hostrc()
{
	echo "$(svcj_chroot)$(svcj_rcpath)"
}

svcj_hostdir()
{
	echo "$(svcj_chroot)$(svcj_workdir)"
}

# svcj_require
#	Requirements shared by every case: root, and room for one more jail.
svcj_require()
{
	local max cur

	if [ "$(id -u)" -ne 0 ]; then
		atf_skip "creating a service jail requires root"
	fi
	# rc.subr bounds itself against children.max only when it is already
	# jailed, so the check is made under the same condition.
	if [ "$(sysctl -n security.jail.jailed)" -ne 0 ]; then
		max=$(sysctl -n security.jail.children.max)
		cur=$(sysctl -n security.jail.children.cur)
		if [ "$max" -eq 0 ] || [ $((max - cur)) -eq 0 ]; then
			atf_skip "no child jail available inside this jail"
		fi
	fi
}

# svcj_mkchroot
#	Build this case's chroot.  Whether it can be built is decided by
#	trying, not by asking whether this is a jail: a jail with allow.mount
#	and the nullfs and devfs sub-options set can run these cases, and a
#	machine whose kernel has no nullfs cannot, jailed or not.
svcj_mkchroot()
{
	local c d

	c=$(svcj_chroot)
	mkdir -p "$c" || atf_fail "cannot create $c"
	# This becomes a root directory, and the kernel checks search
	# permission on it for every lookup an unprivileged process inside
	# makes.  At 0700 the ${name}_user cases fail in su(1).
	chmod 0755 "$c"
	: > "$(pwd)/svcj.mounts"

	for d in bin sbin lib libexec usr; do
		mkdir -p "$c/$d" || atf_fail "cannot create $c/$d"
		if ! mount -t nullfs -o ro "/$d" "$c/$d"; then
			svcj_umount
			atf_skip "cannot nullfs-mount /$d here"
		fi
		# Read-only is a safety property.  A recursive delete of this
		# tree with a mount still under it would run through the
		# mount.
		echo "$c/$d" >> "$(pwd)/svcj.mounts"
	done

	mkdir -p "$c/dev" || atf_fail "cannot create $c/dev"
	if ! mount -t devfs devfs "$c/dev"; then
		svcj_umount
		atf_skip "cannot mount devfs here"
	fi
	echo "$c/dev" >> "$(pwd)/svcj.mounts"

	mkdir -p "$c/etc" "$c/var/run" "$c/var/log" "$c/var/tmp" \
	    "$c/var/empty" "$c/tmp" "$c/root" || atf_fail "cannot populate $c"
	chmod 01777 "$c/tmp" "$c/var/tmp"

	# /etc is copied rather than mounted because the fixture, the pid
	# files and rc.conf all live in it.
	cp -a /etc/. "$c/etc/" || atf_fail "cannot copy /etc into $c"
	rm -rf "$c/etc/rc.conf.d"
	mkdir -p "$c/etc/rc.conf.d" || atf_fail "cannot create rc.conf.d"
	echo 'hostname="svcjtest"' > "$c/etc/rc.conf"
}

# svcj_umount
#	Unmount what svcj_mkchroot mounted, in reverse.  The verdict is what
#	is still mounted afterwards, not what umount(8) returned.
svcj_umount()
{
	local f d

	f="$(pwd)/svcj.mounts"
	[ -f "$f" ] || return 0
	for d in $(tail -r "$f"); do
		umount "$d" 2>/dev/null || umount -f "$d" 2>/dev/null
	done
	if mount | grep -q -F " on $(svcj_chroot)"; then
		return 1
	fi
	: > "$f"
	return 0
}

# svcj_fixture
#	Build the chroot and install this case's rc.d script into it, reading
#	the case-specific lines from standard input.  Those lines go after
#	load_rc_config, so that they win over rc.conf, and before
#	run_rc_command.
svcj_fixture()
{
	local svc c wd

	svc=$(svcj_svcname)

	# Recorded for the cleanup routine.
	echo "$svc" > "$(pwd)/svcj.name"

	svcj_mkchroot
	c=$(svcj_chroot)
	wd=$(svcj_workdir)
	mkdir -p "$c$wd" || atf_fail "cannot create $c$wd"
	chmod 0777 "$c$wd"

	{
		echo "#!/bin/sh"
		echo "# PROVIDE: $svc"
		echo ". /etc/rc.subr"
		echo "name=\"$svc\""
		echo "rcvar=\"${svc}_enable\""
		echo "load_rc_config \$name"
		echo "pidfile=\"$wd/$svc.pid\""
		cat
		echo "run_rc_command \"\$1\""
	} > "$(svcj_hostrc)"
	chmod 0755 "$(svcj_hostrc)"
}

# svcj_daemon_lines
#	The stock daemon used by most cases: one sleep(1), pid in ${pidfile}.
svcj_daemon_lines()
{
	echo 'command="/usr/sbin/daemon"'
	echo 'command_args="-p $pidfile -- /bin/sleep 300"'
	echo 'procname="/bin/sleep"'
}

# svcj_jid
#	The jid of this case's service jail, empty when there is none.
svcj_jid()
{
	jls -j "svcj-$(svcj_svcname)" jid 2>/dev/null
}

# svcj_pid
#	The pid the daemon recorded, empty when there is none yet.  daemon(8)
#	writes the file from a child, so a read right after the rc.d script
#	returns can lose the race; wait for it.
svcj_pid()
{
	local f pid junk

	f="$(svcj_hostdir)/$(svcj_svcname).pid"
	svcj_wait_file "$f" || return 1
	read pid junk < "$f"
	echo "$pid"
}

# svcj_wait_file <path>
#	True once the file exists and has content.  The bound is a
#	deadline rather than a delay: it costs nothing on a machine that
#	is quick, and 30s is far enough above what a loaded one needs
#	that expiry means the thing waited for is not coming.
svcj_wait_file()
{
	local i

	i=0
	while [ $i -lt 300 ]; do
		[ -s "$1" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	return 1
}

# svcj_gone <pid>
#	True once the pid is no longer alive.
svcj_gone()
{
	local i

	i=0
	while [ $i -lt 300 ]; do
		kill -0 "$1" 2>/dev/null || return 0
		sleep 0.1
		i=$((i + 1))
	done
	return 1
}

svcj_cleanup()
{
	local svc c pid junk

	[ -f "$(pwd)/svcj.name" ] || return 0
	read svc junk < "$(pwd)/svcj.name"
	c=$(svcj_chroot)

	jail -R "svcj-$svc" 2>/dev/null
	if [ -f "$c/var/run/svcjt.$svc/$svc.pid" ]; then
		read pid junk < "$c/var/run/svcjt.$svc/$svc.pid"
		if [ -n "$pid" ]; then
			kill -9 "$pid" 2>/dev/null
			svcj_gone "$pid"
		fi
	fi

	if svcj_umount; then
		rm -rf "$c"
	else
		# Leave the tree alone while anything is still mounted under
		# it.  Kyua's own work directory cleanup does not unmount
		# either, so a recursive delete here would run through the
		# mount.
		echo "svcj_test: still mounted under $c, not removing it" >&2
		mount | grep -F " on $c" >&2
		return 1
	fi
}

atf_test_case start_stop cleanup
start_stop_head()
{
	atf_set "descr" "A service with \${name}_svcj=YES starts inside" \
	    "svcj-\${name}, and stop removes both the process and the jail"
	atf_set "require.user" "root"
}
start_stop_body()
{
	local svc c rcs jid pid

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart

	jid=$(svcj_jid)
	[ -n "$jid" ] || atf_fail "no service jail svcj-$svc was created"
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"

	# The daemon really is in that jail, and not merely on the host.
	atf_check -o inline:"$jid\n" \
	    /bin/sh -c "ps -o jid= -p $pid | tr -d ' '"

	# And that jail is rooted at this case's chroot.
	atf_check -o inline:"$c\n" jls -j "svcj-$svc" path

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestop
	svcj_gone "$pid" || atf_fail "the daemon survived stop"
	[ -z "$(svcj_jid)" ] || atf_fail "svcj-$svc survived stop"
}
start_stop_cleanup()
{
	svcj_cleanup
}

atf_test_case all_enable cleanup
all_enable_head()
{
	atf_set "descr" "svcj_all_enable=YES puts a service that has no" \
	    "\${name}_svcj of its own into a service jail"
	atf_set "require.user" "root"
}
all_enable_body()
{
	local svc c rcs

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	svcj_all_enable="YES"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart

	[ -n "$(svcj_jid)" ] || atf_fail "no service jail svcj-$svc"
}
all_enable_cleanup()
{
	svcj_cleanup
}

atf_test_case all_enable_quiet cleanup
all_enable_quiet_head()
{
	atf_set "descr" "svcj_all_enable=YES does not make rc.subr complain" \
	    "about an unset \${name}_svcj"
	atf_set "require.user" "root"
}
all_enable_quiet_body()
{
	local svc c rcs

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	svcj_all_enable="YES"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	# The warning is the visible half of the defect above, and the half
	# an administrator sees once per service on every boot.
	atf_check -s exit:0 -o ignore -e not-match:'is not set properly' \
	    /usr/sbin/chroot "$c" "$rcs" onestart
}
all_enable_quiet_cleanup()
{
	svcj_cleanup
}

atf_test_case all_enable_rcconfd cleanup
all_enable_rcconfd_head()
{
	atf_set "descr" "svcj_all_enable=YES set in a configuration file" \
	    "enables a service jail"
	atf_set "require.user" "root"
}
all_enable_rcconfd_body()
{
	local svc c rcs

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	# The other svcj_all_enable cases set the variable inside the rc.d
	# script.  This one goes through load_rc_config and rc.conf.d.
	echo 'svcj_all_enable="YES"' > "$c/etc/rc.conf.d/$svc"

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart

	[ -n "$(svcj_jid)" ] || atf_fail "no service jail svcj-$svc"
}
all_enable_rcconfd_cleanup()
{
	svcj_cleanup
}

atf_test_case stop_as_user cleanup
stop_as_user_head()
{
	atf_set "descr" "stop terminates the process of a service jail whose" \
	    "\${name}_user is set"
	atf_set "require.user" "root"
}
stop_as_user_body()
{
	local svc c rcs pid

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	${svc}_user="nobody"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"
	atf_check -o inline:"nobody\n" \
	    /bin/sh -c "ps -o user= -p $pid | tr -d ' '"

	# The signal is sent from the host, as ${name}_user, against a process
	# in a subordinate jail.  Since 8a5ceebece03 that needs either
	# PRIV_SIGNAL_DIFFJAIL or allow.unprivileged_parent_tampering on the
	# target jail, and a service jail is created with neither.
	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestop
	svcj_gone "$pid" || atf_fail "the daemon survived stop"
	[ -z "$(svcj_jid)" ] || atf_fail "svcj-$svc survived stop"
}
stop_as_user_cleanup()
{
	svcj_cleanup
}

atf_test_case svcj_toggle_after_stop cleanup
svcj_toggle_after_stop_head()
{
	atf_set "descr" "\${name}_svcj may be changed between a stop and the" \
	    "next start, and the service ends up jailed"
	atf_set "require.user" "root"
}
svcj_toggle_after_stop_body()
{
	local svc c rcs pid

	svcj_require
	svc=$(svcj_svcname)

	# ${name}_svcj is only meaningful for a service that is not running:
	# it decides where the next start puts the process, and stop reads it
	# again to decide where to send the signal.  Changing it underneath a
	# running service therefore has no supported answer, and this case
	# pins the sequence that does -- stop, change, start.
	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="NO"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"
	[ -z "$(svcj_jid)" ] ||
	    atf_fail "a jail was created although ${svc}_svcj was NO"

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestop
	svcj_gone "$pid" || atf_fail "the unjailed daemon survived stop"

	atf_check -s exit:0 sed -i "" "s/${svc}_svcj=\"NO\"/${svc}_svcj=\"YES\"/" \
	    "$(svcj_hostrc)"
	rm -f "$(svcj_hostdir)/$svc.pid"

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"
	[ -n "$(svcj_jid)" ] ||
	    atf_fail "no jail although ${svc}_svcj is now YES"

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestop
	svcj_gone "$pid" || atf_fail "the jailed daemon survived stop"
	[ -z "$(svcj_jid)" ] || atf_fail "svcj-$svc survived stop"
}
svcj_toggle_after_stop_cleanup()
{
	svcj_cleanup
}

atf_test_case reload_as_user cleanup
reload_as_user_head()
{
	atf_set "descr" "reload signals the process of a service jail whose" \
	    "\${name}_user is set"
	atf_set "require.user" "root"
}
reload_as_user_body()
{
	local svc c rcs pid

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	extra_commands="reload"
	${svc}_svcj="YES"
	${svc}_user="nobody"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"

	# reload builds its signal with the same helper as stop and fails for
	# the same reason.  sleep(1) has no handler for SIGHUP, so the process
	# going away is the evidence that the signal was delivered.
	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onereload
	svcj_gone "$pid" || atf_fail "the daemon did not receive the signal"
}
reload_as_user_cleanup()
{
	svcj_cleanup
}

atf_test_case restart_cmd cleanup
restart_cmd_head()
{
	atf_set "descr" "A service jail's own restart_cmd is executed"
	atf_set "require.user" "root"
}
restart_cmd_body()
{
	local svc c rcs wd

	svcj_require
	svc=$(svcj_svcname)
	wd=$(svcj_workdir)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	restart_cmd="${svc}_restart"
	${svc}_restart()
	{
		: > "$wd/restart.ran"
	}
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart

	# A service jail's custom methods are dispatched through a case
	# statement whose restart branch is empty, so the method is skipped
	# and the exit status is still zero.
	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onerestart
	[ -f "$(svcj_hostdir)/restart.ran" ] ||
	    atf_fail "restart_cmd was not run"
}
restart_cmd_cleanup()
{
	svcj_cleanup
}

atf_test_case status_cmd cleanup
status_cmd_head()
{
	atf_set "descr" "A service jail's own status_cmd is executed"
	atf_set "require.user" "root"
}
status_cmd_body()
{
	local svc c rcs wd

	svcj_require
	svc=$(svcj_svcname)
	wd=$(svcj_workdir)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	status_cmd="${svc}_status"
	${svc}_status()
	{
		: > "$wd/status.ran"
	}
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestatus
	[ -f "$(svcj_hostdir)/status.ran" ] ||
	    atf_fail "status_cmd was not run"
}
status_cmd_cleanup()
{
	svcj_cleanup
}

atf_test_case restart_cmd_when_stopped cleanup
restart_cmd_when_stopped_head()
{
	atf_set "descr" "restart on a stopped service that has its own" \
	    "restart_cmd starts it, rather than failing in the absent jail"
	atf_set "require.user" "root"
}
restart_cmd_when_stopped_body()
{
	local svc c rcs wd jid pid

	svcj_require
	svc=$(svcj_svcname)
	wd=$(svcj_workdir)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	restart_cmd="${svc}_restart"
	${svc}_restart()
	{
		: > "$wd/restart.ran"
	}
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	# Nothing is running, so there is no service jail to enter.  A
	# restart is a stop and a start, and only the start half has
	# anything to do.
	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onerestart

	jid=$(svcj_jid)
	[ -n "$jid" ] || atf_fail "restart did not create svcj-$svc"
	pid=$(svcj_pid) || atf_fail "restart did not start the daemon"
	atf_check -o inline:"$jid\n" \
	    /bin/sh -c "ps -o jid= -p $pid | tr -d ' '"

	# The script's own method is for restarting something that runs.
	if [ -f "$(svcj_hostdir)/restart.ran" ]; then
		atf_fail "restart_cmd ran although nothing was running"
	fi
}
restart_cmd_when_stopped_cleanup()
{
	svcj_cleanup
}

atf_test_case status_cmd_when_stopped cleanup
status_cmd_when_stopped_head()
{
	atf_set "descr" "status on a stopped service that has its own" \
	    "status_cmd fails and creates no service jail"
	atf_set "require.user" "root"
}
status_cmd_when_stopped_body()
{
	local svc c rcs wd

	svcj_require
	svc=$(svcj_svcname)
	wd=$(svcj_workdir)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	status_cmd="${svc}_status"
	${svc}_status()
	{
		: > "$wd/status.ran"
	}
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	# Unlike restart there is nothing else to do here: a service that is
	# not running has no status to report from inside a jail that does
	# not exist.
	atf_check -s not-exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" \
	    "$rcs" onestatus
	[ -z "$(svcj_jid)" ] || atf_fail "status created svcj-$svc"
	if [ -f "$(svcj_hostdir)/status.ran" ]; then
		atf_fail "status_cmd ran outside the service jail"
	fi
}
status_cmd_when_stopped_cleanup()
{
	svcj_cleanup
}

atf_test_case stop_removes_dead_jail cleanup
stop_removes_dead_jail_head()
{
	atf_set "descr" "A service jail is gone once its only process is," \
	    "and stop then behaves like an unjailed service"
	atf_set "require.user" "root"
}
stop_removes_dead_jail_body()
{
	local svc c rcs pid

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"

	# A daemon that died on its own.
	atf_check -s exit:0 kill -9 "$pid"
	svcj_gone "$pid" || atf_fail "the daemon could not be killed"

	# The jail is already gone at this point, and not because stop did
	# anything: jail(8) clears the temporary "persist" parameter once
	# exec.start has run, so a service jail is reaped together with its
	# last process.  stop then takes its "not running?" branch and exits
	# 1, which is the same answer it gives for an unjailed service.
	[ -z "$(svcj_jid)" ] ||
	    atf_fail "svcj-$svc outlived its only process"

	atf_check -s exit:1 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestop
	[ -z "$(svcj_jid)" ] || atf_fail "svcj-$svc survived stop"
}
stop_removes_dead_jail_cleanup()
{
	svcj_cleanup
}

atf_test_case stop_orphan_jail_with_child cleanup
stop_orphan_jail_with_child_head()
{
	atf_set "descr" "stop removes the service jail when the process it" \
	    "tracks is gone but another process is still in the jail"
	atf_set "require.user" "root"
}
stop_orphan_jail_with_child_body()
{
	local svc c rcs wd pid

	svcj_require
	svc=$(svcj_svcname)
	wd=$(svcj_workdir)

	# The service leaves a second process behind in the jail, which is
	# what an ordinary daemon with a worker or a helper does.  Only the
	# first one is in ${pidfile}.
	svcj_fixture <<-EOF
	command="/usr/sbin/daemon"
	command_args="-p \$pidfile -- /bin/sh $wd/child.sh"
	procname="/bin/sleep"
	${svc}_svcj="YES"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)
	cat > "$(svcj_hostdir)/child.sh" <<-EOF
	sleep 600 >/dev/null 2>&1 &
	echo started > $wd/child.started
	exec sleep 300
	EOF

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"

	# ${pidfile} is written before the service has run, so the helper
	# is not there yet.  Killing the tracked process first would take
	# the jail with it and measure the single-process case instead.
	svcj_wait_file "$(svcj_hostdir)/child.started" ||
	    atf_fail "the helper never started"

	atf_check -s exit:0 kill -9 "$pid"
	svcj_gone "$pid" || atf_fail "the tracked process could not be killed"

	# Unlike the single-process case, the jail is still alive here: the
	# helper holds it open.  So this is the shape in which an orphaned
	# service jail is actually reachable.
	[ -n "$(svcj_jid)" ] ||
	    atf_fail "the helper did not keep svcj-$svc alive; " \
	        "this case is no longer testing what it says"

	/usr/sbin/chroot "$c" "$rcs" onestop >/dev/null 2>&1

	# stop returns from its "not running?" branch before reaching the
	# jail removal, so the jail and its helper survive -- and the next
	# start fails because svcj-${name} already exists.
	[ -z "$(svcj_jid)" ] || atf_fail "svcj-$svc survived stop"
}
stop_orphan_jail_with_child_cleanup()
{
	svcj_cleanup
}

atf_test_case audit_user cleanup
audit_user_head()
{
	atf_set "descr" "\${name}_audit_user works for a service in a" \
	    "service jail"
	atf_set "require.user" "root"
}
audit_user_body()
{
	local svc c rcs

	svcj_require
	svc=$(svcj_svcname)

	# Control: the same setaudit(8) invocation has to work outside, or
	# what the service jail does with it says nothing.  Where it does not
	# -- no audit support, or an enclosing jail without allow.setaudit --
	# it skips.
	setaudit -U -a root /usr/bin/true 2>/dev/null ||
	    atf_skip "setaudit(8) does not work here, so the service jail" \
	        "cannot be asked about it"

	# setaudit(8) is prefixed to the command inside the jail, where
	# PRIV_AUDIT_GETAUDIT and PRIV_AUDIT_SETAUDIT need allow.setaudit.
	# That is not in a jail's default allow set, so the service can only
	# work if the option below asks for it.
	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	${svc}_svcj_options="setaudit"
	${svc}_audit_user="root"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	svcj_pid > /dev/null || atf_fail "the daemon did not start"
}
audit_user_cleanup()
{
	svcj_cleanup
}

atf_test_case nice_negative cleanup
nice_negative_head()
{
	atf_set "descr" "A negative \${name}_nice is dropped for a service in" \
	    "a service jail"
	atf_set "require.user" "root"
}
nice_negative_body()
{
	local svc c rcs pid

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	${svc}_nice="-5"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"

	# Raising a process' priority needs PRIV_SCHED_SETPRIORITY, which a
	# jail is never granted.  nice(1) only warns when setpriority(2)
	# fails and execs the command regardless, so the request is dropped
	# and the service comes up at whatever its login class gives it --
	# the behaviour rc.conf(5) documents.  Asserted as "not the value
	# asked for" rather than as a literal, because the login class
	# supplies the priority actually in use.
	atf_check -o match:'^-?[0-9]+$' -o not-inline:"-5\n" \
	    /bin/sh -c "ps -o nice= -p $pid | tr -d ' '"
}
nice_negative_cleanup()
{
	svcj_cleanup
}

atf_test_case oomprotect cleanup
oomprotect_head()
{
	atf_set "descr" "\${name}_oomprotect protects a service that runs in" \
	    "a service jail"
	atf_set "require.user" "root"
}
oomprotect_body()
{
	local svc c rcs pid

	svcj_require
	# Decided by trying it, not by asking whether this is a jail:
	# protect(1) needs PRIV_VM_MADV_PROTECT, and what matters is whether
	# this process has it.
	protect /usr/bin/true 2>/dev/null ||
	    atf_skip "protect(1) is not permitted here"
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	${svc}_oomprotect="yes"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	pid=$(svcj_pid) || atf_fail "the daemon wrote no pid file"

	# protect(1) is skipped inside the jail and applied afterwards from
	# outside, against the pid rc.subr found in the jail.
	atf_check -o match:'^..1..... .......0$' -e empty \
	    ps -p "$pid" -ax -o flags,flags2
}
oomprotect_cleanup()
{
	svcj_cleanup
}

atf_test_case options_sysvipc_conflict cleanup
options_sysvipc_conflict_head()
{
	atf_set "descr" "Asking for both sysvipc and sysvipcnew fails without" \
	    "creating a jail"
	atf_set "require.user" "root"
}
options_sysvipc_conflict_body()
{
	local svc c rcs

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	${svc}_svcj_options="sysvipc sysvipcnew"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:1 -o match:'more than one sysvipc option' -e ignore \
	    /usr/sbin/chroot "$c" "$rcs" onestart
	[ -z "$(svcj_jid)" ] || atf_fail "a jail was created anyway"
}
options_sysvipc_conflict_cleanup()
{
	svcj_cleanup
}

atf_test_case options_unknown cleanup
options_unknown_head()
{
	atf_set "descr" "An unrecognised \${name}_svcj_options keyword is" \
	    "reported"
	atf_set "require.user" "root"
}
options_unknown_body()
{
	local svc c rcs

	svcj_require
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	${svc}_svcj_options="net_basic nosuchoption"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	# The keyword is reported and the service starts with the options
	# that were understood.  Whether a typo should instead be fatal is a
	# policy question; this pins today's answer so that changing it is
	# deliberate.
	atf_check -s exit:0 -e ignore \
	    -o match:'unknown service jail option: nosuchoption' \
	    /usr/sbin/chroot "$c" "$rcs" onestart
	[ -n "$(svcj_jid)" ] || atf_fail "no service jail svcj-$svc"
}
options_unknown_cleanup()
{
	svcj_cleanup
}

atf_test_case options_ipaddrs cleanup
options_ipaddrs_head()
{
	atf_set "descr" "\${name}_svcj_ipaddrs restricts the service jail to" \
	    "the listed addresses"
	atf_set "require.user" "root"
}
options_ipaddrs_body()
{
	local svc c rcs

	svcj_require
	# A jail can only hand an address to a child jail if it holds it
	# itself.
	ifconfig lo0 inet 2>/dev/null | grep -q 'inet 127\.0\.0\.1 ' ||
	    atf_skip "127.0.0.1 is not available here"
	svc=$(svcj_svcname)

	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	${svc}_svcj_options="netv4"
	${svc}_svcj_ipaddrs="127.0.0.1"
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	atf_check -o inline:"127.0.0.1\n" jls -j "svcj-$svc" ip4.addr
}
options_ipaddrs_cleanup()
{
	svcj_cleanup
}

atf_test_case extra_command_placement cleanup
extra_command_placement_head()
{
	atf_set "descr" "A script's own extra command runs on the host unless" \
	    "the script declares \${name}_\${cmd}_svcj_enable"
	atf_set "require.user" "root"
}
extra_command_placement_body()
{
	local svc c rcs wd jid

	svcj_require
	svc=$(svcj_svcname)
	wd=$(svcj_workdir)

	# The probe reports the jail id of the SERVICE's own process, as seen
	# from wherever the probe itself is running.  The kernel answers that
	# question relative to the asking process: fill_kinfo_proc() reports a
	# jail id of 0 for a process in the asker's own prison, and the real
	# prison id otherwise.  So the daemon reads as the jail's own jid from
	# outside and as 0 from inside -- which asserts that the method ran in
	# *the service's* jail, not merely somewhere jailed.
	# ${name}_${cmd}_svcj_enable is a declaration by the author of the rc
	# script, not a knob for the administrator, so both halves below set
	# it in the script itself.  It is read only on the branch that a
	# service with ${name}_svcj=YES reaches, which is why a script may
	# set it unconditionally.
	svcj_fixture <<-EOF
	$(svcj_daemon_lines)
	${svc}_svcj="YES"
	extra_commands="probe"
	probe_cmd="${svc}_probe"
	${svc}_probe_svcj_enable="NO"
	${svc}_probe()
	{
		ps -o jid= -p \$(cat "\$pidfile") | tr -d ' ' > "$wd/probe.out"
	}
	EOF
	c=$(svcj_chroot)
	rcs=$(svcj_rcpath)

	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    onestart
	jid=$(svcj_jid)
	[ -n "$jid" ] || atf_fail "no service jail svcj-$svc was created"
	[ "$jid" -ne 0 ] || atf_fail "svcj-$svc has jid 0, so this case " \
	    "cannot tell inside from outside"

	# Not declared: the method runs outside the service jail, and sees
	# the daemon carrying the jail's id.
	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    oneprobe
	atf_check -o inline:"$jid\n" cat "$(svcj_hostdir)/probe.out"

	# Declared: it runs inside, in the same prison as the daemon.
	rm -f "$(svcj_hostdir)/probe.out"
	atf_check -s exit:0 sed -i "" \
	    "s/${svc}_probe_svcj_enable=\"NO\"/${svc}_probe_svcj_enable=\"YES\"/" \
	    "$(svcj_hostrc)"
	atf_check -s exit:0 -o ignore -e ignore /usr/sbin/chroot "$c" "$rcs" \
	    oneprobe
	atf_check -o inline:"0\n" cat "$(svcj_hostdir)/probe.out"
}
extra_command_placement_cleanup()
{
	svcj_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case start_stop
	atf_add_test_case all_enable
	atf_add_test_case all_enable_quiet
	atf_add_test_case all_enable_rcconfd
	atf_add_test_case stop_as_user
	atf_add_test_case svcj_toggle_after_stop
	atf_add_test_case reload_as_user
	atf_add_test_case restart_cmd
	atf_add_test_case status_cmd
	atf_add_test_case restart_cmd_when_stopped
	atf_add_test_case status_cmd_when_stopped
	atf_add_test_case stop_removes_dead_jail
	atf_add_test_case stop_orphan_jail_with_child
	atf_add_test_case audit_user
	atf_add_test_case nice_negative
	atf_add_test_case oomprotect
	atf_add_test_case options_sysvipc_conflict
	atf_add_test_case options_unknown
	atf_add_test_case options_ipaddrs
	atf_add_test_case extra_command_placement
}
