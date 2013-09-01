# $FreeBSD$

LIBEXECINFO=	${.CURDIR}/../../contrib/libexecinfo

LIB=		execinfo

.PATH:		${LIBEXECINFO}

INCS=		execinfo.h symtab.h unwind.h
SRCS=		backtrace.c symtab.c unwind.c

MAN=		backtrace.3

MLINKS+=	backtrace.3 backtrace_symbols.3
MLINKS+=	backtrace.3 backtrace_symbols_fmt.3
MLINKS+=	backtrace.3 backtrace_symbols_fd.3
MLINKS+=	backtrace.3 backtrace_symbols_fd_fmt.3

.include <bsd.lib.mk>
