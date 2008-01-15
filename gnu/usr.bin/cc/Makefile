# $FreeBSD: src/gnu/usr.bin/cc/Makefile,v 1.38 2004/11/07 22:49:20 kan Exp $

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_tools cc_int cc cc1 include protoize doc

.if !defined(NO_CPP)
SUBDIR+= cpp
.endif

.if !defined(NO_CXX)
SUBDIR+= cc1plus c++ c++filt
.if !defined(NO_COLLECT2)
#SUBDIR+= collect2
.endif
.endif

.if !defined(NO_OBJC)
SUBDIR+= cc1obj
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= f77 f771 f77doc
.endif

.if !defined(NO_GCOV)
SUBDIR+= gcov
.endif

.include <bsd.subdir.mk>
