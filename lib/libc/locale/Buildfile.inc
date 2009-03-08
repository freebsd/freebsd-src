# $FreeBSD$

# locale sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/locale ${.CURDIR}/locale

SRCS += \
	ascii.c \
	big5.c \
	btowc.c \
	collate.c \
	collcmp.c \
	euc.c \
	fix_grouping.c \
	gb18030.c \
	gb2312.c \
	gbk.c \
	isctype.c \
	iswctype.c \
	ldpart.c \
	lmessages.c \
	lmonetary.c \
	lnumeric.c \
	localeconv.c \
	mblen.c \
	mbrlen.c \
	mbrtowc.c \
	mbsinit.c \
	mbsnrtowcs.c \
	mbsrtowcs.c \
	mbstowcs.c \
	mbtowc.c \
	mskanji.c \
	nextwctype.c \
	nl_langinfo.c \
	nomacros.c \
	none.c \
	rpmatch.c \
	rune.c \
	runetype.c \
	setlocale.c \
	setrunelocale.c \
	table.c \
	tolower.c \
	toupper.c \
	utf8.c \
	wcrtomb.c \
	wcsftime.c \
	wcsnrtombs.c \
	wcsrtombs.c \
	wcstod.c \
	wcstof.c \
	wcstoimax.c \
	wcstol.c \
	wcstold.c \
	wcstoll.c \
	wcstombs.c \
	wcstoul.c \
	wcstoull.c \
	wcstoumax.c \
	wctob.c \
	wctomb.c \
	wctrans.c \
	wctype.c \
	wcwidth.c

SYM_MAPS += ${.CURDIR}/locale/Symbol.map
