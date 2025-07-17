# $NetBSD: opt-debug-parse.mk,v 1.8 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the -dp command line option, which adds debug logging about
# makefile parsing.

.MAKEFLAGS: -dp

# TODO: Implementation

# Before parse.c 1.639 from 2022-01-08, PrintStackTrace and other diagnostics
# printed a wrong line number, using the last line before the loop body, while
# it should rather be the line number where the .for loop starts.
#
# Before parse.c 1.643 from 2022-01-08, PrintStackTrace tried to be too clever
# by merging stack trace entries, printing confusing line numbers as a result.
.for \
    var \
    in \
    value
# expect+1: trace with multi-line .for loop head
.info trace with multi-line .for loop head
.endfor

# Before parse.c 1.641 from 2022-01-08, the debug log said it returned to
# the line of the '.include' instead of the line following it.
.include "/dev/null"


# In .for loops with multiple variables, the variable details are included in
# the stack trace, just as with a single variable.
.for a b c in 1 2 3 ${:U4 5 6}
# expect+2: trace
# expect+1: trace
.info trace
.endfor


.MAKEFLAGS: -d0

all: .PHONY
