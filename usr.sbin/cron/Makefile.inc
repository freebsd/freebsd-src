# $FreeBSD: src/usr.sbin/cron/Makefile.inc,v 1.3 2001/07/20 06:19:42 obrien Exp $

.if exists(${.OBJDIR}/../lib)
LIBCRON= ${.OBJDIR}/../lib/libcron.a
.else
LIBCRON= ${.CURDIR}/../lib/libcron.a
.endif

.include "../Makefile.inc"
