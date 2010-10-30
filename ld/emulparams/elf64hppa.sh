. ${srcdir}/emulparams/hppa64linux.sh
OUTPUT_FORMAT="elf64-hppa"
LIB_PATH="=/usr/lib/pa20_64:=/opt/langtools/lib/pa20_64"
TEXT_START_ADDR=0x4000000000001000
DATA_ADDR=0x8000000000001000
TARGET_PAGE_SIZE=4096
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"

# The HP dynamic linker actually requires you set the start of text and
# data to some reasonable value.  Of course nobody knows what reasoanble
# really is, so we just use the same values that HP's linker uses.
SHLIB_TEXT_START_ADDR=0x4000000000001000
SHLIB_DATA_ADDR=0x8000000000001000

# The linker is required to define these two symbols.
EXECUTABLE_SYMBOLS='PROVIDE (__SYSTEM_ID = 0x214); PROVIDE (_FPU_STATUS = 0x0);'
# The PA64 ELF port needs two additional initializer sections and also wants
# a start/end symbol pair for the .init and .fini sections.
INIT_START='KEEP (*(.HP.init)); PROVIDE (__preinit_start = .); KEEP (*(.preinit)); PROVIDE (__preinit_end = .); PROVIDE (__init_start = .);'
INIT_END='PROVIDE (__init_end = .);'
FINI_START='PROVIDE (__fini_start = .);'
FINI_END='PROVIDE (__fini_end = .);'
