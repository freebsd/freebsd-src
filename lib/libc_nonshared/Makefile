# $FreeBSD$

# We're actually creating a libc_noshared.a that is PIC along side libc.so.*
# It is used exclusively with libc.so.* - there is no need for any other
# compile modes.
# bsd.lib.mk doesn't have an easy way to express that.
NO_PROFILE?=
.include <bsd.own.mk>
NO_PIC=	
# -fpic on some platforms, -fPIC on others.
CFLAGS+=${PICFLAG} -DPIC -fvisibility=hidden

LIB=	c_nonshared

# So that an empty .a file doesn't cause errors.
SRCS=	__stub.c

.if ${MK_ICONV} == "yes"
SRCS+=	__iconv.c __iconv_free_list.c __iconv_get_list.c \
	iconv.c iconv_canonicalize.c iconv_close.c \
	iconv_open.c iconv_open_into.c \
	iconv_set_relocation_prefix.c iconvctl.c iconvlist.c
CFLAGS+=-I${.CURDIR}/../libc/iconv
.endif

.include <bsd.lib.mk>

