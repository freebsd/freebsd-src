#	$Id: Makefile,v 1.14 1996/08/20 23:54:06 peter Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/lib
.PATH:	${CVSDIR}/man

PROG=	cvs
MAN1=	cvs.1
MAN5=	cvs.5

SRCS=	add.c admin.c buffer.c checkin.c checkout.c classify.c client.c \
	commit.c create_adm.c cvsrc.c diff.c edit.c entries.c error.c \
	expand_path.c fileattr.c filesubr.c find_names.c hash.c history.c \
	ignore.c import.c lock.c log.c login.c logmsg.c main.c mkmodules.c \
	modules.c myndbm.c no_diff.c parseinfo.c patch.c rcs.c rcscmds.c \
	recurse.c release.c remove.c repos.c root.c rtag.c run.c scramble.c \
	server.c status.c subr.c tag.c update.c vers_ts.c version.c watch.c \
	wrapper.c zlib.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../lib -I${CVSDIR}/src -I${CVSDIR}/lib -DHAVE_CONFIG_H

DPADD+=	${LIBCVS} ${LIBGNUREGEX} ${LIBMD} ${LIBCRYPT} ${LIBZ}
LDADD+=	-lcvs -lgnuregex -lmd -lcrypt -lz

.include <bsd.prog.mk>
