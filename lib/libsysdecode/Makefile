# $FreeBSD$

.include <src.opts.mk>

PACKAGE=lib${LIB}
LIB=	sysdecode

SRCS=	errno.c ioctl.c syscallnames.c utrace.c
INCS=	sysdecode.h

CFLAGS+= -I${.CURDIR}/../../sys

MAN+=	sysdecode.3 \
	sysdecode_abi_to_freebsd_errno.3 \
	sysdecode_ioctlname.3 \
	sysdecode_syscallnames.3 \
	sysdecode_utrace.3
MLINKS+= sysdecode_abi_to_freebsd_errno.3 sysdecode_freebsd_to_abi_errno.3

CLEANFILES= ioctl.c

.if defined(COMPAT_32BIT)
CPP+=	-m32
.endif

.if ${MK_PF} != "no"
CFLAGS+=-DPF
.endif

# Workaround duplicate declarations in <netinet/ip_compat.h>
CFLAGS.gcc.ioctl.c+= -Wno-redundant-decls

# Workaround warning for unused ssi_cables[] in <dev/lmc/if_lmc.h>
CFLAGS.gcc.ioctl.c+= -Wno-unused

CFLAGS.gcc+=	${CFLAGS.gcc.${.IMPSRC}}

ioctl.c: mkioctls
	env MACHINE=${MACHINE} CPP="${CPP}" \
		/bin/sh ${.CURDIR}/mkioctls ${DESTDIR}${INCLUDEDIR} > ${.TARGET}

beforedepend: ioctl.c

.include <bsd.lib.mk>
