# $FreeBSD: src/usr.sbin/cron/Makefile.inc,v 1.2 1999/08/28 01:15:48 peter Exp $

.if exists(${.OBJDIR}/../lib)
LIBCRON=${.OBJDIR}/../lib/libcron.a
.else
LIBCRON=${.CURDIR}/../lib/libcron.a
.endif

.include "../Makefile.inc"
