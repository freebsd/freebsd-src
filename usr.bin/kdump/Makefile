#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <src.opts.mk>

.PATH: ${SRCTOP}/usr.bin/ktrace

PROG=		kdump
SRCS=		kdump.c subr.c
CFLAGS+=	-I${SRCTOP}/usr.bin/ktrace

LIBADD=		sysdecode
.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_grp
LIBADD+=	cap_pwd
CFLAGS+=-DWITH_CASPER
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
	    ${MACHINE_CPUARCH} == "i386"
SRCS+=		linux.c
.endif

CFLAGS.linux.c+= -I${SRCTOP}/sys

.include <bsd.prog.mk>
