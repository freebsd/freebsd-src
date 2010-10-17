# If you change this file, please also look at files which source this one:
# mn10300.sh

SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-mn10200"
TEXT_START_ADDR=0x0
ARCH=mn10200
MACHINE=
MAXPAGESIZE=1
ENTRY=_start
EMBEDDED=yes

# This sets the stack to the top of the simulator memory (2^19 bytes).
STACK_ADDR=0x80000

# These are for compatibility with the COFF toolchain.
# XXX These should definitely disappear.
CTOR_START='___ctors = .;'
CTOR_END='___ctors_end = .;'
DTOR_START='___dtors = .;'
DTOR_END='___dtors_end = .;'
