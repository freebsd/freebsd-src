# Note: this parameter script is sourced by the other
# sh[l]elf(32|64).sh parameter scripts.
SCRIPT_NAME=elf
OUTPUT_FORMAT=${OUTPUT_FORMAT-"elf32-sh64"}
TEXT_START_ADDR=0x1000
MAXPAGESIZE=128
ARCH=sh
MACHINE=sh5
ALIGNMENT=8
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
EMBEDDED=yes

DATA_START_SYMBOLS='PROVIDE (___data = .);'

# If data is located right after .text (not explicitly specified),
# then we need to align it to an 8-byte boundary.
OTHER_READONLY_SECTIONS='
PROVIDE (___rodata = DEFINED (.rodata) ? .rodata : 0);
. = ALIGN (8);
'

# Make _edata and .bss aligned by smuggling in an alignment directive.
OTHER_GOT_SECTIONS='. = ALIGN (8);'

# These are for compatibility with the COFF toolchain.
ENTRY=start
CTOR_START='___ctors = .;'
CTOR_END='___ctors_end = .;'
DTOR_START='___dtors = .;'
DTOR_END='___dtors_end = .;'

# Do not use the varname=${varname-'string'} construct here; there are
# problems with that on some shells (e.g. on Solaris) where there is a bug
# that trigs when $varname contains a "}".
# The effect of the .stack definition is like setting STACK_ADDR to 0x80000,
# except that the setting can be overridden, e.g. --defsym _stack=0xff000,
# and that we put an extra sentinal value at the bottom.
# N.B. We can't use PROVIDE to set the default value in a symbol because
# the address is needed to place the .stack section, which in turn is needed
# to hold the sentinel value(s).
test -z "$CREATE_SHLIB" && OTHER_SECTIONS="
  .stack ${RELOCATING-0}${RELOCATING+(DEFINED(_stack) ? _stack : ALIGN (0x40000) + 0x40000)} :
  {
    ${RELOCATING+_stack = .;}
    *(.stack)
    LONG(0xdeaddead)
  }
  .cranges 0 : { *(.cranges) }
"
# We do not need .stack for shared library.
test -n "$CREATE_SHLIB" && OTHER_SECTIONS="
  .cranges 0 : { *(.cranges) }
"

# We need to adjust sizes in the .cranges section after relaxation, so
# we need an after_allocation function, and it goes in this file.
EXTRA_EM_FILE=${EXTRA_EM_FILE-sh64elf}
