# $NetBSD: job-output.mk,v 1.2 2025/06/13 06:13:20 rillig Exp $
#
# Tests for handling the output in parallel mode.

all: .PHONY
	@${MAKE} -f ${MAKEFILE} -j1 empty-lines
	@${MAKE} -f ${MAKEFILE} -j1 stdout-and-stderr
	@${MAKE} -f ${MAKEFILE} -j1 echo-on-stdout-and-stderr

# By sleeping for a second, the output of the child process is written byte
# by byte, to be consumed in small pieces by make.  No matter what the chunk
# size is, the empty lines must not be discarded.
empty-lines: .PHONY
	@echo begin $@
	@sleep 1
	@echo
	@sleep 1
	@echo
	@sleep 1
	@echo end $@

# In parallel mode, both stdout and stderr from the child process are
# collected in a local buffer and then written to make's stdout.
#
# expect: begin stdout-and-stderr
# expect: only stdout:
# expect: This is stdout.
# expect: This is stderr.
# expect: only stderr:
# expect: end stdout-and-stderr
stdout-and-stderr:
	@echo begin $@
	@echo only stdout:
	@${MAKE} -f ${MAKEFILE} echo-on-stdout-and-stderr 2>/dev/null
	@echo only stderr:
	@${MAKE} -f ${MAKEFILE} echo-on-stdout-and-stderr 1>/dev/null
	@echo end $@

echo-on-stdout-and-stderr: .PHONY
	@echo This is stdout.
	@echo This is stderr. 1>&2
