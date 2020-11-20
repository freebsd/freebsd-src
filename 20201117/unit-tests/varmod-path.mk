# $NetBSD: varmod-path.mk,v 1.3 2020/08/23 08:10:49 rillig Exp $
#
# Tests for the :P variable modifier, which looks up the path for a given
# target.
#
# The phony target does not have a corresponding path, therefore ... oops,
# as of 2020-08-23 it is nevertheless resolved to a path.  This is probably
# unintended.
#
# The real target is located in a subdirectory, and its full path is returned.
# If it had been in the current directory, the difference between its path and
# its name would not be visible.
#
# The enoent target does not exist, therefore the target name is returned.

.MAIN: all

_!=	rm -rf varmod-path.subdir
_!=	mkdir varmod-path.subdir
_!=	> varmod-path.subdir/varmod-path.phony
_!=	> varmod-path.subdir/varmod-path.real

# To have an effect, this .PATH declaration must be after the directory is created.
.PATH: varmod-path.subdir

varmod-path.phony: .PHONY
varmod-path.real:

all: varmod-path.phony varmod-path.real
	@echo ${varmod-path.phony:P}
	@echo ${varmod-path.real:P}
	@echo ${varmod-path.enoent:P}

.END:
	@rm -rf varmod-path.subdir
