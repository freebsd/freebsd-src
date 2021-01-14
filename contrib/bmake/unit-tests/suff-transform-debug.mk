# $NetBSD: suff-transform-debug.mk,v 1.1 2020/11/22 23:45:20 rillig Exp $
#
# Test how the debug output of transformation rules looks.

.MAKEFLAGS: -dg1

.SUFFIXES: .a .c .cpp

.c.cpp .cpp.a:
	: Making ${.TARGET} from impsrc ${.IMPSRC} allsrc ${.ALLSRC}.

all:
