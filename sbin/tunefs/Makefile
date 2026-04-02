#	@(#)Makefile	8.1 (Berkeley) 6/5/93
.include <src.opts.mk>

PACKAGE=ufs
PROG=	tunefs
SRCS=	tunefs.c getmntopts.c
LIBADD=	ufs
MAN=	tunefs.8

MOUNT=	${SRCTOP}/sbin/mount
CFLAGS+=	-I${MOUNT}
.PATH:	${MOUNT}

HAS_TESTS=
SUBDIR.${MK_TESTS}=	tests

.include <bsd.prog.mk>
