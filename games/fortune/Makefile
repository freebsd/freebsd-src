#	@(#)Makefile	8.1 (Berkeley) 5/31/93

SUBDIR= fortune

.if !make(install) && !make(distribute)
SUBDIR+=strfile
.endif

SUBDIR+= datfiles

.include <bsd.subdir.mk>
