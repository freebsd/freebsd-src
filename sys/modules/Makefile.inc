# $FreeBSD$

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

SUBDIR_PARALLEL=	yes
