#	$FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.PATH:	${.CURDIR}/../../lib/libc/gen		# for pw_scan.c
CFLAGS+= -I${.CURDIR}/../../lib/libc/gen	# for pw_scan.h

PROG=	pwd_mkdb
SRCS=	pw_scan.c pwd_mkdb.c
MAN8=	pwd_mkdb.8

.include <bsd.prog.mk>
