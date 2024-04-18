#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 The FreeBSD Foundation
#
# This software was developed by Cybermancer Infosec <bofh@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#
# PROVIDE: freebsdci
# REQUIRE: LOGIN FILESYSTEMS
# KEYWORD: firstboot

# This script is used to run the firstboot CI tests on the first boot of a
# FreeBSD image. It is automatically disabled after the first boot.
#
# The script will run the firstboot CI tests and then shut down the system.
# The tests are run in the foreground so that the system can shut down
# immediately after the tests are finished.
#
# Default test types are full and smoke.  To run only the smoke tests, set
# freebsdci_type="smoke" in /etc/rc.conf.local or /etc/rc.conf.
# To run only the full tests, set freebsdci_type="full" in
# /etc/rc.conf.local or /etc/rc.conf.

. /etc/rc.subr

: ${freebsdci_enable:="NO"}

name="freebsdci"
desc="Run FreeBSD CI"
rcvar=freebsdci_enable
start_cmd="firstboot_ci_run"
stop_cmd=":"
os_arch=$(uname -p)

auto_shutdown()
{
	# XXX: Currently RISC-V kernels lack the ability to
	#      make qemu exit on shutdown. Reboot instead;
	#      it makes qemu exit too.
	case "$os_arch" in
		riscv64)
			shutdown -r now
		;;
		*)
			shutdown -p now
		;;
	esac
}

smoke_tests()
{
	echo
	echo "--------------------------------------------------------------"
	echo "BUILD sequence COMPLETED"
	echo "IMAGE sequence COMPLETED"
	echo "BOOT sequence COMPLETED"
	echo "INITIATING system SHUTDOWN"
	echo "--------------------------------------------------------------"
}

full_tests()
{
	# Currently this is a placeholder.
	# This will be used to add the full tests scenario those are run in
	# the CI system
	echo
	echo "--------------------------------------------------------------"
	echo "BUILD sequence COMPLETED"
	echo "IMAGE sequence COMPLETED"
	echo "BOOT sequence COMPLETED"
	echo "TEST sequence STARTED"
	echo "TEST sequence COMPLETED"
	echo "INITIATING system SHUTDOWN"
	echo "--------------------------------------------------------------"
}

firstboot_ci_run()
{
	if [ "$freebsdci_type" = "smoke" ]; then
		smoke_tests
	elif [ "$freebsdci_type" = "full" ]; then
		full_tests
	fi
	auto_shutdown
}

load_rc_config $name
run_rc_command "$1"
