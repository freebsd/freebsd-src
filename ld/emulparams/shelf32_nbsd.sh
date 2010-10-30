# If you change this file, please alsolook at files which source this one:
# shlelf32_nbsd.sh

. ${srcdir}/emulparams/shelf32.sh

OUTPUT_FORMAT="elf32-sh64-nbsd"
TEXT_START_ADDR=0x400000
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"

ENTRY=__start

unset EMBEDDED
unset STACK_ADDR
unset OTHER_SECTIONS
OTHER_SECTIONS="
  .cranges 0 : { *(.cranges) }
"
