# $NetBSD: archive-suffix.mk,v 1.3 2020/11/15 14:07:53 rillig Exp $
#
# Between 2020-08-23 and 2020-08-30, the below code produced an assertion
# failure in Var_SetWithFlags, triggered by Compat_Make, when setting the
# .IMPSRC of an archive node to its .TARGET.
#
# The code assumed that the .TARGET variable of every node would be set, but
# that is not guaranteed.
#
# Between 2016-03-15 and 2016-03-16 the behavior of the below code changed.
# Until 2016-03-15, it remade the target, starting with 2016-03-16 it says
# "`all' is up to date".

.SUFFIXES:
.SUFFIXES: .c .o

all:	lib.a(obj1.o)

.c.o:
	: making $@

obj1.c:
	: $@
