#	$Id: Makefile,v 1.20 1998/01/26 04:21:52 peter Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/lib
.PATH:	${CVSDIR}/man

PROG=	cvs
MAN1=	cvs.1
MAN5=	cvs.5

SRCS=	add.c admin.c buffer.c checkin.c checkout.c classify.c client.c \
	commit.c create_adm.c cvsrc.c diff.c edit.c entries.c error.c \
	expand_path.c fileattr.c filesubr.c find_names.c \
	hardlink.c hash.c history.c \
	ignore.c import.c lock.c log.c login.c logmsg.c main.c mkmodules.c \
	modules.c myndbm.c no_diff.c parseinfo.c patch.c rcs.c rcscmds.c \
	recurse.c release.c remove.c repos.c root.c rtag.c run.c scramble.c \
	server.c status.c subr.c tag.c update.c vers_ts.c version.c watch.c \
	wrapper.c zlib.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../lib -I${CVSDIR}/src -I${CVSDIR}/lib -DHAVE_CONFIG_H

DPADD+=	${LIBCVS} ${LIBDIFF} ${LIBGNUREGEX} ${LIBMD} ${LIBCRYPT} ${LIBZ}
LDADD+=	${LIBCVS} ${LIBDIFF} -lgnuregex -lmd -lcrypt -lz

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DHAVE_KERBEROS -DHAVE_KRB_GET_ERR_TEXT
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
DISTRIBUTION=	krb
# XXX do we want to mess with this?  It's probably not really worth it for
# the public freebsd stuff, but others might want it.  It's an optional
# feature anyway, the -x switch is needed to activate it.
CFLAGS+= -DENCRYPTION
.endif

.include <bsd.prog.mk>
