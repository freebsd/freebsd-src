# $NetBSD: parse.mk,v 1.2 2022/01/22 17:10:51 rillig Exp $
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
