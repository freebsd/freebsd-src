# $FreeBSD$

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

# we do not want to capture dependencies referring to the above
UPDATE_DEPENDFILE= no

.if !make(obj)
# When building host tools we should never pull in headers from the source sys
# directory to avoid any ABI issues that might cause the built binary to crash.
# The only exceptions to this are sys/cddl/compat for dtrace bootstrap tools and
# sys/crypto for libmd bootstrap.
.if !empty(CFLAGS:M*${SRCTOP}/sys*:N*${SRCTOP}/sys/cddl/compat*:N*${SRCTOP}/sys/crypto*)
.error Do not include $${SRCTOP}/sys when building bootstrap tools. \
    Copy the header to $${WORLDTMP}/legacy in tools/build/Makefile instead. \
    Error was caused by Makefile in ${.CURDIR}
.endif

# ${SRCTOP}/include should also never be used to avoid ABI issues
.if !empty(CFLAGS:M*${SRCTOP}/include*)
.error Do not include $${SRCTOP}/include when building bootstrap tools. \
    Copy the header to $${WORLDTMP}/legacy in tools/build/Makefile instead. \
    Error was caused by Makefile in ${.CURDIR}
.endif
.endif
