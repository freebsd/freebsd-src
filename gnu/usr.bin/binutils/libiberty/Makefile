# $FreeBSD$

.include "../Makefile.inc0"

# Get __FreeBSD_version
.if exists(/sbin/sysctl)
SYSCTL=	/sbin/sysctl
.else
SYSCTL=	/usr/sbin/sysctl
.endif
OSVERSION!=	${SYSCTL} -n kern.osreldate

.PATH: ${SRCDIR}/libiberty

LIB=	iberty
SRCS=	argv.c choose-temp.c concat.c cp-demangle.c cplus-dem.c \
	dyn-string.c getopt.c getopt1.c getpwd.c getruntime.c \
	hex.c floatformat.c lbasename.c objalloc.c obstack.c safe-ctype.c \
	xatexit.c xexit.c xmalloc.c \
	xstrdup.c xstrerror.c
.if ${OSVERSION} < 420000
SRCS+=	basename.c
.endif
CFLAGS+=	-DHAVE_CONFIG_H
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
