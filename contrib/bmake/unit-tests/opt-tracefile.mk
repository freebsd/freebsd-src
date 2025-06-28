# $NetBSD: opt-tracefile.mk,v 1.6 2025/05/09 18:38:40 rillig Exp $
#
# Tests for the command line option '-T', which in jobs mode appends a trace
# record to a trace log whenever a job is started or completed.

all: .PHONY
	@rm -f opt-tracefile.log
	@${MAKE} -f ${MAKEFILE} -j1 -Topt-tracefile.log trace
	@awk '{ $$1 = "<timestamp>"; $$4 = "<make-pid>"; if (NF >= 7) $$7 = "<job-pid>"; print }' opt-tracefile.log
	@rm opt-tracefile.log

trace dependency1 dependency2: .PHONY
	@echo 'Making ${.TARGET} from ${.ALLSRC:S,^$,<nothing>,W}.'

trace: dependency1 dependency2
