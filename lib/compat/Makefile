# $FreeBSD$

SUBDIR=

# Note that compat21 is *not* for the 2.1.x branch!
.if defined(COMPAT1X) || defined(RELEASEDIR)
SUBDIR+=	compat1x
.endif
.if defined(COMPAT20) || defined(RELEASEDIR)
SUBDIR+=	compat20
.endif
.if defined(COMPAT21) || defined(RELEASEDIR)
SUBDIR+=	compat21
.endif
.if defined(COMPAT22) || defined(RELEASEDIR)
SUBDIR+=	compat22
.endif
.if defined(COMPAT3X) || defined(RELEASEDIR)
SUBDIR+=	compat3x.${MACHINE_ARCH}
.endif
.if defined(COMPAT4X) || defined(RELEASEDIR)
SUBDIR+=	compat4x.${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
