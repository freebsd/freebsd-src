# $FreeBSD$

.include <bsd.own.mk>

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_tools .WAIT \
	libiberty libcpp libdecnumber cc_int cc cc1 include doc

.if ${MK_CPP} != "no"
SUBDIR+= cpp
.endif

.if ${MK_CXX} != "no"
.if ${MK_GNUCXX} != "no"
SUBDIR+= cc1plus c++ 
.endif
# This should be moved into the above block once c++filt from elftoolchain or
# similar is provided.
SUBDIR+= c++filt
.endif

.if ${MK_GCOV} != "no"
SUBDIR+= gcov
.endif

SUBDIR_DEPEND_c++:=	libcpp libiberty
SUBDIR_DEPEND_c++filt=	libiberty
SUBDIR_DEPEND_cc=	libcpp libiberty
SUBDIR_DEPEND_cpp=	libcpp libiberty
SUBDIR_DEPEND_cc1plus=	cc_int libcpp libdecnumber libiberty
SUBDIR_DEPEND_cc1=	cc_int libcpp libdecnumber libiberty
SUBDIR_DEPEND_gcov=	libiberty

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
