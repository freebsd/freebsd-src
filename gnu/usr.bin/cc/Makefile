# $FreeBSD$

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_fbsd cc_tools cc_int cccp cc1 cc

.if !defined(NO_CPP)
SUBDIR+= cpp
.endif

.if !defined(NO_CXX)
SUBDIR+= cc1plus c++ c++filt doc
#.if !defined(NO_COLLECT2)
#SUBDIR+= collect2
#.endif
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
