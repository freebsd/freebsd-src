SCRIPT_NAME=i386lynx
OUTPUT_FORMAT="coff-i386-lynx"
# This is what LynxOS /lib/init1.o wants.
ENTRY=_main
# following are dubious
TARGET_PAGE_SIZE=0x1000
TEXT_START_ADDR=0
NONPAGED_TEXT_START_ADDR=0x1000
ARCH=i386
