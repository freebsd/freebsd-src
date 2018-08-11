# $FreeBSD$

.include <src.opts.mk>

PROG=	newsyslog
MAN=	newsyslog.8 newsyslog.conf.5
SRCS=	newsyslog.c ptimes.c
LIBADD=	sbuf

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
