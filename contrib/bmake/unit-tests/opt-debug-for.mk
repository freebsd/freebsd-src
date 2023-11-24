# $NetBSD: opt-debug-for.mk,v 1.4 2020/10/05 19:27:48 rillig Exp $
#
# Tests for the -df command line option, which adds debug logging for
# parsing and evaluating .for loops.

.MAKEFLAGS: -df

# XXX: In the debug log, the "new loop 2" appears out of context.
# There should be a "begin loop 1" before, and all these messages should
# contain line number information.
#
# XXX: The "loop body" should print the nesting level as well.
#
# XXX: It is hard to extract any information from the debug log since
# the "begin" and "end" events are not balanced and the nesting level
# is not printed consistently.  It would also be helpful to mention the
# actual substitutions, such as "For 1: outer=b".
#
.for outer in a b
.  for inner in 1 2
VAR.${outer}${inner}=	value
.  endfor
.endfor

all:
	@:;
