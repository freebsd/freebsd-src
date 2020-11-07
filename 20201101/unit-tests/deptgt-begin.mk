# $NetBSD: deptgt-begin.mk,v 1.3 2020/08/29 17:34:21 rillig Exp $
#
# Tests for the special target .BEGIN in dependency declarations,
# which is a container for commands that are run before any other
# commands from the shell lines.

.BEGIN:
	: $@

all:
	: $@

_!=	echo : parse time 1>&2
