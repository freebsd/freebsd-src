# $FreeBSD: src/gnu/usr.bin/cc/Makefile,v 1.41.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_tools libiberty libcpp libdecnumber cc_int cc cc1 include doc

.if ${MK_CPP} != "no"
SUBDIR+= cpp
.endif

.if ${MK_CXX} != "no"
SUBDIR+= cc1plus c++ c++filt
.endif

.if ${MK_OBJC} != "no"
SUBDIR+= cc1obj
.endif

.if ${MK_GCOV} != "no"
SUBDIR+= gcov
.endif

.include <bsd.subdir.mk>
