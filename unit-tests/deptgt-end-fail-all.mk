# $NetBSD: deptgt-end-fail-all.mk,v 1.2 2020/12/07 01:04:07 rillig Exp $
#
# Test whether the commands from the .END target are run even if there is
# an error before.  The manual page says "after everything else is done",
# which leaves room for interpretation.
#
# Until 2020-12-07, the .END node was made even if the main nodes had failed.
# This was not intended since the .END node had already been skipped if a
# dependency of the main nodes had failed, just not if one of the main nodes
# themselves had failed.  This inconsistency was not worth keeping.  To run
# some commands on error, use the .ERROR target instead, see deptgt-error.mk.

all: .PHONY
	: Making ${.TARGET} out of nothing.
	false

.END:
	: Making ${.TARGET} out of nothing.
	false
