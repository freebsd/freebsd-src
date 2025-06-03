# $NetBSD: cmd-interrupt.mk,v 1.5 2024/07/13 15:10:06 rillig Exp $
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

all: clean-before
all: interrupt-ordinary
all: interrupt-phony
all: interrupt-precious
all: interrupt-compat
all: clean-after

clean-before clean-after: .PHONY
	@rm -f cmd-interrupt-ordinary cmd-interrupt-phony
	@rm -f cmd-interrupt-precious cmd-interrupt-compat

interrupt-ordinary:
	@${.MAKE} ${MAKEFLAGS} -f ${MAKEFILE} cmd-interrupt-ordinary || true
	# The ././ is necessary to work around the file cache.
	@echo ${.TARGET}: ${exists(././cmd-interrupt-ordinary) :? error : ok }

interrupt-phony: .PHONY
	@${.MAKE} ${MAKEFLAGS} -f ${MAKEFILE} cmd-interrupt-phony || true
	# The ././ is necessary to work around the file cache.
	@echo ${.TARGET}: ${exists(././cmd-interrupt-phony) :? ok : error }

interrupt-precious: .PRECIOUS
	@${.MAKE} ${MAKEFLAGS} -f ${MAKEFILE} cmd-interrupt-precious || true
	# The ././ is necessary to work around the file cache.
	@echo ${.TARGET}: ${exists(././cmd-interrupt-precious) :? ok : error }

interrupt-compat:
	@${MAKE} -f ${MAKEFILE} cmd-interrupt-compat || true
	@echo ${.TARGET} ${exists(././cmd-interrupt-compat) :? expected-fail : unexpected-ok }

cmd-interrupt-ordinary:
	> ${.TARGET}
	@kill -INT ${.MAKE.PID}

cmd-interrupt-phony: .PHONY
	> ${.TARGET}
	@kill -INT ${.MAKE.PID}

cmd-interrupt-precious: .PRECIOUS
	> ${.TARGET}
	@kill -INT ${.MAKE.PID}

# When the make process (and not the process group) is interrupted in compat
# mode, it first tries to interrupt the process group of the currently running
# child command, but that fails since there is no such process group, rather
# the child command runs in the same process group as make itself.  The child
# command then continues, and after sleeping a bit creates the target file.
cmd-interrupt-compat:
	@kill -INT ${.MAKE.PID} && sleep 1 && > ${.TARGET}
