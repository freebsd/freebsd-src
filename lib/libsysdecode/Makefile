# $FreeBSD$

.include <src.opts.mk>

LIB=	sysdecode

SRCS=	ioctl.c utrace.c
INCS=	sysdecode.h

MAN+=	sysdecode.3 \
	sysdecode_ioctlname.3 \
	sysdecode_utrace.3

CLEANFILES= ioctl.c

.if defined(COMPAT_32BIT)
CPP+=	-m32
.endif

.if ${MK_PF} != "no"
CFLAGS+=-DPF
.endif

# Workaround duplicate declarations in <netinet/ip_compat.h>
CFLAGS.gcc.ioctl.c+= -Wno-redundant-decls
CFLAGS.gcc+=	${CFLAGS.gcc.${.IMPSRC}}

ioctl.c: mkioctls
	env MACHINE=${MACHINE} CPP="${CPP}" \
		/bin/sh ${.CURDIR}/mkioctls ${DESTDIR}${INCLUDEDIR} > ${.TARGET}

beforedepend: ioctl.c

.include <bsd.lib.mk>
