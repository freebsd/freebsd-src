# $FreeBSD$

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_tools cc_int cc_drv cpp cc1 cc cc1obj cc1plus c++ c++filt doc

.if !defined(NO_OBJC)
SUBDIR+= cc1obj
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= f77 f771 f77doc
.endif

.include <bsd.subdir.mk>
