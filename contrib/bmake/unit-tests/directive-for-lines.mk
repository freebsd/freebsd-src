# $NetBSD: directive-for-lines.mk,v 1.6 2025/06/30 21:44:39 rillig Exp $
#
# Tests for the line numbers that are reported in .for loops.
#
# Since parse.c 1.127 from 2007-01-01 and before parse.c 1.494 from
# 2020-12-19, the line numbers for the .info directives and error
# messages inside .for loops had been wrong since ParseGetLine skipped empty
# lines, even when collecting the lines for the .for loop body.

# expect+21: This is line 31.
# expect+20: This is line 31.
# expect+26: This is line 38.

# expect+17: This is line 31.
# expect+16: This is line 31.
# expect+22: This is line 38.

.for outer in a b

# comment \
# continued comment

.for inner in 1 2

# comment \
# continued comment

VAR= \
	multi-line

.info This is line 31.

.endfor

# comment \
# continued comment

.info This is line 38.

.endfor
