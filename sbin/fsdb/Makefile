#	$NetBSD: Makefile,v 1.1.1.1 1995/10/08 23:08:36 thorpej Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.6 1997/02/22 14:32:24 peter Exp $

PROG=	fsdb
MAN8=	fsdb.8
SRCS=	fsdb.c fsdbutil.c \
	dir.c inode.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c preen.c setup.c utilities.c ffs_subr.c ffs_tables.c \
	opt_diagnostic.h
CFLAGS+= -I${.CURDIR}/../fsck -I${.OBJDIR}
CLEANFILES+=opt_diagnostic.h
LDADD+= -ledit -ltermcap
DPADD+=	${LIBEDIT} ${LIBTERMCAP}
.PATH:	${.CURDIR}/../fsck ${.CURDIR}/../../sys/ufs/ffs

opt_diagnostic.h:
	touch opt_diagnostic.h

.include <bsd.prog.mk>
