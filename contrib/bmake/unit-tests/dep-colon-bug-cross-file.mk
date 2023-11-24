# $NetBSD: dep-colon-bug-cross-file.mk,v 1.5 2023/06/01 20:56:35 rillig Exp $
#
# Until 2020-09-25, the very last dependency group of a top-level makefile
# was not finished properly.  This made it possible to add further commands
# to that target.
#
# In pass 1, there is a dependency group at the bottom of the file.
# This dependency group is not finished properly.  Finishing the dependency
# group would add the OP_HAS_COMMANDS flag to the "all" target, thereby
# preventing any commands from being added later.
#
# After the file has been parsed completely, it is parsed again in pass 2.
# In this pass, another command is added to the "current dependency group",
# which was still the one from pass 1, which means it was possible to later
# add commands to an existing target, even across file boundaries.
#
# Oops, even worse.  Running this test in a make from 2020-09-25 or earlier
# on NetBSD 8.0 x86_64 with MALLOC_OPTIONS=JA produces this or a similar
# output:
#
#	make: cannot open ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ.
#
# The 'Z' means access to already freed memory; see jemalloc(3).  The cause
# for this is that in MainParseArgs, the command line arguments were not
# properly copied before storing them in global variables.

PASS?=	1

.if ${PASS} == 2
all:
# expect+1: warning: duplicate script for target "all" ignored
	: pass 2
.endif

.if ${PASS} == 1

PASS=	2
.MAKEFLAGS: -f ${.PARSEDIR:q}/${.PARSEFILE:q}

all:
# expect+1: warning: using previous script for "all" defined here
	: pass 1
.endif
