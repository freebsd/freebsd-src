# $NetBSD: opt-keep-going-multiple.mk,v 1.1 2020/12/07 01:32:04 rillig Exp $
#
# Tests for the -k command line option, which stops building a target as soon
# as an error is detected, but continues building the other, independent
# targets, as far as possible.
#
# Until 2020-12-07, the exit status of make depended only on the last of the
# main targets.  Even if the first few targets could not be made, make
# nevertheless exited with status 0.

.MAKEFLAGS: -k
.MAKEFLAGS: fail1 fail2 succeed

fail1 fail2: .PHONY
	false ${.TARGET}

succeed: .PHONY
	true ${.TARGET}

.END:
	: The end.
