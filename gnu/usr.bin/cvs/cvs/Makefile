#	$Id: Makefile,v 1.25 1995/03/22 19:39:45 nate Exp $

PROG=	cvs
MAN1=	cvs.1
MAN5=	cvs.5
SRCS =	add.c admin.c checkin.c checkout.c classify.c commit.c create_adm.c  \
	cvsrc.c diff.c entries.c find_names.c history.c ignore.c import.c    \
	lock.c log.c logmsg.c main.c modules.c no_diff.c parseinfo.c patch.c \
	rcs.c recurse.c release.c remove.c repos.c root.c rtag.c status.c \
	tag.c update.c vers_ts.c

CFLAGS+= -I${.CURDIR}/../lib -DHAVE_CONFIG_H -DHAVE_TIMEZONE

DPADD+=	${LIBCVS} ${LIBGNUREGEX}
LDADD+=	-lcvs -lgnuregex

.include "../../Makefile.inc"
.include <bsd.prog.mk>
