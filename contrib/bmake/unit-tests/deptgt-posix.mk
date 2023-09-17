# $NetBSD: deptgt-posix.mk,v 1.4 2022/05/07 21:24:52 rillig Exp $
#
# Tests for the special target '.POSIX', which enables POSIX mode.
#
# As of 2022-04-18, when parsing the dependency line '.POSIX', the variable
# '%POSIX' is defined and <posix.mk> is included, if it exists.  Other than
# that, POSIX support is still incomplete, the exact set of supported features
# needs to be cross-checked with the POSIX specification.
#
# At the point of '.POSIX:', <sys.mk> has been loaded already, unless the
# option '-r' was given.  This means that an implementation of <posix.mk> must
# work both with and without the system rules from <sys.mk> being in effect.
#
# Implementation note: this test needs to run isolated from the usual tests
# directory to prevent unit-tests/posix.mk from interfering with the posix.mk
# from the system directory that this test uses; since at least 1997, the
# directive '.include <file>' has been looking in the current directory first
# before searching the file in the system search path, as described in
# https://gnats.netbsd.org/15163.
#
# See also:
#	https://pubs.opengroup.org/onlinepubs/9699919799/utilities/make.html

TESTTMP=	${TMPDIR:U/tmp}/make.test.deptgt-posix
SYSDIR=		${TESTTMP}/sysdir
MAIN_MK=	${TESTTMP}/main.mk
INCLUDED_MK=	${TESTTMP}/included.mk

all: .PHONY
.SILENT:

set-up-sysdir: .USEBEFORE
	mkdir -p ${SYSDIR}
	printf '%s\n' > ${SYSDIR}/sys.mk \
	    'CC=sys-cc' \
	    'SEEN_SYS_MK=yes'
	printf '%s\n' > ${SYSDIR}/posix.mk \
	    'CC=posix-cc'

check-is-posix: .USE
	printf '%s\n' >> ${MAIN_MK} \
		'.if $${CC} != "posix-cc"' \
		'.  error' \
		'.endif' \
		'.if $${%POSIX} != "1003.2"' \
		'.  error' \
		'.endif' \
		'all: .PHONY'

check-not-posix: .USE
	printf '%s\n' >> ${MAIN_MK} \
		'.if $${CC} != "sys-cc"' \
		'.  error' \
		'.endif' \
		'.if defined(%POSIX)' \
		'.  error' \
		'.endif' \
		'all: .PHONY'

check-not-seen-sys-mk: .USE
	printf '%s\n' >> ${MAIN_MK} \
	    '.if defined(SEEN_SYS_MK)' \
	    '.  error' \
	    '.endif'

run: .USE
	(cd "${TESTTMP}" && MAKEFLAGS=${MAKEFLAGS.${.TARGET}:Q} ${MAKE} \
	    -m "${SYSDIR}" -f ${MAIN_MK:T})
	rm -rf ${TESTTMP}

# If the main makefile has a '.for' loop as its first non-comment line, a
# strict reading of POSIX 2018 makes the makefile non-conforming.
all: after-for
after-for: .PHONY set-up-sysdir check-not-posix run
	printf '%s\n' > ${MAIN_MK} \
	    '# comment' \
	    '' \
	    '.for i in once' \
	    '.POSIX:' \
	    '.endfor'

# If the main makefile has an '.if' conditional as its first non-comment line,
# a strict reading of POSIX 2018 makes the makefile non-conforming.
all: after-if
after-if: .PHONY set-up-sysdir check-not-posix run
	printf '%s\n' > ${MAIN_MK} \
	    '# comment' \
	    '' \
	    '.if 1' \
	    '.POSIX:' \
	    '.endif'

# If the main makefile first includes another makefile and that included
# makefile tries to switch to POSIX mode, that's too late.
all: in-included-file
in-included-file: .PHONY set-up-sysdir check-not-posix run
	printf 'include included.mk\n' > ${MAIN_MK}
	printf '.POSIX:\n' > ${INCLUDED_MK}

# If the main makefile switches to POSIX mode in its very first line, before
# and comment lines or empty lines, that works.
all: in-first-line
in-first-line: .PHONY set-up-sysdir check-is-posix run
	printf '%s\n' > ${MAIN_MK} \
	    '.POSIX:'

# The only allowed lines before switching to POSIX mode are comment lines.
# POSIX defines comment lines as "blank lines, empty lines, and lines with
# <number-sign> ('#') as the first character".
all: after-comment-lines
after-comment-lines: .PHONY set-up-sysdir check-is-posix run
	printf '%s\n' > ${MAIN_MK} \
	    '# comment' \
	    '' \
	    '.POSIX:'

# Running make with the option '-r' skips the builtin rules from <sys.mk>.
# In that mode, '.POSIX:' just loads <posix.mk>, which works as well.
MAKEFLAGS.no-builtins=	-r
all: no-builtins
no-builtins: .PHONY set-up-sysdir check-is-posix check-not-seen-sys-mk run
	printf '%s\n' > ${MAIN_MK} \
	    '.POSIX:'
