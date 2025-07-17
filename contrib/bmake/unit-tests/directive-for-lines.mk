# $NetBSD: directive-for-lines.mk,v 1.5 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the line numbers that are reported in .for loops.
#
# Since parse.c 1.127 from 2007-01-01 and before parse.c 1.494 from
# 2020-12-19, the line numbers for the .info directives and error
# messages inside .for loops had been wrong since ParseGetLine skipped empty
# lines, even when collecting the lines for the .for loop body.

.for outer in a b

# comment \
# continued comment

.for inner in 1 2

# comment \
# continued comment

VAR= \
	multi-line

# expect+4: expect 23
# expect+3: expect 23
# expect+2: expect 23
# expect+1: expect 23
.info expect 23

.endfor

# comment \
# continued comment

# expect+2: expect 30
# expect+1: expect 30
.info expect 30

.endfor
