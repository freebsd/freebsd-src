#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# locale sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/locale ${.CURDIR}/../libc/locale

SRCS+=	ansi.c ctype.c euc.c frune.c isctype.c lconv.c localeconv.c \
	mbrune.c none.c rune.c setlocale.c table.c utf2.c setrunelocale.c \
	runetype.c tolower.c toupper.c nomacros.c collate.c setinvalidrune.c \
	collcmp.c
