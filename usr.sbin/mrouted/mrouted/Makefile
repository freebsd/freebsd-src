#	$FreeBSD$

PROG=	mrouted

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
LDADD+=	-lmrouted
.if exists(${.OBJDIR}/../common)
LDDESTDIR+=	-L${.OBJDIR}/../common
DPADD+= 	${.OBJDIR}/../common/libmrouted.a
.else
LDDESTDIR+=	-L$S/common
DPADD+=		$S/common/libmrouted.a
.endif

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c
CLEANFILES+= y.tab.h
MAN8=	${.CURDIR}/../mrouted.8

.include <bsd.prog.mk>
