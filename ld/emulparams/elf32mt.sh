MACHINE=
SCRIPT_NAME=elf
TEMPLATE_NAME=generic
EXTRA_EM_FILE=genelf
OUTPUT_FORMAT="elf32-mt"
# See also `include/elf/mt.h'
TEXT_START_ADDR=0x2000
ARCH=mt
ENTRY=_start
EMBEDDED=yes
ELFSIZE=32
MAXPAGESIZE=256
# This is like setting STACK_ADDR to 0x0073FFFF0, except that the setting can
# be overridden, e.g. --defsym _stack=0x0f00, and that we put an extra
# sentinal value at the bottom.
# N.B. We can't use PROVIDE to set the default value in a symbol because
# the address is needed to place the .stack section, which in turn is needed
# to hold the sentinel value(s).
test -z "$CREATE_SHLIB" && OTHER_SECTIONS="  .stack        ${RELOCATING-0}${RELOCATING+(DEFINED(__stack) ? __stack : 0x007FFFF0)} :
  {
    ${RELOCATING+__stack = .;}
    *(.stack)
    LONG(0xdeaddead)
  }"
# We do not need .stack for shared library.
test -n "$CREATE_SHLIB" && OTHER_SECTIONS=""
