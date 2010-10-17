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
# This is like setting STACK_ADDR to 0x30000, except that the setting can
# be overridden, e.g. --defsym _stack=0x0f00, and that we put an extra
# sentinal value at the bottom.
# N.B. We can't use PROVIDE to set the default value in a symbol because
# the address is needed to place the .stack section, which in turn is needed
# to hold the sentinel value(s).
test -z "$CREATE_SHLIB" && OTHER_SECTIONS="  .stack        ${RELOCATING-0}${RELOCATING+(DEFINED(_stack) ? _stack : 0x30000)} :
  {
    ${RELOCATING+_stack = .;}
    *(.stack)
    LONG(0xdeaddead)
  }"
# We do not need .stack for shared library.
test -n "$CREATE_SHLIB" && OTHER_SECTIONS=""
