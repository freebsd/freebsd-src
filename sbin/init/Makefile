#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

FILESGROUPS=	FILESETC FILESETCEXEC FILESETCDEFAULTS
FILESETCDIR=	/etc
FILESETC=	network.subr rc rc.initdiskless rc.subr rc.shutdown
FILESETCMODE=	644
FILESETCEXEC=	netstart pccard_ether rc.resume rc.suspend
FILESETCEXECDIR=	/etc
FILESETCEXECMODE=	755
FILESETCDEFAULTSDIR=	/etc/defaults
FILESETCDEFAULTS=	rc.conf
PACKAGE=runtime
PROG=	init
SRCS=	init.c getmntopts.c
MAN=	init.8
PRECIOUSPROG=
INSTALLFLAGS=-b -B.bak
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
LIBADD=	util crypt

# Needed for getmntopts.c
MOUNT=	${SRCTOP}/sbin/mount
CFLAGS+=-I${MOUNT}
.PATH:	${MOUNT}

NO_SHARED?=	YES

SUBDIR+=	rc.d

.include <bsd.prog.mk>
