PROG=	dpost
SRCS=	dpost.c \
	draw.c \
	color.c \
	pictures.c \
	ps_include.c \
	afm.c \
	makedev.c \
	glob.c \
	misc.c \
	request.c \
	getopt.c \
	asciitype.c \
	otf.c \
	../fontmap.c \
	version.c \
	bst.c
LIBADD=	m

CFLAGS+=	-I${DOCTOOLSDIR}/stuff/bst -I${DOCTOOLSDIR}/troff/troff.d/dpost.d-I${DOCTOOLSDIR}/troff/troff.d/dpost.d \
		-I${DOCTOOLSDIR}/troff/troff.d/ \
		-DMACDIR='"${MACDIR}"' \
		-DRELEASE='"${RELEASE}"' \
		-DHYPDIR='"${HYPDIR}"' \
		-DFNTDIR='"${FNTDIR}"' \
		-DTABDIR='"${TABDIR}"' \
		-DPSTDIR='"${PSTDIR}"' \

.include <bsd.prog.mk>

RELEASE!=	awk -F= '/^RELEASE/ { print $$2 }' ${DOCTOOLSDIR}/version.mk
.PATH:	${DOCTOOLSDIR}/stuff/bst
.PATH:	${DOCTOOLSDIR}/troff/troff.d/dpost.d
.PATH:	${DOCTOOLSDIR}/troff
