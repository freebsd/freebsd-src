# $Id: Makefile,v 1.2 1996/12/17 14:15:33 davidn Exp $

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

BINOWN=	root
BINMODE=0555

.include <bsd.prog.mk>
