
.if !target(__${_this}__)
__${_this}__:

.if ${.MAKE.MODE:Mmeta*} != ""
.if !empty(SUBDIR) && !defined(LIB) && !defined(PROG) && ${.MAKE.MAKEFILES:M*bsd.prog.mk} == ""
.if ${.MAKE.MODE:Mleaf*} != ""
# we only want leaf dirs to build in meta mode... and we are not one
.MAKE.MODE = normal
.endif
.endif
.endif

# XXX: This should be combined with external compiler support in Makefile.inc1
# and local.meta.sys.mk (CROSS_TARGET_FLAGS)
.if ${MK_SYSROOT} == "yes" && !empty(SYSROOT) && ${MACHINE:Nhost*} != ""
CFLAGS_LAST+= --sysroot=${SYSROOT}
CXXFLAGS_LAST+= --sysroot=${SYSROOT}
LDADD+= --sysroot=${SYSROOT}
.elif ${MK_STAGING} == "yes"
ISYSTEM?= ${STAGE_INCLUDEDIR}
# no space after -isystem makes it easier to
# grep the flag out of command lines (in meta files) to see its value.
CFLAGS+= -isystem${ISYSTEM}
# XXX: May be needed for GCC to build with libc++ rather than libstdc++. See Makefile.inc1
#CXXFLAGS+= -std=gnu++11
#LDADD+= -L${STAGE_LIBDIR}/libc++
#CXXFLAGS+= -I${STAGE_INCLUDEDIR}/usr/include/c++/v1
LDADD+= -L${STAGE_LIBDIR}
.endif

.if ${MACHINE:Nhost*} == ""
.if ${.MAKE.DEPENDFILE:E:Nhost*} != ""
UPDATE_DEPENDFILE?= no
.endif
HOST_CFLAGS+= -DHOSTPROG
CFLAGS+= ${HOST_CFLAGS}
.endif

.-include "src.init.mk"
.-include <site.init.mk>
.-include "${.CURDIR}/local.init.mk"
.endif
