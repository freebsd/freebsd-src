#
# $Id: Makefile,v 1.13 1999/04/18 09:34:01 obrien Exp $
#

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_tools cc_int cc_drv cpp cc1 cc cc1obj cc1plus c++ c++filt doc

.if !defined(NO_FORTRAN)
SUBDIR+= f77
SUBDIR+= f771
SUBDIR+= f77doc
.endif

.include <bsd.subdir.mk>
