# $FreeBSD: src/gnu/usr.bin/cc/Makefile,v 1.21.2.1 2000/07/04 05:32:51 obrien Exp $

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_fbsd cc_tools cc_int cccp cc1 cc

.if !defined(NO_CPP)
SUBDIR+= cpp
.endif

.if !defined(NO_CXX)
SUBDIR+= cc1plus c++ c++filt doc
.endif

.if !defined(NO_OBJC)
SUBDIR+= cc1obj
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= f77 f771 f77doc
.endif

.include <bsd.subdir.mk>
