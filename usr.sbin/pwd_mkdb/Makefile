#	@(#)Makefile	5.1 (Berkeley) 3/8/91

PROG=	pwd_mkdb
SRCS=	pw_scan.c pwd_mkdb.c
MAN8=	pwd_mkdb.8
.if defined (PW_COMPACT)
CFLAGS+=-DPW_COMPACT
.endif

.include <bsd.prog.mk>
