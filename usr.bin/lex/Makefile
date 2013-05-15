# $FreeBSD$
#
# By default, flex will be configured to generate 8-bit scanners only if the
# -8 flag is given.  If you want it to always generate 8-bit scanners, add
# "-DDEFAULT_CSIZE=256" to CFLAGS.  Note that doing so will double the size
# of all uncompressed scanners.
#
# Bootstrapping of lex is handled automatically.
# Also note that flex.skel no longer gets installed.
#

PROG=		lex
LINKS+=	${BINDIR}/lex ${BINDIR}/lex++
LINKS+=	${BINDIR}/lex ${BINDIR}/flex
LINKS+=	${BINDIR}/lex ${BINDIR}/flex++

FLEXDIR=	${.CURDIR}/../../contrib/flex

.PATH:		${FLEXDIR}

SRCS=		buf.c ccl.c dfa.c ecs.c filter.c gen.c main.c misc.c \
		nfa.c options.c parse.y regex.c scan.c scanflags.c \
		scanopt.c skel.c sym.c tables.c tables_shared.c \
		tblcmp.c yylex.c
LFLAGS+=	-is
CFLAGS+=	-I. -I${.CURDIR} -I${FLEXDIR} -DHAVE_CONFIG_H
INCS=		FlexLexer.h
INCSDIR=	${INCLUDEDIR}
MLINKS+=	lex.1 flex.1
MLINKS+=	lex.1 flex++.1
MLINKS+=	lex.1 lex++.1

WARNS?=		3

CLEANFILES=	scan.c skel.c

SUBDIR=		lib

MAJOR_VERSION=		2
MINOR_VERSION=		5
SUBMINOR_VERSION=	37

skel.c: mkskel.sh flex.skl
	sed 's/m4_/m4postproc_/g; s/m4preproc_/m4_/g' \
	    ${FLEXDIR}/flex.skl | \
	m4 -I${FLEXDIR} -P \
	    -DFLEX_MAJOR_VERSION=${MAJOR_VERSION} \
	    -DFLEX_MINOR_VERSION=${MINOR_VERSION} \
	    -DFLEX_SUBMINOR_VERSION=${SUBMINOR_VERSION} | \
	sed 's/m4postproc_/m4_/g' | \
	sh ${FLEXDIR}/mkskel.sh > ${.TARGET}

bootstrap: scan.c skel.c
	cmp -s ${.CURDIR}/initscan.c scan.c || { \
		rm -f scan.c ; \
		cp -f ${.CURDIR}/initscan.c scan.c ; \
	}
	cmp -s ${.CURDIR}/initskel.c skel.c || { \
		rm -f skel.c ; \
		cp -f ${.CURDIR}/initskel.c skel.c ; \
	}

test: check
check: ${PROG}
	./${PROG} ${LFLAGS} -t ${COMPRESSION} ${FLEXDIR}/scan.l \
	| sed 's,\"${FLEXDIR}/scan.l\",\"scan.l\",' \
	| diff -I '\$$FreeBS[D]: .*\$$' ${.CURDIR}/initscan.c -
	@echo "Check successful"

.include <bsd.prog.mk>
