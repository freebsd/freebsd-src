#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	vmstat
MAN=	vmstat.8
LIBADD=	devstat kvm memstat xo util

WARNS?=	6

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
