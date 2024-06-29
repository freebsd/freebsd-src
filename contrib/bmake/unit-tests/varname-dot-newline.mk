# $NetBSD: varname-dot-newline.mk,v 1.7 2024/06/15 22:06:31 rillig Exp $
#
# Tests for the special .newline variable, which contains a single newline
# character (U+000A).


# https://austingroupbugs.net/view.php?id=1549 proposes:
# > After all macro expansion is complete, when an escaped <newline> is
# > found in a command line in a makefile, the command line that is executed
# > shall contain the <backslash>, the <newline>, and the next line, except
# > that the first character of the next line shall not be included if it is
# > a <tab>.
#
# The above quote assumes that each resulting <newline> character has a "next
# line", but that's not how the .newline variable works.
BACKSLASH_NEWLINE:=	\${.newline}


# Check that .newline is read-only

NEWLINE:=	${.newline}

.if make(try-to-modify)
# A '?=' assignment is fine.  This pattern can be used to provide the variable
# to older or other variants of make that don't know that variable.
.newline?=	fallback
# expect+1: Cannot overwrite ".newline" as it is read-only
.newline=	overwritten
# expect+1: Cannot append to ".newline" as it is read-only
.newline+=	appended
# expect+1: Cannot delete ".newline" as it is read-only
.undef .newline
.endif

.if ${.newline} != ${NEWLINE}
.  error The .newline variable can be overwritten.  It should be read-only.
.endif

all:
	@${MAKE} -f ${MAKEFILE} try-to-modify || true
	@echo 'first${.newline}second'
	@echo 'backslash newline: <${BACKSLASH_NEWLINE}>'
