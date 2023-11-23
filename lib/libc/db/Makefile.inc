CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${LIBC_SRCTOP}/db/btree/Makefile.inc"
.include "${LIBC_SRCTOP}/db/db/Makefile.inc"
.include "${LIBC_SRCTOP}/db/hash/Makefile.inc"
.include "${LIBC_SRCTOP}/db/man/Makefile.inc"
.include "${LIBC_SRCTOP}/db/mpool/Makefile.inc"
.include "${LIBC_SRCTOP}/db/recno/Makefile.inc"

SYM_MAPS+=${LIBC_SRCTOP}/db/Symbol.map
