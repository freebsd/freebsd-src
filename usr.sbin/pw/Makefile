# $FreeBSD$

PROG=	pw
SRCS=	pw.c pw_conf.c pw_user.c pw_group.c pw_log.c pw_nis.c pw_vpw.c \
	grupd.c pwupd.c fileupd.c edgroup.c psdate.c \
	bitmap.c cpdir.c rm_r.c

MAN=	pw.conf.5 pw.8

#RND=	-DUSE_MD5RAND
CFLAGS+= -W -Wall
LDADD=	-lcrypt -lutil
DPADD=	${LIBCRYPT} ${LIBUTIL}

.include <bsd.prog.mk>
