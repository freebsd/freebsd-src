#	From: @(#)Makefile.inc	5.1 (Berkeley) 2/18/91
#	$Id: Makefile.inc,v 1.5 1994/04/13 20:12:15 wollman Exp $

# locale sources
.PATH: ${.CURDIR}/${MACHINE}/locale ${.CURDIR}/locale

SRCS+=	ansi.c frune.c mbrune.c lconv.c localeconv.c rune.c setlocale.c \
	table.c isctype.c \
	euc.c none.c utf2.c
MAN3+=	locale/mbrune.3 locale/multibyte.3 locale/rune.3 locale/setlocale.3
MAN4+=	locale/euc.4 locale/utf2.4

MLINKS+=	mbrune.3 mbrrune.3 mbrune.3 mbmb.3
MLINKS+=	multibyte.3 mblen.3 multibyte.3 mbstowcs.3 \
		multibyte.3 mbtowc.3 multibyte.3 wcstombs.3 \
		multibyte.3 wctomb.3
MLINKS+=	rune.3 setrunelocale.3 rune.3 setinvalidrune.3 \
		rune.3 sgetrune.3 rune.3 sputrune.3
MLINKS+=	setlocale.3 localeconv.3
MLINKS+=	utf2.4 utf8.4
