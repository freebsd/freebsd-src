PROG = cvs
CFLAGS +=	-I${.CURDIR}/../lib \
		-DDIRENT -DSTDC_HEADERS -DPOSIX -DBROKEN_SIGISMEMBER \
		-DFTIME_MISSING -DHAVE_TIMEZONE -DUTIME_NULL_MISSING -DDO_LINKS

LDADD=  -L${.CURDIR}/../lib/${.CURDIR} -lcvs

SRCS = add.c admin.c checkin.c checkout.c classify.c commit.c \
create_adm.c diff.c entries.c find_names.c history.c ignore.c \
import.c lock.c log.c logmsg.c main.c rcs.c modules.c \
no_diff.c parseinfo.c patch.c recurse.c release.c remove.c repos.c rtag.c \
status.c tag.c update.c vers_ts.c version.c

MAN1=	cvs.0
MAN5=	cvs.0

.include <bsd.prog.mk>
.include "../../Makefile.inc"
