# $FreeBSD$

PROG=	pw
SRCS=	pw.c pw_conf.c pw_user.c pw_group.c pw_log.c pw_nis.c pw_vpw.c \
	grupd.c pwupd.c fileupd.c edgroup.c psdate.c \
	bitmap.c cpdir.c rm_r.c

MAN=	pw.8 pw.conf.5
MAN5=	pw.conf.5
MAN8=	pw.8

#RND=	-DUSE_MD5RAND
CFLAGS+= -W -Wall $(CDB) $(RND)
LDADD=	-lcrypt -lutil
DPADD=	${LIBCRYPT} ${LIBUTIL}

BINMODE=0555

.include <bsd.prog.mk>
