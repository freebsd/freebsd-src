#

.PATH: ${LIBC_SRCTOP}/csu
.include "${LIBC_SRCTOP}/csu/${LIBC_ARCH}/Makefile.inc"

SRCS+= \
	 ignore_init.c

CFLAGS+= -I${LIBC_SRCTOP}/csu/${LIBC_ARCH}
SYM_MAPS+=${LIBC_SRCTOP}/csu/Symbol.map
