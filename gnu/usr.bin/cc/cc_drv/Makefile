# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}

.if !defined(OSVERSION)
OSVERSION!=    /sbin/sysctl -n kern.osreldate
.endif

SRCS=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c
# Maintain backward compatibility
SRCS+= mkstemp.c

CFLAGS+=	-DPREFIX=\"/usr\"
CFLAGS+=	-DIN_GCC

LIB=		cc_drv
NOPROFILE=	YES
NOPIC=		YES

install:
	@true

${OUT_OBJ}.o ${OUT_OBJ}.so:	${OUT_FILE}

.include <bsd.lib.mk>
