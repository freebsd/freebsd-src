#	@(#)Makefile	5.6 (Berkeley) 4/27/91

SUBDIR=	fortune

.ifmake (clean) || (cleandir) || (obj)
SUBDIR+=datfiles
.endif

.ifmake !(install)
SUBDIR+=strfile
.else
SUBDIR+=datfiles
.endif

.include <bsd.subdir.mk>
