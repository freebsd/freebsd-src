# $FreeBSD$

# Common flags to build lua related files

.include "defs.mk"

.if ${MACHINE_CPUARCH} == "amd64" && ${DO32:U0} == 0
CFLAGS+=	-fPIC
.endif

CFLAGS+=	-I${LUASRC} -I${LDRSRC} -I${LIBLUASRC}
CFLAGS+=	-DBOOT_LUA
