#	@(#)Makefile	8.1 (Berkeley) 5/31/93

SUBDIR=	fortune

.ifmake (clean) || (cleandir)
SUBDIR+=datfiles
.endif

.ifmake !(install)
SUBDIR+=strfile
.else
SUBDIR+=datfiles
.endif

.include <bsd.subdir.mk>
