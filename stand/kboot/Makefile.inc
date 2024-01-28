# Bring in the mini-libc that we wrote

KBOOTSRC=${BOOTSRC}/kboot
CFLAGS+=-I${KBOOTSRC}/include
LIBKBOOT=${BOOTOBJ}/kboot/libkboot/libkboot.a

.include "../Makefile.inc"
