#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# locale sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/locale ${.CURDIR}/locale

SRCS+=	big5.c btowc.c collate.c collcmp.c euc.c fix_grouping.c frune.c \
	gb18030.c gbk.c isctype.c iswctype.c \
	ldpart.c lmessages.c lmonetary.c lnumeric.c localeconv.c mblen.c \
	mbrlen.c \
	mbrtowc.c mbrune.c mbsinit.c mbsrtowcs.c mbtowc.c mbstowcs.c \
	mskanji.c nl_langinfo.c nomacros.c none.c rune.c \
	runetype.c setinvalidrune.c setlocale.c setrunelocale.c table.c \
	tolower.c toupper.c utf2.c utf8.c wcrtomb.c wcsrtombs.c wcsftime.c \
	wcstof.c wcstod.c \
	wcstoimax.c wcstol.c wcstold.c wcstoll.c \
	wcstombs.c \
	wcstoul.c wcstoull.c wcstoumax.c wctob.c wctomb.c wctrans.c wctype.c \
	wcwidth.c

MAN+=	btowc.3 \
	ctype.3 digittoint.3 isalnum.3 isalpha.3 isascii.3 isblank.3 iscntrl.3 \
	isdigit.3 isgraph.3 islower.3 isprint.3 ispunct.3 isspace.3 \
	isupper.3 iswalnum.3 isxdigit.3 mbrlen.3 mbrtowc.3 mbrune.3 mbsinit.3 \
	mbsrtowcs.3 multibyte.3 \
	nl_langinfo.3 \
	rune.3 \
	setlocale.3 toascii.3 tolower.3 toupper.3 towlower.3 towupper.3 \
	wcsftime.3 \
	wcrtomb.3 \
	wcsrtombs.3 wcstod.3 wcstol.3 \
	wctrans.3 wctype.3 wcwidth.3
MAN+=	euc.4 utf2.4
MAN+=	big5.5 gb18030.5 mskanji.5 utf8.5

MLINKS+=btowc.3 wctob.3
MLINKS+=isdigit.3 isnumber.3
MLINKS+=iswalnum.3 iswalpha.3 iswalnum.3 iswascii.3 iswalnum.3 iswblank.3 \
	iswalnum.3 iswcntrl.3 iswalnum.3 iswdigit.3 iswalnum.3 iswgraph.3 \
	iswalnum.3 iswhexnumber.3 iswalnum.3 iswhexnumber.3 \
	iswalnum.3 iswideogram.3 iswalnum.3 iswlower.3 iswalnum.3 iswnumber.3 \
	iswalnum.3 iswphonogram.3 iswalnum.3 iswprint.3 iswalnum.3 iswpunct.3 \
	iswalnum.3 iswrune.3 iswalnum.3 iswspace.3 iswalnum.3 iswspecial.3 \
	iswalnum.3 iswupper.3 iswalnum.3 iswxdigit.3
MLINKS+=isxdigit.3 ishexnumber.3
MLINKS+=mbrune.3 mbmb.3 mbrune.3 mbrrune.3
MLINKS+=multibyte.3 mblen.3 multibyte.3 mbstowcs.3 multibyte.3 mbtowc.3 \
	multibyte.3 wcstombs.3 multibyte.3 wctomb.3
MLINKS+=rune.3 fgetrune.3 rune.3 fputrune.3 rune.3 fungetrune.3 \
	rune.3 setinvalidrune.3 rune.3 setrunelocale.3 rune.3 sgetrune.3 \
	rune.3 sputrune.3
MLINKS+=setlocale.3 localeconv.3
MLINKS+=wcstod.3 wcstof.3 wcstod.3 wcstold.3
MLINKS+=wcstol.3 wcstoul.3 wcstol.3 wcstoll.3 wcstol.3 wcstoull.3 \
	wcstol.3 wcstoimax.3 wcstol.3 wcstoumax.3
MLINKS+=wctrans.3 towctrans.3
MLINKS+=wctype.3 iswctype.3
