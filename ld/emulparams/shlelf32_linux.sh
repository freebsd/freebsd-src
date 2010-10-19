# If you change this file, please also look at files which source this one:
# shelf32_linux.sh

SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-sh64-linux"
TEXT_START_ADDR=0x400000
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x1000
ARCH=sh
MACHINE=sh5
ALIGNMENT=8
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes


DATA_START_SYMBOLS='PROVIDE (___data = .);'

# If data is located right after .text (not explicitly specified),
# then we need to align it to an 8-byte boundary.
OTHER_READONLY_SECTIONS='
PROVIDE (___rodata = DEFINED (.rodata) ? .rodata : 0);
. = ALIGN (8);
'

# Make _edata and .bss aligned by smuggling in an alignment directive.
OTHER_GOT_SECTIONS='. = ALIGN (8);'

CTOR_START='___ctors = .;'
CTOR_END='___ctors_end = .;'
DTOR_START='___dtors = .;'
DTOR_END='___dtors_end = .;'

# Do not use the varname=${varname-'string'} construct here; there are
# problems with that on some shells (e.g. on Solaris) where there is a bug
# that trigs when $varname contains a "}".
test -z "$OTHER_RELOCATING_SECTIONS" && OTHER_RELOCATING_SECTIONS='
 .cranges 0 : { *(.cranges) }
'

# We need to adjust sizes in the .cranges section after relaxation, so
# we need an after_allocation function, and it goes in this file.
EXTRA_EM_FILE=${EXTRA_EM_FILE-sh64elf}
