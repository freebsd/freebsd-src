#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# locale sources
.PATH: ${.CURDIR}/${MACHINE}/locale ${.CURDIR}/locale

SRCS+=	ansi.c ctype.c euc.c frune.c isctype.c lconv.c localeconv.c \
	mbrune.c none.c rune.c common_setlocale.c \
	startup_setlocale.c read_runemagi.c setlocale.c table.c utf2.c \
	runetype.c tolower.c toupper.c nomacros.c collate.c

MAN3+=	locale/ctype.3 locale/isalnum.3 locale/isalpha.3 locale/isascii.3 \
	locale/isblank.3 locale/iscntrl.3 locale/isdigit.3 locale/isgraph.3 \
	locale/islower.3 locale/isprint.3 locale/ispunct.3 locale/isspace.3 \
	locale/isupper.3 locale/isxdigit.3 locale/mbrune.3 locale/multibyte.3 \
	locale/rune.3 locale/setlocale.3 locale/toascii.3 locale/tolower.3 \
	locale/toupper.3
MAN4+=	locale/euc.4 locale/utf2.4
