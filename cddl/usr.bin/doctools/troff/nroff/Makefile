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

DPADD=	${.OBJDIR}/../libhnj/libhnj.a
LDADD=	${.OBJDIR}/../libhnj/libhnj.a

CFLAGS+=	-I${DOCTOOLSDIR}/stuff/bst -I${DOCTOOLSDIR}/troff/nroff.d \
		-I${DOCTOOLSDIR}/troff/ -DSHELL='"/bin/sh"' \
		-DMACDIR='"${MACDIR}"' \
		-DRELEASE='"${RELEASE}"' \
		-DHYPDIR='"${HYPDIR}"' \
		-DFNTDIR='"${FNTDIR}"' \
		-DTABDIR='"${TABDIR}"' \
		-DNROFF -DUSG -DEUC

.include <bsd.prog.mk>

RELEASE!=	awk -F= '/^RELEASE/ { print $$2 }' ${DOCTOOLSDIR}/version.mk
.PATH:	${DOCTOOLSDIR}/troff/nroff.d ${DOCTOOLSDIR}/stuff/bst ${DOCTOOLSDIR}/troff/
