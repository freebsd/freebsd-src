# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $FreeBSD$

.include <src.opts.mk>

PROG	 = passwd
BINOWN	 = root
BINMODE	 = 4555
LIBADD   = pam
PRECIOUSPROG=
.if ${MK_NIS} != "no"
LINKS	 = ${BINDIR}/passwd ${BINDIR}/yppasswd
MLINKS	 = passwd.1 yppasswd.1
.endif

.include <bsd.prog.mk>
