#	@(#)Makefile	8.1 (Berkeley) 6/4/93


.if	${MACHINE} == "tahoe"
SUBDIR=csu/tahoe.pcc
.elif	${MACHINE} == "vax"
SUBDIR=csu/vax.pcc
.else
SUBDIR=csu/${MACHINE}
.endif

# XXX MISSING:		libplot
SUBDIR+=libalias libc libcompat libcom_err libcurses libdisk libedit \
	libf2c libftpio libgnumalloc libipx \
	libkvm libmd libmytinfo libncurses libpcap libresolv librpcsvc \
	libscsi libskey libss libtcl libtermcap libutil libxpg4 liby libz
	

.if defined(WANT_LIBC_R)
SUBDIR+= libc_r
.endif

.if !exists(${.CURDIR}/../secure) || defined(NOSECURE) || defined(NOCRYPT)
SUBDIR+= libcrypt
.else
SUBDIR+= ../secure/lib/libcrypt
.if defined(RELEASEDIR)
# releases do need both libraries
SUBDIR+= libcrypt
.endif
.endif

.if !exists(${.CURDIR}/../eBones) || defined(NOSECURE)
SUBDIR+= libtelnet
.else
.if defined(RELEASEDIR)
# releases do need both libraries
SUBDIR+= libtelnet
.endif
SUBDIR+= ../eBones/lib/libtelnet
.endif

.if defined(WANT_CSRG_LIBM)
SUBDIR+= libm
.else
SUBDIR+= msun
.endif

SUBDIR+= compat

.include <bsd.subdir.mk>
