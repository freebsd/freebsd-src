# $Id: Makefile,v 1.3 1997/02/22 14:57:32 peter Exp $

SUBDIR=

# Note that compat21 is *not* for the 2.1.x branch!
.if defined(COMPAT1X) || defined(RELEASEDIR)
SUBDIR+=	compat1x
.endif
.if defined(COMPAT20) || defined(RELEASEDIR)
SUBDIR+=	compat20
.endif

.include <bsd.subdir.mk>
