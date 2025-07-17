# $NetBSD: opt-chdir.mk,v 1.7 2024/04/02 11:11:00 rillig Exp $
#
# Tests for the -C command line option, which changes the directory at the
# beginning.
#
# This option has been available since 2009-08-27.

.MAKEFLAGS: -d0			# switch stdout to line-buffered

all: chdir-root
all: chdir-nonexistent

# Changing to another directory is possible via the command line.
# In this test, it is the root directory since almost any other directory
# is not guaranteed to exist on every platform.
chdir-root: .PHONY .IGNORE
	@MAKE_OBJDIR_CHECK_WRITABLE=no ${MAKE} -C / -V 'cwd: $${.CURDIR}'

# Trying to change to a nonexistent directory exits immediately.
# Note: just because the whole point of /nonexistent is that it should
# not exist - doesn't mean it doesn't.
chdir-nonexistent: .PHONY .IGNORE
	@${MAKE} -C /nonexistent.${.MAKE.PID}
