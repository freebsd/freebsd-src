#	@(#)Makefile	8.1 (Berkeley) 6/4/93


.if	${MACHINE} == "tahoe"
SUBDIR=csu/tahoe.pcc
.elif	${MACHINE} == "vax"
SUBDIR=csu/vax.pcc
.else
SUBDIR=csu/${MACHINE}
.endif

# XXX MISSING:		libplot
SUBDIR+=	libc libcompat libcom_err libcurses libedit \
		libdisk libf2c libftpio \
	libkvm libmd libmytinfo libncurses libpcap libresolv librpcsvc \
	libscsi libskey libss libtelnet libtermcap libutil liby

SUBDIR+= libcrypt
.if exists(../secure) && !defined(NOSECURE) && !defined(NOCRYPT)
SUBDIR+= ../secure/lib/libcrypt
.endif

.if defined(WANT_CSRG_LIBM)
SUBDIR+= libm
.else
SUBDIR+= msun
.endif

.include <bsd.subdir.mk>
