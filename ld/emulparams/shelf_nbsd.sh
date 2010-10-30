# If you change this file, please alsolook at files which source this one:
# shlelf_nbsd.sh

. ${srcdir}/emulparams/shelf.sh

OUTPUT_FORMAT="elf32-sh-nbsd"
TEXT_START_ADDR=0x400000
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"

DATA_START_SYMBOLS='__data_start = . ;';

ENTRY=_start

unset EMBEDDED
unset OTHER_SECTIONS
