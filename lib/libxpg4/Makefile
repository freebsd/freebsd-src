LIB=    xpg4
SRCS=   setlocale.c setrunelocale.c euc.c utf2.c
CFLAGS+= -Wall -DXPG4 -I${.CURDIR}/../libc/locale
.PATH: ${.CURDIR}/../libc/locale

NOMAN= YES

.include <bsd.lib.mk>
