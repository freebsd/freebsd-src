# $FreeBSD: src/gnu/usr.bin/binutils/ld/armelf_fbsd.sh,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $
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
