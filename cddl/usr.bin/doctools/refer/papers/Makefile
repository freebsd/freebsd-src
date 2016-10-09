
CLEANFILES=	runinv mkey inv Ind.ia Ind.ib Ind.ic
SCRIPTS=	runinv
BINDIR=		${LIBDIR}/reftools/papers
# We do not care about REFDIR for that
CFLAGS+=	-DREFDIR=''
MAN=

FILES=	Rbstjissue \
	Rv7man \
	Ind.ia \
	Ind.ib \
	Ind.ic

FILESDIR=	${LIBDIR}/reftools/papers

.SUFFIXES:	.sh
.sh:
	echo "#!/bin/sh" > ${.TARGET}
	sed 's:@REFDIR@:${REFDIR}:g' ${.IMPSRC} >> ${.TARGET}

build-tools: mkey inv

.include <bsd.prog.mk>

MKEYSRCS= \
	${DOCTOOLSDIR}/refer/mkey1.c \
	${DOCTOOLSDIR}/refer/mkey2.c \
	${DOCTOOLSDIR}/refer/mkey3.c \
	${DOCTOOLSDIR}/refer/deliv2.c

INVSRCS=	\
	${DOCTOOLSDIR}/refer/inv1.c \
	${DOCTOOLSDIR}/refer/inv2.c \
	${DOCTOOLSDIR}/refer/inv3.c \
	${DOCTOOLSDIR}/refer/inv5.c \
	${DOCTOOLSDIR}/refer/inv6.c \
	${DOCTOOLSDIR}/refer/deliv2.c

mkey: ${MKEYSRCS}
	${CC} -o ${.TARGET} ${CFLAGS} ${MKEYSRCS}

inv: ${INVSRCS}
	${CC} -o ${.TARGET} ${CFLAGS} ${.IMPSRC} ${INVSRCS}

Ind.ia Ind.ib Ind.ic: Rbstjissue Rv7man mkey inv
	./mkey ${DOCTOOLSDIR}/refer/papers/R* | ./inv -v -h997 -n Xind
	mv Xind.ia Ind.ia
	mv Xind.ib Ind.ib
	mv Xind.ic Ind.ic

.PATH:	${DOCTOOLSDIR}/refer/papers
