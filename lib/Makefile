#	@(#)Makefile	5.25.1.1 (Berkeley) 5/7/91

SUBDIR=	csu.${MACHINE} libc libcurses libm libpthread \
	libresolv librpcsvc libtelnet libterm libutil liby

.if exists(libcrypt)
.if !defined(NOCRYPT)
SUBDIR+= libcrypt
.endif
.endif

.if !defined(SPLIT_F2C)
SUBDIR+= libf2c
.else
SUBDIR+= libI77 libF77
.endif

.include <bsd.subdir.mk>
