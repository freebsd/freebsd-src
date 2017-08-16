# $FreeBSD$

.PATH: ${SRCTOP}/sys/amd64/sgx

KMOD=	sgx
SRCS=	sgx.c sgxvar.h assym.s sgx_support.S

sgx_support.o:  assym.s
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

.include <bsd.kmod.mk>
