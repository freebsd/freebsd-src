PROG=	ptx

FILES=	eign
FILESDIR=	${LIBDIR}

.if defined(WORLDTMP)
CFLAGS+=	-DLIBDIR='"${WORLDTMP}/legacy/${LIBDIR}"'
.else
CFLAGS+=	-DLIBDIR='"${LIBDIR}"'
.endif

.include <bsd.prog.mk>
.PATH:	${DOCTOOLSDIR}/ptx
