SYM_MAPS+=${LIBC_SRCTOP}/stdlib/malloc/Symbol.map

.PATH: ${LIBC_SRCTOP}/stdlib/malloc

MISRCS+=	free_aligned_sized.c \
		free_sized.c

MAN+=		free_sized.3

MLINKS+=	free_sized.3 free_aligned_sized.3

.include "${LIBC_SRCTOP}/stdlib/malloc/${OPT_LIBC_MALLOC}/Makefile.inc"
