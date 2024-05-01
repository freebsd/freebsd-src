.PATH: ${SRCTOP}/sys/skel

KMOD=	skel
SRCS=	skel.c

.include <bsd.kmod.mk>

CFLAGS+= -DKDTRACE_HOOKS
