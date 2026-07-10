CONFGROUPS=	CONFTTYS
PACKAGE=runtime
PROG=	init
MAN=	init.8
SRCS=	init.c
PRECIOUSPROG=
INSTALLFLAGS=-b -B.bak
CFLAGS.init.c+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
.ifdef RESCUE
CFLAGS.init.c+=-DRESCUE
.else
SRCS+=	reroot_seed.embed.s
ACFLAGS.reroot_seed.embed.s+=-I${OBJTOP}/sbin/reroot_seed
.endif
LIBADD=	util crypt

CONFTTYSNAME=	ttys
CONFTTYS+=	ttys

.include <bsd.prog.mk>
