#	$Id: Makefile,v 9.0 2012/10/19 15:13:11 zy Exp $
# $FreeBSD$

.include <bsd.own.mk>

V=	${.CURDIR}/../../../contrib/nvi

.if ${MK_ICONV} == yes
UCAT=	dutch english french german polish spanish swedish
.else
# turn on WITH_ICONV=yes if you want UTF-8
UCAT=
.endif

CAT=	dutch english french german polish ru_RU.KOI8-R spanish swedish \
	uk_UA.KOI8-U zh_CN.GB2312

SCAN=	${V}/cl/*.c ${V}/common/*.c ${V}/ex/*.c ${V}/vi/*.c

.PATH: ${V}/catalog

all:		dump

build-tools:	dump

.for c in ${UCAT}
CAT+=	${c}.UTF-8
CLEANFILES+=${c}.UTF-8.base
$c.UTF-8.base:	${c}.base
.if ${c} == "polish"
	iconv -f ISO8859-2 -t UTF-8 ${.ALLSRC} > ${.TARGET}
.else
	iconv -f ISO8859-1 -t UTF-8 ${.ALLSRC} > ${.TARGET}
.endif
.endfor

CAT+=	zh_CN.UTF-8 uk_UA.UTF-8 ru_RU.UTF-8
CLEANFILES+=	zh_CN.UTF-8.base uk_UA.UTF-8.base ru_RU.UTF-8.base
zh_CN.UTF-8.base:	zh_CN.GB2312.base
	iconv -f GB2312 -t UTF-8 ${.ALLSRC} > ${.TARGET}

uk_UA.UTF-8.base:	uk_UA.KOI8-U.base
	iconv -f KOI8-U -t UTF-8 ${.ALLSRC} > ${.TARGET}

ru_RU.UTF-8.base:	ru_RU.KOI8-R.base
	iconv -f KOI8-R -t UTF-8 ${.ALLSRC} > ${.TARGET}

.for c in ${CAT}
${c}:	${c}.base 
	@echo "... $c";							\
	rm -f $c;							\
	sort -u ${.ALLSRC} |						\
	awk '{								\
		if ($$1 == 1) {						\
			print "\nMESSAGE NUMBER 1 IS NOT LEGAL";	\
			exit 1;						\
		}							\
		if (++nline > $$1) {					\
			print "DUPLICATE MESSAGE NUMBER " $$1;		\
			exit 1;						\
		}							\
		print $0;						\
	}' |								\
	sed -e '1s/^/$$set 1~$$quote "~/; 1y/~/\n/' |			\
	gencat $c /dev/stdin;						\
	chmod 444 $c;							\
	if grep DUPLICATE $c > /dev/null; then				\
		grep DUPLICATE $@;					\
	fi;								\
	if grep 'NOT LEGAL' $c > /dev/null; then			\
		grep 'NOT LEGAL' $@;					\
	fi
.endfor

CHK=	dutch.check english.check french.check german.check \
	polish.check ru_RU.KOI8-R.check spanish.check swedish.check \
	uk_UA.KOI8-U.check zh_CN.GB2312.check
check: ${CHK}
.for c in ${CAT}
${c}.check: ${c}.base
	@echo "... $c";							\
	f=${.ALLSRC:S;.base$;;};					\
	(echo "Unused message id's (this is okay):";			\
	awk '{								\
		while (++nline < $$1)					\
			printf "%03d\n", nline;				\
	}' < $$f.base;							\
	echo =========================;					\
	echo "MISSING ERROR MESSAGES (Please add!):";			\
	awk '{print $$1}' < $$f.base | sort -u > __ck1;			\
	awk '{print $$1}' < english.base | sort -u > __ck2;		\
	comm -13 __ck1 __ck2;						\
	echo =========================;					\
	echo "Extra error messages (just delete them):";		\
	comm -23 __ck1 __ck2;						\
	echo =========================;					\
	echo "MESSAGES WITH THE SAME MESSAGE ID's (FIX!):";		\
	for j in							\
	    `sed '/^$$/d' < $$f.base | LANG=C sort -u |			\
	    awk '{print $$1}' | uniq -d`; do				\
		egrep $$j $$f.base;					\
	done;								\
	echo =========================;					\
	echo "Duplicate messages, both id and message (this is okay):";	\
	sed '/^$$/d' < $$f.base | LANG=C sort | uniq -c |			\
	awk '$$1 != 1 { print $$0 }' | sort -n;				\
	echo =========================) > $c
.endfor

english.base: dump ${SCAN} #Makefile
	./dump ${SCAN} |\
	sed -e '/|/!d' \
	    -e 's/|/ "/' \
	    -e 's/^"//' |\
	sort -nu > $@


dump: dump.c
	${CC} -o dump ${.ALLSRC}

CLEANFILES+= dump ${CAT} english.base *.check __ck1 __ck2

CATALOGS= ${CAT}
NLLINKS= nl_NL
ENLINKS= en_AU en_CA en_GB en_NZ en_US
FRLINKS= fr_BE fr_CA fr_CH fr_FR
DELINKS= de_AT de_CH de_DE
ESLINKS= es_ES
SVLINKS= sv_SE
PLLINKS= pl_PL

FILES=	${CATALOGS}
FILESDIR= /usr/share/vi/catalog
SYMLINKS=
.for l in ${NLLINKS}
SYMLINKS+= dutch ${FILESDIR}/$l.ISO8859-1
SYMLINKS+= dutch ${FILESDIR}/$l.ISO8859-15
.endfor
.for l in ${ENLINKS}
SYMLINKS+= english ${FILESDIR}/$l.ISO8859-1
SYMLINKS+= english ${FILESDIR}/$l.ISO8859-15
SYMLINKS+= english ${FILESDIR}/$l.US-ASCII
.endfor
SYMLINKS+= english ${FILESDIR}/POSIX
SYMLINKS+= english ${FILESDIR}/C
.for l in ${FRLINKS}
SYMLINKS+= french ${FILESDIR}/$l.ISO8859-1
SYMLINKS+= french ${FILESDIR}/$l.ISO8859-15
.endfor
.for l in ${DELINKS}
SYMLINKS+= german ${FILESDIR}/$l.ISO8859-1
SYMLINKS+= german ${FILESDIR}/$l.ISO8859-15
.endfor
.for l in ${ESLINKS}
SYMLINKS+= spanish ${FILESDIR}/$l.ISO8859-1
SYMLINKS+= spanish ${FILESDIR}/$l.ISO8859-15
.endfor
.for l in ${SVLINKS}
SYMLINKS+= swedish ${FILESDIR}/$l.ISO8859-1
SYMLINKS+= swedish ${FILESDIR}/$l.ISO8859-15
.endfor
.for l in ${PLLINKS}
SYMLINKS+= polish ${FILESDIR}/$l.ISO8859-2
.endfor
# iconv not required for these.
SYMLINKS+= ru_RU.KOI8-R ${FILESDIR}/ru_SU.KOI8-R
SYMLINKS+= zh_CN.GB2312 ${FILESDIR}/zh_CN.GB18030
SYMLINKS+= zh_CN.GB2312 ${FILESDIR}/zh_CN.GBK
SYMLINKS+= zh_CN.GB2312 ${FILESDIR}/zh_CN.eucCN
.if ${MK_ICONV} == yes
.for l in ${NLLINKS}
SYMLINKS+= dutch.UTF-8 ${FILESDIR}/$l.UTF-8
.endfor
.for l in ${ENLINKS}
SYMLINKS+= english.UTF-8 ${FILESDIR}/$l.UTF-8
.endfor
.for l in ${FRLINKS}
SYMLINKS+= french.UTF-8 ${FILESDIR}/$l.UTF-8
.endfor
.for l in ${DELINKS}
SYMLINKS+= german.UTF-8 ${FILESDIR}/$l.UTF-8
.endfor
.for l in ${ESLINKS}
SYMLINKS+= spanish.UTF-8 ${FILESDIR}/$l.UTF-8
.endfor
.for l in ${SVLINKS}
SYMLINKS+= swedish.UTF-8 ${FILESDIR}/$l.UTF-8
.endfor
.for l in ${PLLINKS}
SYMLINKS+= polish.UTF-8 ${FILESDIR}/$l.UTF-8
.endfor
.endif

.include <bsd.prog.mk>
