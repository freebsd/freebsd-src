# $FreeBSD$

PROG=	fstest
NO_MAN=

OSTYPE!= uname

.if ${OSTYPE} == "FreeBSD"
CFLAGS+= -D__OS_FreeBSD__
CFLAGS+= -DHAS_LCHMOD -DHAS_CHFLAGS -DHAS_LCHFLAGS -DHAS_FREEBSD_ACL

.elif ${OSTYPE} == "SunOS"
CFLAGS+= -D__OS_Solaris__
CFLAGS+= -DHAS_TRUNCATE64 -DHAS_STAT64

.elif ${OSTYPE} == "Darwin"
CFLAGS+= -D__OS_Darwin__
CFLAGS+= -DHAS_LCHMOD -DHAS_CHFLAGS -DHAS_LCHFLAGS

.elif ${OSTYPE} == "Linux"
CFLAGS+= -D__OS_Linux__
CFLAGS+= -D_GNU_SOURCE
.endif

.include <bsd.prog.mk>
