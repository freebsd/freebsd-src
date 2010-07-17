# $FreeBSD: src/gnu/usr.bin/binutils/ld/armelf_fbsd.sh,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $
. ${srcdir}/emulparams/armelf.sh
. ${srcdir}/emulparams/elf_fbsd.sh
MAXPAGESIZE=0x8000
GENERATE_PIE_SCRIPT=yes

unset STACK_ADDR
unset EMBEDDED
. ${srcdir}/emulparams/armelf.sh
. ${srcdir}/emulparams/elf_fbsd.sh
MAXPAGESIZE=0x8000
GENERATE_PIE_SCRIPT=yes

unset STACK_ADDR
unset EMBEDDED
