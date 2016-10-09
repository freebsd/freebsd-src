PROG=	nroff
SRCS=	n10.c \
		n6.c \
		hytab.c \
		n1.c \
		n2.c \
		n3.c \
		n4.c \
		n5.c \
		n7.c \
		n8.c \
		n9.c \
		ni.c \
		nii.c \
		suftab.c \
		draw.c \
		bst.c \
		version.c
		

FILESDIR=	${TABDIR}

TERMSFILES=	tab.2631 tab.2631-c tab.2631-e tab.lp \
		tab.300 tab.300-12 tab.300s tab.300s-12 tab.382 \
		tab.4000a tab.450 tab.450-12 tab.832 \
		tab.37 tab.8510 tab.X tab.tn300
FILES=		${TERMSFILES} tab.utf8
CLEANFILES=	${TERMSFILES}

DPADD=	${.OBJDIR}/../libhnj/libhnj.a
LDADD=	${.OBJDIR}/../libhnj/libhnj.a

CFLAGS+=	-I${DOCTOOLSDIR}/stuff/bst -I${DOCTOOLSDIR}/troff/nroff.d \
		-I${DOCTOOLSDIR}/troff/ -DSHELL='"/bin/sh"' \
		-DRELEASE='"${RELEASE}"' \
		-DNROFF -DUSG -DEUC
.if defined(WORLDTMP)
CFLAGS+=	-DMACDIR='"${WORLDTMP}/legacy/${MACDIR}"' \
		-DHYPDIR='"${WORLDTMP}/legacy/${HYPDIR}"' \
		-DFNTDIR='"${WORLDTMP}/legacy/${FNTDIR}"' \
		-DTABDIR='"${WORLDTMP}/legacy/${TABDIR}"'
.else
CFLAGS+=	-DMACDIR='"${MACDIR}"' \
		-DHYPDIR='"${HYPDIR}"' \
		-DFNTDIR='"${FNTDIR}"' \
		-DTABDIR='"${TABDIR}"'
.endif

NLTERM=		${DOCTOOLSDIR}/troff/nroff.d/terms.d

.for f in tab.2631 tab.2631-c tab.2631-e tab.lp
$f: a.${f:E} b.lp
	cat ${.ALLSRC} >${.TARGET}
.endfor

.for f in tab.300 tab.300-12 tab.300s tab.300s-12 tab.382 tab.4000a tab.450 tab.450-12 tab.832 
$f: a.${f:E} b.300
	cat ${.ALLSRC} >${.TARGET}
.endfor

.for f in tab.37 tab.8510 tab.X tab.tn300
$f: ab.${f:E}
	cat ${.ALLSRC} >${.TARGET}
.endfor

.include <bsd.prog.mk>

RELEASE!=	awk -F= '/^RELEASE/ { print $$2 }' ${DOCTOOLSDIR}/version.mk
.PATH:	${DOCTOOLSDIR}/troff/nroff.d ${DOCTOOLSDIR}/stuff/bst ${DOCTOOLSDIR}/troff/ ${NLTERM}
