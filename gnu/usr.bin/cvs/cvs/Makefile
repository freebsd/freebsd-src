#	$FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/lib
.PATH:	${CVSDIR}/man

PROG=	cvs
MAN1=	cvs.1
MAN5=	cvs.5
SRCS=	add.c admin.c checkin.c checkout.c classify.c client.c commit.c \
	create_adm.c cvsrc.c diff.c entries.c expand_path.c find_names.c \
	history.c ignore.c import.c lock.c log.c login.c logmsg.c main.c \
	modules.c no_diff.c parseinfo.c patch.c rcs.c rcscmds.c recurse.c \
	release.c remove.c repos.c root.c rtag.c server.c status.c tag.c \
	update.c vers_ts.c wrapper.c

SRCS+=	subr.c error.c filesubr.c version.c myndbm.c fileattr.c watch.c run.c \
	hash.c edit.c mkmodules.c scramble.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../lib -I${CVSDIR}/src -I${CVSDIR}/lib -DHAVE_CONFIG_H

DPADD+=	${LIBCVS} ${LIBGNUREGEX} ${LIBMD} ${LIBCRYPT}
LDADD+=	-lcvs -lgnuregex -lmd -lcrypt

.include <bsd.prog.mk>
