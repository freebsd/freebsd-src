# $FreeBSD$

SHLIB_NAME?= libgost.so
SRCS=	e_gost_err.c gost2001.c gost2001_keyx.c gost89.c gost94_keyx.c \
	gost_ameth.c gost_asn1.c gost_crypt.c gost_ctl.c gost_eng.c \
	gost_keywrap.c gost_md.c gost_params.c gost_pmeth.c gost_sign.c \
	gosthash.c
.if defined(LIBCHERI) && defined(CHERI_LLD_INTEGRATED_CAPSIZEFIX)
# adding libcrypto is the correct fix but the build system doesn't build
# it in the right order. Use the allow undefined hack for now
#LIBADD=	crypto
LDFLAGS+=-Wl,-verbose, -Wl,-allow-undefined-cap-relocs
.endif

.include <bsd.lib.mk>
