PROG = mount_nandfs
SRCS = mount_nandfs.c getmntopts.c
MAN  = mount_nandfs.8

LDADD = -lutil
DPADD = ${LIBUTIL}

# mount_nandfs needs mntopts.h and getmntopts.c from src/sbin/mount/
MOUNT  ?= ${.CURDIR}/../mount
CFLAGS += -I${MOUNT}
WARNS ?= 6

.PATH: ${MOUNT}

.include <bsd.prog.mk>
