# $NetBSD: deptgt-notparallel.mk,v 1.3 2021/12/13 23:38:54 rillig Exp $
#
# Tests for the special target .NOTPARALLEL in dependency declarations, which
# prevents the job module from doing anything in parallel, by setting the
# maximum jobs to 1.  This only applies to the current make, it is not
# exported to submakes.

.MAKEFLAGS: -j4

# Set opts.maxJobs back to 1.  Without this line, the output would be in
# random order, interleaved with separators like '--- 1 ---'.
.NOTPARALLEL:

all: 1 2 3 4 5 6 7 8
1 2 3 4 5 6 7 8: .PHONY
	: Making ${.TARGET} out of nothing.
