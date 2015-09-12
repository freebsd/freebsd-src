# $FreeBSD$

.include <src.opts.mk>

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR= cc_tools libiberty libcpp libdecnumber cc_int cc cc1 include

.if ${MK_CPP} != "no"
SUBDIR+= cpp
.endif

.if ${MK_CXX} != "no"
SUBDIR+= cc1plus c++
.endif

.if ${MK_GCOV} != "no"
SUBDIR+= gcov
.endif

.include <bsd.subdir.mk>
