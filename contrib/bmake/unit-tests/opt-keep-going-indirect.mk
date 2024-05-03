# $NetBSD: opt-keep-going-indirect.mk,v 1.3 2024/04/02 15:05:15 rillig Exp $
#
# Tests for the -k command line option, which stops building a target as soon
# as an error is detected, but continues building the other, independent
# targets, as far as possible.
#
# History:
#	In 1993, the exit status for the option '-k' was always 0, even if a
#	direct or an indirect target failed.
#
#	Since 2000.12.30.02.05.21, the word '(continuing)' is missing in jobs
#	mode, both for direct as well as indirect targets.
#
#	Since 2001.10.16.18.50.12, the exit status for a direct failure in
#	compat mode is the correct 1, while jobs mode and indirect failures
#	still return the wrong exit status 0.  The number of empty lines
#	between the various error messages differs between the modes, for no
#	reason.
#
#	At 2006.11.17.22.07.39, the exit status for direct failures in both
#	modes and for indirect failures in jobs mode was fixed to the correct
#	1.  The exit status for indirect failures in compat mode is still the
#	wrong 0.  On the downside, a failed indirect target in jobs mode is no
#	longer listed as "not remade because of errors".
#
#	At 2016.08.26.23.28.39, the additional empty line for a direct failure
#	in compat mode was removed, making it consistent with a direct failure
#	in jobs mode.  This left only one inconsistency, in that indirect
#	failures in jobs mode (by far the most common when building large
#	projects) did not produce any empty line.
#
#	Since 2020.12.07.00.53.30, the exit status is consistently 1 for
#	failures in all 4 modes.
#
# Bugs:
#	The output in case of a failure needlessly differs between compat and
#	jobs mode.  As of 2022-02-12, compat mode outputs '(continuing)' while
#	jobs mode doesn't.  In compat mode, the output does not mention which
#	target failed.
#
# See also:
#	https://gnats.netbsd.org/49720

.PHONY: all direct indirect

# The 'set +e' was necessary in 2003, when the shell was run with '-e' by
# default.
# The 'env -i' prevents that the environment variable MAKEFLAGS is passed down
# to the child processes.
all:
	@echo 'direct compat'
	@set +e; env -i "PATH=$$PATH" ${MAKE} -r -f ${MAKEFILE} -k direct; echo "exited $$?"
	@echo

	@echo 'direct jobs'
	@set +e; env -i "PATH=$$PATH" ${MAKE} -r -f ${MAKEFILE} -k direct -j1; echo "exited $$?"
	@echo

	@echo 'indirect compat'
	@set +e; env -i "PATH=$$PATH" ${MAKE} -r -f ${MAKEFILE} -k indirect; echo "exited $$?"
	@echo

	@echo 'indirect jobs'
	@set +e; env -i "PATH=$$PATH" ${MAKE} -r -f ${MAKEFILE} -k indirect -j1; echo "exited $$?"
	@echo

indirect: direct
direct:
	false

# TODO: Mention the target that failed, maybe even the chain of targets.
# expect: direct compat
# expect: *** Error code 1 (continuing)
# expect: exited 1

# TODO: Add '(continuing)'.
# expect: direct jobs
# expect: *** [direct] Error code 1
# expect: exited 1

# TODO: Mention the target that failed, maybe even the chain of targets.
# expect: indirect compat
# expect: *** Error code 1 (continuing)
# expect: exited 1

# TODO: Add '(continuing)'.
# TODO: Add 'not remade because of errors'.
# expect: indirect jobs
# expect: *** [direct] Error code 1
# expect: exited 1
