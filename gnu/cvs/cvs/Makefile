PROG = cvs
CFLAGS +=	-I${.CURDIR}/../lib \
		-DDIRENT -DSTDC_HEADERS -DPOSIX -DBROKEN_SIGISMEMBER \
		-DFTIME_MISSING -DHAVE_TIMEZONE -DUTIME_NULL_MISSING

.if exists(${.CURDIR}/../lib/obj)
LDADD=  -L${.CURDIR}/../lib/obj -lcvs
.else
LDADD=  -L${.CURDIR}/../lib/ -lcvs
.endif

SRCS = add.c admin.c checkin.c checkout.c classify.c commit.c \
create_adm.c diff.c entries.c find_names.c history.c ignore.c \
import.c lock.c log.c logmsg.c main.c rcs.c modules.c \
no_diff.c parseinfo.c patch.c recurse.c release.c remove.c repos.c rtag.c \
status.c tag.c update.c vers_ts.c version.c

MAN1=	cvs.1
MAN5=	cvs.5

check:
	@echo `pwd` ${.CURDIR}

.include <bsd.prog.mk>
.include "../../Makefile.inc"
