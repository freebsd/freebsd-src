# $NetBSD: parse.mk,v 1.3 2022/07/24 20:25:23 rillig Exp $
#
# Test those parts of the parsing that do not belong in any of the other
# categories.

# expect+1: Makefile appears to contain unresolved CVS/RCS/??? merge conflicts
<<<<<< old

# No diagnostic since the following line is parsed as a variable assignment,
# even though the variable name is empty.  See also varname-empty.mk.
====== middle

# expect+1: Makefile appears to contain unresolved CVS/RCS/??? merge conflicts
>>>>>> new


# Since parse.c 1.578 from 2021-12-14 and before parse.c 1.681 from
# 2022-07-24, if a line of a makefile could only be a dependency specification
# but didn't contain any of the dependency operators ':', '!', '::' and its
# expansion ended with a space, make read a single byte from the memory beyond
# the expanded line's terminating '\0'.
#
# https://bugs.freebsd.org/265119
one-target ${:U }
