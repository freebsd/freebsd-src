#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# locale sources
.PATH: ${.CURDIR}/../libc/${MACHINE_ARCH}/locale ${.CURDIR}/../libc/locale

SRCS+=	ansi.c big5.c btowc.c collate.c collcmp.c euc.c fix_grouping.c frune.c \
	isctype.c \
	ldpart.c lmessages.c lmonetary.c lnumeric.c localeconv.c mbrune.c \
	mskanji.c nl_langinfo.c nomacros.c none.c rune.c \
	runetype.c setinvalidrune.c setlocale.c setrunelocale.c table.c \
	tolower.c toupper.c utf2.c wctob.c wctrans.c wctype.c

.if ${LIB} == "c"
MAN+=	btowc.3 \
	ctype.3 digittoint.3 isalnum.3 isalpha.3 isascii.3 isblank.3 iscntrl.3 \
	isdigit.3 isgraph.3 islower.3 isprint.3 ispunct.3 isspace.3 \
	isupper.3 isxdigit.3 mbrune.3 multibyte.3 nl_langinfo.3 rune.3 \
	setlocale.3 toascii.3 tolower.3 toupper.3 wctrans.3 wctype.3
MAN+=	euc.4 utf2.4

MLINKS+=btowc.3 wctob.3
MLINKS+=isdigit.3 isnumber.3
MLINKS+=isxdigit.3 ishexnumber.3
MLINKS+=mbrune.3 mbmb.3 mbrune.3 mbrrune.3
MLINKS+=multibyte.3 mblen.3 multibyte.3 mbstowcs.3 multibyte.3 mbtowc.3 \
	multibyte.3 wcstombs.3 multibyte.3 wctomb.3
MLINKS+=rune.3 fgetrune.3 rune.3 fputrune.3 rune.3 fungetrune.3 \
	rune.3 setinvalidrune.3 rune.3 setrunelocale.3 rune.3 sgetrune.3 \
	rune.3 sputrune.3
MLINKS+=setlocale.3 localeconv.3
MLINKS+=wctrans.3 towctrans.3
MLINKS+=wctype.3 iswctype.3
.endif
