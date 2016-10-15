DOCTOOLSDIR=	${SRCTOP}/contrib/heirloom-doctools
CFLAGS+=	-I${DOCTOOLSDIR}/include -DHAVE_STRLCPY -DHAVE_STRLCAT -DHAVE_WCSLCPY
NO_WERROR=	yes
LIBDIR=	/usr/libexec/doctools
REFDIR=	${LIBDIR}/reftools
DTSHAREDIR=	/usr/share/doctools
FNTDIR=	${DTSHAREDIR}/font
HYPDIR=	${DTSHAREDIR}/hyphen
MACDIR=	${DTSHAREDIR}/tmac
PSTDIR=	${DTSHAREDIR}/devpost/postscript
PUBDIR=	${DTSHAREDIR}/pub
TABDIR=	${DTSHAREDIR}/nterm
.if defined(WORLDTMP)
WORLDTMPDIR=	${WORLDTMP}/legacy
.endif
.include "../Makefile.inc"
