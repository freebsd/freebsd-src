SUBDIR=	\
	dpost \
	tmac \
	devps \
	devpslow \
	devpsmed

PROGS=	troff \
	devaps \
	dhtml

SRCS.troff=	t10.c \
		t6.c \
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
		makedev.c \
		afm.c \
		otf.c \
		unimap.c \
		version.c \
		fontmap.c \
		bst.c
CFLAGS.troff= -DUSG -DEUC

DPADD.troff=	${.OBJDIR}/../libhnj/libhnj.a
LDADD.troff=	${.OBJDIR}/../libhnj/libhnj.a

SRCS.devaps=	daps.c \
		build.c \
		draw.c
LIBADD.devaps=	m
MAN.devaps=

SRCS.dhtml=	main.c \
		dhtml.l \
		tr_out.c \
		char.c \
		lib.c \
		bst.c

CFLAGS.dhtml+=	-I${DOCTOOLSDIR}/troff/troff.d/dhtml


CFLAGS+=	-I${DOCTOOLSDIR}/stuff/bst -I${DOCTOOLSDIR}/troff/troff.d \
		-I${DOCTOOLSDIR}/troff/ -DSHELL='"/bin/sh"' \
		-DMACDIR='"${MACDIR}"' \
		-DRELEASE='"${RELEASE}"' \
		-DHYPDIR='"${HYPDIR}"' \
		-DFNTDIR='"${FNTDIR}"' \
		-DTABDIR='"${TABDIR}"' \
		-DPSTDIR='"${PSTDIR}"' \

.include <bsd.progs.mk>

RELEASE!=	awk -F= '/^RELEASE/ { print $$2 }' ${DOCTOOLSDIR}/version.mk
.PATH:	${DOCTOOLSDIR}/troff/troff.d ${DOCTOOLSDIR}/stuff/bst ${DOCTOOLSDIR}/troff/
.PATH:	${DOCTOOLSDIR}/troff/troff.d/devaps
.PATH:	${DOCTOOLSDIR}/troff/troff.d/dhtml
