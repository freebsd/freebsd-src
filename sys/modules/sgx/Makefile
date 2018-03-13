# $FreeBSD$

.PATH: ${SRCTOP}/sys/amd64/sgx

KMOD=	sgx
SRCS=	sgx.c sgxvar.h assym.s sgx_support.S
SRCS+=	opt_compat.h opt_hwpmc_hooks.h opt_kstack_pages.h

sgx_support.o:  assym.s
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

.include <bsd.kmod.mk>
