#	@(#)Makefile	8.1 (Berkeley) 6/4/93


.if	${MACHINE} == "tahoe"
SUBDIR=csu/tahoe.pcc
.elif	${MACHINE} == "vax"
SUBDIR=csu/vax.pcc
.else
SUBDIR=csu/${MACHINE}
.endif

# XXX MISSING:		libplot
SUBDIR+=        libc libcompat libcom_err libcurses libedit \
		libf2c libforms \
	libkvm libmd libmytinfo libncurses libpcap libresolv librpcsvc \
	libscsi libskey libss libtermcap libutil libxpg4 liby \
	libdisk

.if !exists(../secure) || defined(NOSECURE) || defined(NOCRYPT)
SUBDIR+= libcrypt
.else
SUBDIR+= ../secure/lib/libcrypt
.if defined(RELEASEDIR)
# releases do need both libraries
SUBDIR+= libcrypt
.endif
.endif

.if !exists(../secure) || defined(NOSECURE)
SUBDIR+= libtelnet
.else
.if defined(RELEASEDIR)
# releases do need both libraries
SUBDIR+= libtelnet
.endif
SUBDIR+= ../secure/lib/libtelnet
.endif

.if defined(WANT_CSRG_LIBM)
SUBDIR+= libm
.else
SUBDIR+= msun
.endif

.include <bsd.subdir.mk>
