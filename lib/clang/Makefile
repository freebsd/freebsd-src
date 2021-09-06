# $FreeBSD$

.include <src.opts.mk>

# These have to be built in order.
SUBDIR=		libllvm
.if ${MK_CLANG} != "no"
SUBDIR+=	libclang
SUBDIR+=	headers
.endif
.if ${MK_LLDB} != "no"
SUBDIR+=	liblldb
.endif

.include <bsd.subdir.mk>
