# $FreeBSD: src/usr.sbin/pw/Makefile,v 1.17 2001/07/20 06:20:15 obrien Exp $

PROG=	pw
MAN=	pw.conf.5 pw.8
SRCS=	pw.c pw_conf.c pw_user.c pw_group.c pw_log.c pw_nis.c pw_vpw.c \
	grupd.c pwupd.c fileupd.c edgroup.c psdate.c \
	bitmap.c cpdir.c rm_r.c

LDADD=	-lcrypt -lutil
DPADD=	${LIBCRYPT} ${LIBUTIL}

.include <bsd.prog.mk>
