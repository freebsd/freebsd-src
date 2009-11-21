#	@(#)Makefile	8.2 (Berkeley) 4/22/94
# $FreeBSD: src/usr.sbin/nologin/Makefile,v 1.13.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	nologin
MAN=	nologin.5 nologin.8

SYMLINKS=	${BINDIR}/nologin /sbin/nologin

# It is important that nologin be statically linked for security
# reasons.  A dynamic non-setuid binary can be linked against a trojan
# libc by setting LD_LIBRARY_PATH appropriately.  Both sshd(8) and
# login(1) make it possible to log in with an unsanitized environment,
# rendering a dynamic nologin binary virtually useless.
NO_SHARED=	YES

.include <bsd.prog.mk>
