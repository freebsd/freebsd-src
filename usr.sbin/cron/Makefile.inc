#	$Id: Makefile.inc,v 1.1 1998/03/08 00:19:19 bde Exp $

.if exists(${.OBJDIR}/../lib)
LIBCRON=${.OBJDIR}/../lib/libcron.a
.else
LIBCRON=${.CURDIR}/../lib/libcron.a
.endif

.include "../Makefile.inc"
