#	@(#)Makefile	5.25.1.1 (Berkeley) 5/7/91

SUBDIR=	csu.${MACHINE} libc libcurses libmalloc libpthread \
	libresolv librpcsvc libskey libtelnet libterm libutil liby

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

.if !defined(WANT_MSUN)
SUBDIR+= libm
.else
SUBDIR+= msun
.endif

.include <bsd.subdir.mk>
