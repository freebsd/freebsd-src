# $FreeBSD: src/usr.sbin/cron/Makefile.inc,v 1.1.2.1 1999/08/29 15:40:32 peter Exp $

.if exists(${.OBJDIR}/../lib)
LIBCRON=${.OBJDIR}/../lib/libcron.a
.else
LIBCRON=${.CURDIR}/../lib/libcron.a
.endif

.include "../Makefile.inc"
