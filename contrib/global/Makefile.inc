BINDIR?=	/usr/bin
MANDIR?=	/usr/share/man/man
.if exists(${.OBJDIR}/../lib)
LIBUTIL=${.OBJDIR}/../lib/libutil.a
.else
LIBUTIL=${.CURDIR}/../lib/libutil.a
.endif
