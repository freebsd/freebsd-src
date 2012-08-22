
.if defined(.PARSEDIR)
.if ${.MAKE.MODE:Mmeta*} != ""
.if !empty(SUBDIR) && !defined(LIB) && !defined(PROG) && ${.MAKE.MAKEFILES:M*bsd.prog.mk} == ""
.if ${.MAKE.MODE:Mleaf*} != ""
# we only want leaf dirs to build in meta mode... and we are not one
.MAKE.MODE = normal
.endif
.endif
.endif
.endif

.if ${MACHINE} == "host"
HOST_CC?= /usr/bin/cc
HOST_CFLAGS+= -DHOSTPROG
CC= ${HOST_CC}
CFLAGS+= ${HOST_CFLAGS}
.endif
