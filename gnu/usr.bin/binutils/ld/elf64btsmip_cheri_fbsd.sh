# $FreeBSD$
. ${srcdir}/emulparams/elf64btsmip.sh
. ${srcdir}/emulparams/elf_fbsd.sh
ELF_INTERPRETER_NAME=\"/libexec/ld-cheri-elf.so.1\"

GENERATE_PIE_SCRIPT=yes
