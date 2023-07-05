# $NetBSD: varmod-path.mk,v 1.4 2023/05/10 15:53:32 rillig Exp $
#
# Tests for the :P variable modifier, which looks up the path for a given
# target.
#
# The phony target does not have a corresponding path, therefore ... oops,
# as of 2020-08-23 it is nevertheless resolved to a path.  This is probably
# unintended.
#
# In this test, the real target is located in a subdirectory, and its full
# path is returned.  If it had been in the current directory, the difference
# between its path and its name would not be visible.
#
# The enoent target does not exist, therefore the plain name of the target
# is returned.

.MAIN: all

_!=	rm -rf varmod-path.subdir
_!=	mkdir varmod-path.subdir
_!=	> varmod-path.subdir/varmod-path.phony
_!=	> varmod-path.subdir/varmod-path.real

# To have an effect, this .PATH declaration must be processed after the
# directory has been created.
.PATH: varmod-path.subdir

varmod-path.phony: .PHONY
varmod-path.real:

all: varmod-path.phony varmod-path.real
	@echo ${varmod-path.phony:P}
	@echo ${varmod-path.real:P}
	@echo ${varmod-path.enoent:P}

.END:
	@rm -rf varmod-path.subdir
