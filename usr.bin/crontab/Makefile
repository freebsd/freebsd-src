#
# PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
# --------------------         -----   ----------------------
# CURRENT PATCH LEVEL:         1       00131
# --------------------         -----   ----------------------
#
# 06 Apr 93	Adam Glass	Fixed so manual pages install, install crontab
#				with proper permissions and owners
#
PROG=	crontab
SRCS=	crontab.c misc.c entry.c env.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../libexec/crond -DDEBUGGING=1 -DBSD -DCRONDIR='"/var/cron"' -fstrength-reduce
BINOWN =root
BINMODE=4111
MAN1=	crontab.1
MAN5=	crontab.5

.include <bsd.prog.mk>
.PATH: ${.CURDIR}/../../libexec/crond
