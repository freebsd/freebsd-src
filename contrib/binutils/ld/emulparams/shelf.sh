# If you change this file, please also look at files which source this one:
# shlelf.sh, shelf_nbsd.sh

SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-sh"
TEXT_START_ADDR=0x1000
MAXPAGESIZE=128
ARCH=sh
MACHINE=
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
EMBEDDED=yes

# These are for compatibility with the COFF toolchain.
ENTRY=start
CTOR_START='___ctors = .;'
CTOR_END='___ctors_end = .;'
DTOR_START='___dtors = .;'
DTOR_END='___dtors_end = .;'
STACK_ADDR=0x30000
