# Bring in the mini-libc that we wrote

KBOOTSRC=${BOOTSRC}/kboot
CFLAGS+=-I${KBOOTSRC}/include -I${KBOOTSRC}/include/arch/${MACHINE_ARCH}
LIBKBOOT=${BOOTOBJ}/kboot/libkboot/libkboot.a

.include "../Makefile.inc"
