# $NetBSD: job-flags.mk,v 1.2 2020/11/14 13:17:47 rillig Exp $
#
# Tests for Job.flags, which are controlled by special source dependencies
# like .SILENT or .IGNORE, as well as the command line options -s or -i.

.MAKEFLAGS: -j1

all: silent .WAIT ignore .WAIT ignore-cmds

.BEGIN:
	@echo $@

silent: .SILENT .PHONY
	echo $@

ignore: .IGNORE .PHONY
	@echo $@
	true in $@
	false in $@
	@echo 'Still there in $@'

ignore-cmds: .PHONY
	# This node is not marked .IGNORE; individual commands can be switched
	# to ignore mode by prefixing them with a '-'.
	-false without indentation
	# This also works if the '-' is indented by a space or a tab.
	# Leading whitespace is stripped off by ParseLine_ShellCommand.
	 -false space
		-false tab

.END:
	@echo $@
