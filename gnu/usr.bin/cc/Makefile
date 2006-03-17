# $FreeBSD$

.include <bsd.own.mk>

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_tools cc_int cc cc1 include protoize doc

.if ${MK_CPP} != "no"
SUBDIR+= cpp
.endif

.if ${MK_CXX} != "no"
SUBDIR+= cc1plus c++ c++filt
.if ${MK_COLLECT2} != "no"
#SUBDIR+= collect2
.endif
.endif

.if ${MK_OBJC} != "no"
SUBDIR+= cc1obj
.endif

.if ${MK_FORTRAN} != "no"
SUBDIR+= f77 f771 f77doc
.endif

.if ${MK_GCOV} != "no"
SUBDIR+= gcov
.endif

.include <bsd.subdir.mk>
