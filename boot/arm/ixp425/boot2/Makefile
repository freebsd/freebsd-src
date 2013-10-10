#  $FreeBSD$

.include <bsd.own.mk>

# We get a lot of the std lib functions from here.
.PATH: ${.CURDIR}/../../at91/libat91

# Enable to get debug msgs
#DEBUG=yes

# Hack to search through the kernel for ufs:ad0s1a and replace it with
# the correct one for the active slice/partition.
FIXUP_BOOT_DRV=yes

P=boot2
FILES=${P}
SRCS=arm_init.S boot2.c ${BOOT_FLAVOR:tl}_board.c
SRCS+=memchr.c memcmp.c memcpy.c memmem.c memset.c printf.c strcmp.c strcpy.c
SRCS+=strlen.c ashldi3.c divsi3.S muldi3.c
.if ${MK_ARM_EABI} != "no"
SRCS+=aeabi_unwind.c
.endif
NO_MAN=

KERNPHYSADDR=0x180000
KERNVIRTADDR=${KERNPHYSADDR}
BOOT_STACK=0x200000-4
M=${MACHINE}
LDFLAGS=-e ${KERNPHYSADDR} -EB -T ldscript.${M}
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}
S=${.CURDIR}/../../../..

CFLAGS+= \
	-DBOOT_STACK=${BOOT_STACK} \
	-I${.CURDIR}/../../../common \
	-I${.CURDIR}

.if defined(FIXUP_BOOT_DRV)
CFLAGS+=-DFIXUP_BOOT_DRV
.endif
.if defined(DEBUG)
CFLAGS+=-DDEBUG
.endif

ldscript.$M:	$S/conf/ldscript.$M
	cat $S/conf/ldscript.$M|sed s/KERNPHYSADDR/${KERNPHYSADDR}/g| \
	  sed s/KERNVIRTADDR/${KERNVIRTADDR}/g | \
	  sed s/" + SIZEOF_HEADERS"// > ldscript.$M

${P}:	ldscript.$M

CLEANFILES+=ldscript.$M

memchr.c:	$S/../lib/libc/string/memchr.c
	sed -e 's/string\.h/lib.h/' < $S/../lib/libc/string/memchr.c > \
	  ${.TARGET}

memmem.c:	$S/../lib/libc/string/memmem.c
	sed -e 's/string\.h/lib.h/' < $S/../lib/libc/string/memmem.c > \
	  ${.TARGET}

CLEANFILES+=memchr.c memmem.c

ashldi3.o:	$S/libkern/ashldi3.c
	${CC} -c ${CFLAGS} -D_KERNEL -o ${.TARGET} ${.IMPSRC}

divsi3.o:	$S/libkern/${M}/divsi3.S
	${CC} -c ${CFLAGS} -D_KERNEL -o ${.TARGET} ${.IMPSRC}

muldi3.o:	$S/libkern/${M}/muldi3.c
	${CC} -c ${CFLAGS} -D_KERNEL -o ${.TARGET} ${.IMPSRC}

inflate.c:	$S/kern/inflate.c
	sed -e 's/extern void putstr (char/extern void putstr (const char/' < \
	    $S/kern/inflate.c > ${.TARGET}

CLEANFILES+=inflate.c

.include <bsd.prog.mk>

