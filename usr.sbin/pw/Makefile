# $FreeBSD: src/usr.sbin/pw/Makefile,v 1.18.30.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	pw
MAN=	pw.conf.5 pw.8
SRCS=	pw.c pw_conf.c pw_user.c pw_group.c pw_log.c pw_nis.c pw_vpw.c \
	grupd.c pwupd.c fileupd.c edgroup.c psdate.c \
	bitmap.c cpdir.c rm_r.c

DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.include <bsd.prog.mk>
