#	$Id: Makefile,v 1.10 1995/03/31 07:55:08 nate Exp $

PROG=	cvs
MAN1=	cvs.1
MAN5=	cvs.5
SRCS =	add.c admin.c checkin.c checkout.c classify.c client.c commit.c \
	create_adm.c cvsrc.c diff.c entries.c expand_path.c find_names.c \
	history.c ignore.c import.c lock.c log.c login.c logmsg.c main.c \
	modules.c no_diff.c parseinfo.c patch.c rcs.c rcscmds.c recurse.c \
	release.c remove.c repos.c root.c rtag.c server.c status.c tag.c \
	update.c vers_ts.c wrapper.c

CFLAGS+= -I${.CURDIR}/../lib -DHAVE_CONFIG_H

DPADD+=	${LIBCVS} ${LIBGNUREGEX}
LDADD+=	-lcvs -lgnuregex

.include "../../Makefile.inc"
.include <bsd.prog.mk>

