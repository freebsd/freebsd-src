# $FreeBSD: src/gnu/usr.bin/cc/Makefile,v 1.41.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
