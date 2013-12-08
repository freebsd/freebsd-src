# $FreeBSD$

LIB=	capsicum

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib

SRCS=	libcapsicum.c
SRCS+=	libcapsicum_dns.c
SRCS+=	libcapsicum_grp.c
SRCS+=	libcapsicum_pwd.c
SRCS+=	libcapsicum_random.c
SRCS+=	libcapsicum_service.c
SRCS+=	libcapsicum_sysctl.c

INCS=	libcapsicum.h
INCS+=	libcapsicum_dns.h
INCS+=	libcapsicum_grp.h
INCS+=	libcapsicum_pwd.h
INCS+=	libcapsicum_random.h
INCS+=	libcapsicum_service.h
INCS+=	libcapsicum_sysctl.h

DPADD=	${LIBNV}
LDADD=	-lnv

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../libnv

WARNS?=	6

MAN+=	libcapsicum.3

MLINKS+=libcapsicum.3 cap_init.3
MLINKS+=libcapsicum.3 cap_wrap.3
MLINKS+=libcapsicum.3 cap_unwrap.3
MLINKS+=libcapsicum.3 cap_sock.3
MLINKS+=libcapsicum.3 cap_clone.3
MLINKS+=libcapsicum.3 cap_close.3
MLINKS+=libcapsicum.3 cap_limit_get.3
MLINKS+=libcapsicum.3 cap_limit_set.3
MLINKS+=libcapsicum.3 cap_send_nvlist.3
MLINKS+=libcapsicum.3 cap_recv_nvlist.3
MLINKS+=libcapsicum.3 cap_xfer_nvlist.3
MLINKS+=libcapsicum.3 cap_service_open.3

.include <bsd.lib.mk>
