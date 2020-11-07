# $NetBSD: cmd-interrupt.mk,v 1.2 2020/08/28 18:16:22 rillig Exp $
#
# Tests for interrupting a command.
#
# If a command is interrupted (usually by the user, here by itself), the
# target is removed.  This is to avoid having an unfinished target that
# would be newer than all of its sources and would therefore not be
# tried again in the next run.
#
# This happens for ordinary targets as well as for .PHONY targets, even
# though the .PHONY targets usually do not correspond to a file.
#
# To protect the target from being removed, the target has to be marked with
# the special source .PRECIOUS.  These targets need to ensure for themselves
# that interrupting them does not leave an inconsistent state behind.
#
# See also:
#	CompatDeleteTarget

all: clean-before interrupt-ordinary interrupt-phony interrupt-precious clean-after

clean-before clean-after: .PHONY
	@rm -f cmd-interrupt-ordinary cmd-interrupt-phony cmd-interrupt-precious

interrupt-ordinary: .PHONY
	@${.MAKE} ${MAKEFLAGS} -f ${MAKEFILE} cmd-interrupt-ordinary || true
	# The ././ is necessary to work around the file cache.
	@echo ${.TARGET}: ${exists(././cmd-interrupt-ordinary) :? error : ok }

interrupt-phony: .PHONY
	@${.MAKE} ${MAKEFLAGS} -f ${MAKEFILE} cmd-interrupt-phony || true
	# The ././ is necessary to work around the file cache.
	@echo ${.TARGET}: ${exists(././cmd-interrupt-phony) :? error : ok }

interrupt-precious: .PRECIOUS
	@${.MAKE} ${MAKEFLAGS} -f ${MAKEFILE} cmd-interrupt-precious || true
	# The ././ is necessary to work around the file cache.
	@echo ${.TARGET}: ${exists(././cmd-interrupt-precious) :? ok : error }

cmd-interrupt-ordinary:
	> ${.TARGET}
	@kill -INT ${.MAKE.PID}

cmd-interrupt-phony: .PHONY
	> ${.TARGET}
	@kill -INT ${.MAKE.PID}

cmd-interrupt-precious: .PRECIOUS
	> ${.TARGET}
	@kill -INT ${.MAKE.PID}
