#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# locale sources
.PATH: ${.CURDIR}/${MACHINE}/locale ${.CURDIR}/locale

SRCS+=	ansi.c ctype.c euc.c frune.c isctype.c lconv.c localeconv.c \
	mbrune.c none.c rune.c setlocale.c table.c utf2.c

MAN3+=	ctype.0 isalnum.0 isalpha.0 isascii.0 isblank.0 iscntrl.0 \
	isdigit.0 isgraph.0 islower.0 isprint.0 ispunct.0 isspace.0 \
	isupper.0 isxdigit.0 mbrune.0 multibyte.0 rune.0 setlocale.0 \
	toascii.0 tolower.0 toupper.0
MAN4+=	euc.0 utf2.0
