#
# $Id: Makefile,v 1.12 1999/04/13 17:56:28 markm Exp $
#

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_tools cc_int cc_drv cpp cc1 cc cc1obj cc1plus c++ c++filt doc

.if !defined(NO_FORTRAN)
SUBDIR+= f77
SUBDIR+= f771
.endif

.include <bsd.subdir.mk>
