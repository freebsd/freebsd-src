#	$Id$
#	@(#)Makefile	8.1 (Berkeley) 6/4/93

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-${BINFORMAT})
SUBDIR=csu/${MACHINE_ARCH}-${BINFORMAT}
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH})
SUBDIR=csu/${MACHINE_ARCH}
.endif

# XXX MISSING:		libplot
SUBDIR+=libalias libc libcalendar libcompat libcom_err libcurses \
	libedit libf2c libftpio libgnumalloc libipx libmd libmytinfo \
	libncurses libopie libpcap libresolv librpcsvc \
	libskey libss libtermcap libutil libxpg4 liby libz
	
.if !defined(NOLIBC_R)
SUBDIR+= libc_r
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=libdisk libkvm libscsi libvgl

.if !defined(NOTCL) && exists (${.CURDIR}/../contrib/tcl) && \
	exists(${.CURDIR}/../usr.bin/tclsh) && exists (${.CURDIR}/libtcl)
SUBDIR+=libtcl
.endif
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

.if defined(RELEASEDIR) || !exists(${.CURDIR}/../kerberosIV) || defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
# releases do need both libraries
SUBDIR+= libtelnet
.endif

.if defined(WANT_CSRG_LIBM)
SUBDIR+= libm
.else
SUBDIR+= msun
.endif

SUBDIR+= compat

.include <bsd.subdir.mk>
