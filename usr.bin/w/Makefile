PROG=	w
SRCS=	fmt.c pr_time.c proc_compare.c w.c
MAN=	w.1 uptime.1
LIBADD=	kvm sbuf xo util
LINKS=	${BINDIR}/w ${BINDIR}/uptime

.PATH: ${SRCTOP}/bin/ps

.include <bsd.prog.mk>
