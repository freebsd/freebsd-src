#	@(#)Makefile	8.1 (Berkeley) 6/4/93


.if	${MACHINE} == "tahoe"
SUBDIR=csu/tahoe.pcc
.elif	${MACHINE} == "vax"
SUBDIR=csu/vax.pcc
.else
SUBDIR=csu/${MACHINE}
.endif

# XXX MISSING:		libmp libplot
SUBDIR+=	libc libcompat libcom_err libcrypt libcurses libedit \
		libdisk libf2c libforms \
	libkvm libmd libmytinfo libncurses libpcap libresolv librpcsvc \
	libscsi libskey libss libtelnet libtermcap libutil liby

.if defined(WANT_CSRG_LIBM)
SUBDIR+= libm
.else
SUBDIR+= msun
.endif

.include <bsd.subdir.mk>
