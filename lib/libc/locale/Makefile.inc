#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.8 1997/05/03 03:50:00 jb Exp $

# locale sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/locale ${.CURDIR}/../libc/locale

SRCS+=	ansi.c ctype.c euc.c frune.c isctype.c lconv.c localeconv.c \
	mbrune.c none.c rune.c setlocale.c table.c utf2.c setrunelocale.c \
	runetype.c tolower.c toupper.c nomacros.c collate.c setinvalidrune.c \
	collcmp.c

# Only build man pages with libc.
.if ${LIB} == "c"
MAN3+=	locale/ctype.3 locale/isalnum.3 locale/isalpha.3 locale/isascii.3 \
	locale/isblank.3 locale/iscntrl.3 locale/isdigit.3 locale/isgraph.3 \
	locale/islower.3 locale/isprint.3 locale/ispunct.3 locale/isspace.3 \
	locale/isupper.3 locale/isxdigit.3 locale/mbrune.3 locale/multibyte.3 \
	locale/rune.3 locale/setlocale.3 locale/toascii.3 locale/tolower.3 \
	locale/toupper.3
MAN4+=	locale/euc.4 locale/utf2.4
MLINKS+= rune.3 setrunelocale.3 rune.3 setinvalidrune.3 rune.3 sgetrune.3 \
	 rune.3 sputrune.3 rune.3 fgetrune.3 rune.3 fungetrune.3 \
	 rune.3 fputrune.3 mbrune.3 mbrrune.3 mbrune.3 mbmb.3 \
	 multibyte.3 mblen.3 multibyte.3 mbstowcs.3 multibyte.3 mbtowc.3 \
	 multibyte.3 wcstombs.3 multibyte.3 wctomb.3 setlocale.3 localeconv.3
.endif
