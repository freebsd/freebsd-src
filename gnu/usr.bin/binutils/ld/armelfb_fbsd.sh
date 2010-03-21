# $FreeBSD: src/gnu/usr.bin/binutils/ld/armelfb_fbsd.sh,v 1.1.14.1 2010/02/10 00:26:20 kensmith Exp $
#XXX: This should be used once those bits are merged back in the FSF repo.
#. ${srcdir}/emulparams/armelf_fbsd.sh
#
#OUTPUT_FORMAT="elf32-bigarm"
. ${srcdir}/emulparams/armelf.sh
. ${srcdir}/emulparams/elf_fbsd.sh
MAXPAGESIZE=0x8000
GENERATE_PIE_SCRIPT=yes

unset STACK_ADDR
unset EMBEDDED
OUTPUT_FORMAT="elf32-bigarm"
#XXX: This should be used once those bits are merged back in the FSF repo.
#. ${srcdir}/emulparams/armelf_fbsd.sh
#
#OUTPUT_FORMAT="elf32-bigarm"
. ${srcdir}/emulparams/armelf.sh
. ${srcdir}/emulparams/elf_fbsd.sh
MAXPAGESIZE=0x8000
GENERATE_PIE_SCRIPT=yes

unset STACK_ADDR
unset EMBEDDED
OUTPUT_FORMAT="elf32-bigarm"
