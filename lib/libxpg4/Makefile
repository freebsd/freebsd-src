LIB=    xpg4
SRCS=   setlocale.c setrunelocale.c euc.c mskanji.c utf2.c runetype.c \
	tolower.c toupper.c
CFLAGS+= -Wall -DXPG4 -I${.CURDIR}/../libc/locale
.PATH: ${.CURDIR}/../libc/locale

#
# If there is a machine dependent libc makefile, use it:
#
.if exists(${.CURDIR}/../libc/${MACHINE_ARCH}/Makefile.inc)
.include "${.CURDIR}/../libc/${MACHINE_ARCH}/Makefile.inc"
.endif

NOMAN= YES

.include <bsd.lib.mk>
