#	@(#)Makefile	8.1 (Berkeley) 5/31/93

SUBDIR=	fortune

.ifmake (clean) || (cleandir) || (obj)
SUBDIR+=datfiles
.endif

.if !make(install) && !make(distribute)
SUBDIR+=strfile
.else
SUBDIR+=datfiles
.endif

.include <bsd.subdir.mk>
