#	@(#)Makefile	5.25.1.1 (Berkeley) 5/7/91

SUBDIR=	csu.${MACHINE} libc libcurses libm \
	libresolv librpc librpcsvc libtelnet libterm libutil liby

.if exists(libcrypt)
.if !defined(NOCRYPT)
SUBDIR+= libcrypt
.endif
.endif

.include <bsd.subdir.mk>
