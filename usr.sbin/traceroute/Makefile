
.include <src.opts.mk>

PACKAGE=	runtime
PROG=	traceroute
MAN=	traceroute.8
SRCS=	address.c as.c ip.c traceroute.c traceroute4.c	\
	traceroute6.c options.c pkt_make.c resolve.c util.c
BINOWN=	root
BINMODE=4555

.if !defined(TRACEROUTE_NO_IPSEC)
CFLAGS+= -DIPSEC
.endif

.if !defined(TRACEROUTE_NO_IPSEC)
LIBADD+=	ipsec
.endif

LIBADD+=	casper cap_net

WARNS?=	3

# For development.
CFLAGS+=-W -Wall -Werror -Wno-cast-align -fsanitize=address,undefined
LDFLAGS+=-fsanitize=address,undefined

tidy:
.for src in ${SRCS}
	clang-tidy ${.CURDIR}/${src} -- /usr/local/bin/clang ${CFLAGS} -c ${.CURDIR}/${src}
.endfor

.PHONY: tidy

.include <bsd.prog.mk>
