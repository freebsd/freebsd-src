#	@(#)Makefile	8.1 (Berkeley) 6/4/93

# XXX MISSING:		libmp libplot
SUBDIR=	libc libcompat libcrypt libcurses libedit libkvm libmd \
	libresolv librpcsvc libskey libtelnet libterm libutil liby

.if	${MACHINE} == "tahoe"
SUBDIR+=csu/tahoe.pcc
.elif	${MACHINE} == "vax"
SUBDIR+=csu/vax.pcc
.else
SUBDIR+=csu/${MACHINE}
.endif

.if !defined(WANT_MSUN)
SUBDIR+= libm
.else
SUBDIR+= msun
.endif

.include <bsd.subdir.mk>
