
PACKAGE=quotacheck
PROG=	quotacheck
SRCS=	quotacheck.c preen.c fsutil.c utilities.c
WARNS?=	3
MAN=	quotacheck.8
LIBADD=	util ufs

.PATH:	${.CURDIR:H}/fsck ${.CURDIR:H}/fsck_ffs

.include <bsd.prog.mk>
