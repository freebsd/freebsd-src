# $FreeBSD: src/gnu/usr.bin/rcs/lib/Makefile,v 1.9.20.1 2009/04/15 03:14:26 kensmith Exp $

# Define FSYNC_ALL to get slower but safer writes in case of crashes in
# the middle of CVS/RCS changes
#CFLAGS += -DFSYNC_ALL

LIB     = rcs
SRCS    = maketime.c partime.c rcsedit.c rcsfcmp.c rcsfnms.c rcsgen.c \
	  rcskeep.c rcskeys.c rcslex.c rcsmap.c rcsrev.c rcssyn.c rcstime.c \
	  rcsutil.c merger.c version.c

INTERNALLIB=

.include <bsd.lib.mk>
