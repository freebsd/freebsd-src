SUBDIR=	charlib

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
FILESGROUPS=	FILESPS FILESFONT
FILESPSDIR=	${PSTDIR}
FILESFONTDIR=	${FNTDIR}/devpost
FILESPS=	aps.ps \
		baseline.ps \
		color.ps \
		cutmarks.ps \
		dpost.ps \
		draw.ps \
		fatcourier.ps \
		forms.ps \
		ps_include.ps
FILESFONT=	B \
		AB.name \
		AI \
		AI.name \
		AR \
		AR.name \
		AX \
		AX.name \
		B \
		B.name \
		BI \
		BI.name \
		CB \
		CB.name \
		CI \
		CI.name \
		CO \
		CO.name \
		CW \
		CW.name \
		CX \
		CX.name \
		DESC \
		DESC.big \
		DESC.small \
		FONTMAP \
		GR \
		GR.name \
		H \
		H.name \
		HB \
		HB.name \
		HI \
		HI.name \
		HNB \
		HNB.name \
		HNBI \
		HNBI.name \
		HNI \
		HNI.name \
		HNR \
		HNR.name \
		HX \
		HX.name \
		I \
		I.name \
		KB \
		KB.name \
		KI \
		KI.name \
		KR \
		KR.name \
		KX \
		KX.name \
		NB \
		NB.name \
		NI \
		NI.name \
		NR \
		NR.name \
		NX \
		NX.name \
		PA \
		PA.name \
		PB \
		PB.name \
		PI \
		PI.name \
		PX \
		PX.name \
		R \
		R.name \
		S \
		S.big \
		S.name \
		S.small \
		S1 \
		S1.name \
		VB \
		VB.name \
		VI \
		VI.name \
		VR \
		VR.name \
		VX \
		VX.name \
		ZD \
		ZD.name \
		ZI \
		ZI.name \

SYMLINKS+=	H ${FNTDIR}/devpost/G \
		H ${FNTDIR}/devpost/HM \
		H ${FNTDIR}/devpost/HK \
		H ${FNTDIR}/devpost/HL \
		HI ${FNTDIR}/devpost/GI \

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
.PATH:	${DOCTOOLSDIR}/troff/troff.d/postscript
.PATH:	${DOCTOOLSDIR}/troff/troff.d/font/devpost
