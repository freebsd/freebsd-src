# $FreeBSD$

.if !defined(SVNDIR)

SVNDIR=		${.CURDIR}/../../../../contrib/subversion/subversion
APRU=		${.CURDIR}/../../../../contrib/apr-util
APR=		${.CURDIR}/../../../../contrib/apr
SQLITE=		${.CURDIR}/../../../../contrib/sqlite3
SERF=		${.CURDIR}/../../../../contrib/serf

WARNS?=		0	# definitely not ready

.if exists(${.CURDIR}/../../../Makefile.inc)
.include "${.CURDIR}/../../../Makefile.inc"
.endif

.endif
