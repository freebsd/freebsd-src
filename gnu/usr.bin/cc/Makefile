# $FreeBSD$

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_fbsd cc_tools cc_int cccp cpp cc1 cc cc1plus c++ c++filt doc

.if !defined(NO_OBJC)
SUBDIR+= cc1obj
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= f77 f771 f77doc
.endif

.include <bsd.subdir.mk>
