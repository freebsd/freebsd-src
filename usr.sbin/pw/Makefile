# $Id: Makefile,v 1.5 1997/02/22 16:12:17 peter Exp $

PROG=	pw
SRCS=	pw.c pw_conf.c pw_user.c pw_group.c pw_log.c pw_nis.c \
	grupd.c pwupd.c fileupd.c edgroup.c psdate.c \
	bitmap.c cpdir.c rm_r.c

MAN5=	pw.conf.5
MAN8=	pw.8

#RND=	-DUSE_MD5RAND
CFLAGS+= -Wall $(CDB) $(RND)
LDADD=	-lcrypt
DPADD=	${LIBCRYPT}

BINMODE=0555

.include <bsd.prog.mk>
