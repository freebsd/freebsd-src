#
#
#
# PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
# --------------------         -----   ----------------------
# CURRENT PATCH LEVEL:         1       00037
# --------------------         -----   ----------------------
#
# 15 Sep 92	Adrian Hall		Fixes SRCS line
#

PROG=	crond
SRCS=	crond.c database.c user.c entry.c misc.c job.c do_command.c env.c
CFLAGS+=-I${.CURDIR} -DDEBUGGING=1 -DBSD -DCRONDIR='"/var/cron"' -fstrength-reduce
MAN8=	crond.8

.include <bsd.prog.mk>
