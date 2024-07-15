.include <src.opts.mk>

PACKAGE=	runtime

PROG=	logger

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.if ${MK_CASPER} != "no"
LIBADD+=        casper
LIBADD+=        cap_syslog
CFLAGS+=-DWITH_CASPER
.endif

.include <bsd.prog.mk>
